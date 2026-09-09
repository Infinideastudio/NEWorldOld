//! Out-of-game menu background — rotating sky cube with a Gaussian blur.
//!
//! Pipeline chain (run when `App` has no live `Game`):
//!
//! 1. Render the [`Atlases::background_cube`] cubemap as a slow-rotating
//!    skybox into intermediate `A` (`menu_background.wgsl`).
//! 2. Run [`shaders/filter.wgsl`] horizontal Gaussian on `A` → `B`.
//! 3. Run the same shader as a vertical Gaussian on `B` → the surface view.
//!
//! The blur intermediates are sized to the surface and recreated on
//! [`Self::resize`]; everything else is pipeline-creation-time stable.
//!
//! Bind-group shape (cube pipeline):
//! * group 0: cube uniforms (`view_proj` + `rotation`)
//! * group 1: cubemap + sampler
//!
//! Bind-group shape (filter pipeline):
//! * group 0: [`FilterUniforms`]
//! * group 1: input 2D texture + sampler
//!
//! Two filter uniform buffers are kept so the horizontal and vertical
//! passes can co-exist in one encoder without a buffer-write fence.

use bytemuck::{Pod, Zeroable};
use cgmath::{Deg, Matrix4, SquareMatrix, perspective};
use wgpu::util::DeviceExt;

use crate::client::game::camera::OPENGL_TO_WGPU_REVERSED;
use crate::client::render::uniforms::FilterUniforms;
use crate::textures::Atlases;

const SKY_SHADER_SRC: &str = include_str!("../../../shaders/menu_background.wgsl");
const FILTER_SHADER_SRC: &str = include_str!("../../../shaders/filter.wgsl");

/// Filter ids — must match `shaders/filter.wgsl` (`fs_main` branches on
/// these values; anything else is treated as a black clear).
const FILTER_ID_HORIZONTAL: i32 = 1;
const FILTER_ID_VERTICAL: i32 = 2;

/// Gaussian blur tuning. Same shape and scale the C++ build's menu-blur
/// path used: a wide, soft kernel that hides image detail without going
/// fully grey. `radius` is in target pixels; `step_size` strides the
/// inner loop.
const BLUR_RADIUS: f32 = 18.0;
const BLUR_STEP: f32 = 2.0;
const BLUR_SIGMA: f32 = 9.0;

/// Sky-cube uniform layout. One `mat4x4<f32>` (64 B) plus a `vec4<f32>`
/// (16 B) of scalar parameters — total 80 B, but we round up to 96 B
/// (next multiple of 16) for the WGSL uniform-address-space rule. The
/// rotation math itself lives in `menu_background.wgsl`; the host only
/// pushes the camera projection and `params.x = time_secs` so there is
/// no host-side `mat4x4` to marshal.
#[repr(C)]
#[derive(Clone, Copy, Debug, Pod, Zeroable)]
struct SkyUniforms {
    view_proj: [[f32; 4]; 4],
    params: [f32; 4],
}

/// Cube + filter pipelines plus the two intermediate render targets the
/// blur ping-pongs through.
pub struct MenuBackground {
    target_format: wgpu::TextureFormat,

    sky_pipeline: wgpu::RenderPipeline,
    sky_uniforms_bg: wgpu::BindGroup,
    sky_texture_bg: wgpu::BindGroup,
    sky_uniform_buffer: wgpu::Buffer,

    filter_pipeline: wgpu::RenderPipeline,
    filter_input_layout: wgpu::BindGroupLayout,
    filter_sampler: wgpu::Sampler,
    filter_h_uniforms_bg: wgpu::BindGroup,
    filter_v_uniforms_bg: wgpu::BindGroup,
    filter_h_buffer: wgpu::Buffer,
    filter_v_buffer: wgpu::Buffer,

    /// `inter_a` receives the cube render. `inter_b` receives the
    /// horizontal blur. Both are recreated on resize.
    inter_a: Intermediate,
    inter_b: Intermediate,

    width: u32,
    height: u32,
}

struct Intermediate {
    #[allow(dead_code)] // texture is held to keep the view alive
    texture: wgpu::Texture,
    view: wgpu::TextureView,
    sample_bg: wgpu::BindGroup,
}

impl MenuBackground {
    /// Build the pipelines and the initial pair of intermediate targets
    /// sized to `(width, height)`. `target_format` is the final write
    /// target — usually [`crate::render::Gfx::surface_format`]; the
    /// intermediates use the same format so one filter pipeline drives
    /// both passes.
    pub fn new(
        device: &wgpu::Device,
        atlases: &Atlases,
        target_format: wgpu::TextureFormat,
        width: u32,
        height: u32,
    ) -> Self {
        // ---------------- sky cube pipeline ----------------
        let sky_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("menu_background sky shader"),
            source: wgpu::ShaderSource::Wgsl(SKY_SHADER_SRC.into()),
        });

        let sky_uniforms_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("menu_background sky uniforms bgl"),
            entries: &[wgpu::BindGroupLayoutEntry {
                binding: 0,
                // Vertex stage uses `view_proj` + `params.x` (time);
                // fragment stage may also read `params` for diagnostics
                // / future shader-driven effects, so expose both.
                visibility: wgpu::ShaderStages::VERTEX | wgpu::ShaderStages::FRAGMENT,
                ty: wgpu::BindingType::Buffer {
                    ty: wgpu::BufferBindingType::Uniform,
                    has_dynamic_offset: false,
                    min_binding_size: None,
                },
                count: None,
            }],
        });

        let sky_texture_bgl = device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
            label: Some("menu_background sky texture bgl"),
            entries: &[
                wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Texture {
                        sample_type: wgpu::TextureSampleType::Float { filterable: true },
                        view_dimension: wgpu::TextureViewDimension::Cube,
                        multisampled: false,
                    },
                    count: None,
                },
                wgpu::BindGroupLayoutEntry {
                    binding: 1,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                    count: None,
                },
            ],
        });

        let sky_sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("menu_background sky sampler"),
            address_mode_u: wgpu::AddressMode::ClampToEdge,
            address_mode_v: wgpu::AddressMode::ClampToEdge,
            address_mode_w: wgpu::AddressMode::ClampToEdge,
            mag_filter: wgpu::FilterMode::Linear,
            min_filter: wgpu::FilterMode::Linear,
            mipmap_filter: wgpu::MipmapFilterMode::Nearest,
            ..Default::default()
        });

        let sky_uniform_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("menu_background sky uniforms"),
            contents: bytemuck::bytes_of(&SkyUniforms {
                view_proj: identity4(),
                params: [0.0; 4],
            }),
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        });

        let sky_uniforms_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("menu_background sky uniforms bg"),
            layout: &sky_uniforms_bgl,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: sky_uniform_buffer.as_entire_binding(),
            }],
        });

        let sky_texture_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("menu_background sky texture bg"),
            layout: &sky_texture_bgl,
            entries: &[
                wgpu::BindGroupEntry {
                    binding: 0,
                    resource: wgpu::BindingResource::TextureView(&atlases.background_cube.view),
                },
                wgpu::BindGroupEntry {
                    binding: 1,
                    resource: wgpu::BindingResource::Sampler(&sky_sampler),
                },
            ],
        });

        let sky_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("menu_background sky pipeline layout"),
            bind_group_layouts: &[Some(&sky_uniforms_bgl), Some(&sky_texture_bgl)],
            immediate_size: 0,
        });

        let sky_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("menu_background sky pipeline"),
            layout: Some(&sky_layout),
            vertex: wgpu::VertexState {
                module: &sky_shader,
                entry_point: Some("vs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                buffers: &[],
            },
            primitive: wgpu::PrimitiveState {
                topology: wgpu::PrimitiveTopology::TriangleList,
                strip_index_format: None,
                front_face: wgpu::FrontFace::Ccw,
                // Cull off so the inner cube faces (visible from the
                // origin) draw regardless of winding.
                cull_mode: None,
                unclipped_depth: false,
                polygon_mode: wgpu::PolygonMode::Fill,
                conservative: false,
            },
            depth_stencil: None,
            multisample: wgpu::MultisampleState::default(),
            fragment: Some(wgpu::FragmentState {
                module: &sky_shader,
                entry_point: Some("fs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                targets: &[Some(wgpu::ColorTargetState {
                    format: target_format,
                    blend: None,
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview_mask: None,
            cache: None,
        });

        // ---------------- filter pipeline ----------------
        let filter_shader = device.create_shader_module(wgpu::ShaderModuleDescriptor {
            label: Some("menu_background filter shader"),
            source: wgpu::ShaderSource::Wgsl(FILTER_SHADER_SRC.into()),
        });

        let filter_uniforms_bgl =
            device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
                label: Some("menu_background filter uniforms bgl"),
                entries: &[wgpu::BindGroupLayoutEntry {
                    binding: 0,
                    visibility: wgpu::ShaderStages::FRAGMENT,
                    ty: wgpu::BindingType::Buffer {
                        ty: wgpu::BufferBindingType::Uniform,
                        has_dynamic_offset: false,
                        min_binding_size: None,
                    },
                    count: None,
                }],
            });

        let filter_input_layout =
            device.create_bind_group_layout(&wgpu::BindGroupLayoutDescriptor {
                label: Some("menu_background filter input bgl"),
                entries: &[
                    wgpu::BindGroupLayoutEntry {
                        binding: 0,
                        visibility: wgpu::ShaderStages::FRAGMENT,
                        ty: wgpu::BindingType::Texture {
                            sample_type: wgpu::TextureSampleType::Float { filterable: true },
                            view_dimension: wgpu::TextureViewDimension::D2,
                            multisampled: false,
                        },
                        count: None,
                    },
                    wgpu::BindGroupLayoutEntry {
                        binding: 1,
                        visibility: wgpu::ShaderStages::FRAGMENT,
                        ty: wgpu::BindingType::Sampler(wgpu::SamplerBindingType::Filtering),
                        count: None,
                    },
                ],
            });

        let filter_sampler = device.create_sampler(&wgpu::SamplerDescriptor {
            label: Some("menu_background filter sampler"),
            address_mode_u: wgpu::AddressMode::ClampToEdge,
            address_mode_v: wgpu::AddressMode::ClampToEdge,
            address_mode_w: wgpu::AddressMode::ClampToEdge,
            mag_filter: wgpu::FilterMode::Linear,
            min_filter: wgpu::FilterMode::Linear,
            mipmap_filter: wgpu::MipmapFilterMode::Nearest,
            ..Default::default()
        });

        let filter_h_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("menu_background filter horizontal uniforms"),
            contents: bytemuck::bytes_of(&filter_uniforms(width, height, FILTER_ID_HORIZONTAL)),
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        });
        let filter_v_buffer = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("menu_background filter vertical uniforms"),
            contents: bytemuck::bytes_of(&filter_uniforms(width, height, FILTER_ID_VERTICAL)),
            usage: wgpu::BufferUsages::UNIFORM | wgpu::BufferUsages::COPY_DST,
        });

        let filter_h_uniforms_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("menu_background filter h uniforms bg"),
            layout: &filter_uniforms_bgl,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: filter_h_buffer.as_entire_binding(),
            }],
        });
        let filter_v_uniforms_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
            label: Some("menu_background filter v uniforms bg"),
            layout: &filter_uniforms_bgl,
            entries: &[wgpu::BindGroupEntry {
                binding: 0,
                resource: filter_v_buffer.as_entire_binding(),
            }],
        });

        let filter_layout = device.create_pipeline_layout(&wgpu::PipelineLayoutDescriptor {
            label: Some("menu_background filter pipeline layout"),
            bind_group_layouts: &[Some(&filter_uniforms_bgl), Some(&filter_input_layout)],
            immediate_size: 0,
        });

        let filter_pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
            label: Some("menu_background filter pipeline"),
            layout: Some(&filter_layout),
            vertex: wgpu::VertexState {
                module: &filter_shader,
                entry_point: Some("vs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                buffers: &[],
            },
            primitive: wgpu::PrimitiveState::default(),
            depth_stencil: None,
            multisample: wgpu::MultisampleState::default(),
            fragment: Some(wgpu::FragmentState {
                module: &filter_shader,
                entry_point: Some("fs_main"),
                compilation_options: wgpu::PipelineCompilationOptions::default(),
                targets: &[Some(wgpu::ColorTargetState {
                    format: target_format,
                    blend: None,
                    write_mask: wgpu::ColorWrites::ALL,
                })],
            }),
            multiview_mask: None,
            cache: None,
        });

        let inter_a = create_intermediate(
            device,
            target_format,
            width,
            height,
            "menu_background.inter_a",
            &filter_input_layout,
            &filter_sampler,
        );
        let inter_b = create_intermediate(
            device,
            target_format,
            width,
            height,
            "menu_background.inter_b",
            &filter_input_layout,
            &filter_sampler,
        );

        Self {
            target_format,
            sky_pipeline,
            sky_uniforms_bg,
            sky_texture_bg,
            sky_uniform_buffer,
            filter_pipeline,
            filter_input_layout,
            filter_sampler,
            filter_h_uniforms_bg,
            filter_v_uniforms_bg,
            filter_h_buffer,
            filter_v_buffer,
            inter_a,
            inter_b,
            width,
            height,
        }
    }

    /// Recreate the intermediate textures + their input bind groups and
    /// re-upload the filter uniforms with the new buffer dimensions.
    /// Cheap when the size hasn't changed.
    pub fn resize(&mut self, device: &wgpu::Device, queue: &wgpu::Queue, width: u32, height: u32) {
        if width == self.width && height == self.height {
            return;
        }
        self.width = width;
        self.height = height;
        self.inter_a = create_intermediate(
            device,
            self.target_format,
            width,
            height,
            "menu_background.inter_a",
            &self.filter_input_layout,
            &self.filter_sampler,
        );
        self.inter_b = create_intermediate(
            device,
            self.target_format,
            width,
            height,
            "menu_background.inter_b",
            &self.filter_input_layout,
            &self.filter_sampler,
        );
        queue.write_buffer(
            &self.filter_h_buffer,
            0,
            bytemuck::bytes_of(&filter_uniforms(width, height, FILTER_ID_HORIZONTAL)),
        );
        queue.write_buffer(
            &self.filter_v_buffer,
            0,
            bytemuck::bytes_of(&filter_uniforms(width, height, FILTER_ID_VERTICAL)),
        );
    }

    /// Render the sky cube → horizontal blur → vertical blur chain into
    /// `surface_view`. Issues 3 render passes on `encoder`. `time_secs`
    /// drives the cube rotation; `(width, height)` is used for the
    /// projection's aspect ratio (the intermediate sizes were locked at
    /// the last [`Self::resize`]).
    pub fn render(
        &self,
        queue: &wgpu::Queue,
        encoder: &mut wgpu::CommandEncoder,
        surface_view: &wgpu::TextureView,
        width: u32,
        height: u32,
        time_secs: f32,
    ) {
        // Camera sits at origin looking down -Z. Use the same
        // `OPENGL_TO_WGPU_REVERSED * perspective` chain the world camera
        // uses so cgmath's [-1,1] NDC depth maps into wgpu's [0,1].
        let aspect = (width.max(1) as f32) / (height.max(1) as f32);
        let proj = OPENGL_TO_WGPU_REVERSED * perspective(Deg(70.0), aspect, 0.01, 10.0);
        let view = Matrix4::identity();
        let view_proj = proj * view;

        // Rotation math runs entirely in WGSL — see
        // `shaders/menu_background.wgsl`. We just push the wall-clock
        // seconds as `params.x`.
        let sky = SkyUniforms {
            view_proj: mat4_to_array(view_proj),
            params: [time_secs, 0.0, 0.0, 0.0],
        };
        queue.write_buffer(&self.sky_uniform_buffer, 0, bytemuck::bytes_of(&sky));

        // Pass 1: cube → A.
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("menu_background sky pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &self.inter_a.view,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color::BLACK),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
                multiview_mask: None,
            });
            pass.set_pipeline(&self.sky_pipeline);
            pass.set_bind_group(0, &self.sky_uniforms_bg, &[]);
            pass.set_bind_group(1, &self.sky_texture_bg, &[]);
            pass.draw(0..36, 0..1);
        }

        // Pass 2: horizontal blur A → B.
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("menu_background blur horizontal"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: &self.inter_b.view,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color::BLACK),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
                multiview_mask: None,
            });
            pass.set_pipeline(&self.filter_pipeline);
            pass.set_bind_group(0, &self.filter_h_uniforms_bg, &[]);
            pass.set_bind_group(1, &self.inter_a.sample_bg, &[]);
            pass.draw(0..6, 0..1);
        }

        // Pass 3: vertical blur B → surface.
        {
            let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("menu_background blur vertical"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: surface_view,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Clear(wgpu::Color::BLACK),
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
                multiview_mask: None,
            });
            pass.set_pipeline(&self.filter_pipeline);
            pass.set_bind_group(0, &self.filter_v_uniforms_bg, &[]);
            pass.set_bind_group(1, &self.inter_b.sample_bg, &[]);
            pass.draw(0..6, 0..1);
        }
    }
}

fn create_intermediate(
    device: &wgpu::Device,
    format: wgpu::TextureFormat,
    width: u32,
    height: u32,
    label: &str,
    input_layout: &wgpu::BindGroupLayout,
    sampler: &wgpu::Sampler,
) -> Intermediate {
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label: Some(label),
        size: wgpu::Extent3d {
            width: width.max(1),
            height: height.max(1),
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
        view_formats: &[],
    });
    let view = texture.create_view(&wgpu::TextureViewDescriptor {
        label: Some(label),
        ..Default::default()
    });
    let sample_bg = device.create_bind_group(&wgpu::BindGroupDescriptor {
        label: Some(label),
        layout: input_layout,
        entries: &[
            wgpu::BindGroupEntry {
                binding: 0,
                resource: wgpu::BindingResource::TextureView(&view),
            },
            wgpu::BindGroupEntry {
                binding: 1,
                resource: wgpu::BindingResource::Sampler(sampler),
            },
        ],
    });
    Intermediate {
        texture,
        view,
        sample_bg,
    }
}

fn filter_uniforms(width: u32, height: u32, filter_id: i32) -> FilterUniforms {
    let mut u = FilterUniforms::default();
    u.buffer_width = width.max(1) as f32;
    u.buffer_height = height.max(1) as f32;
    u.filter_id = filter_id;
    u.gaussian_blur_radius = BLUR_RADIUS;
    u.gaussian_blur_step_size = BLUR_STEP;
    u.gaussian_blur_sigma = BLUR_SIGMA;
    u
}

fn identity4() -> [[f32; 4]; 4] {
    [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
}

fn mat4_to_array(m: Matrix4<f32>) -> [[f32; 4]; 4] {
    [m.x.into(), m.y.into(), m.z.into(), m.w.into()]
}
