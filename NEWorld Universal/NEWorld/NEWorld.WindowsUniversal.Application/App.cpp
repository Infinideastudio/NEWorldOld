//
//该文件演示了如何在 Windows 应用商店应用中使用 ICoreWindow 初始化 EGL。
//

#include "pch.h"
#include "app.h"
#include "..\NEWorld.Shared\SimpleRenderer.h"

using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Microsoft::WRL;
using namespace Platform;

using namespace NEWorld;

// 帮助程序，用于将以与设备无关的像素 (DIP) 为单位的长度转换为以物理像素为单位的长度。
inline float ConvertDipsToPixels(float dips, float dpi)
{
    static const float dipsPerInch = 96.0f;
    return floor(dips * dpi / dipsPerInch + 0.5f); // 舍入到最接近的整数。
}

//IFrameworkViewSource 接口的实现，这对运行我们的应用程序是必要的。
ref class SimpleApplicationSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource
{
public:
    virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView()
    {
        return ref new App();
    }
};

//主函数为我们的应用程序创建 IFrameworkViewSource，然后运行应用程序。
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
    auto simpleApplicationSource = ref new SimpleApplicationSource();
    CoreApplication::Run(simpleApplicationSource);
    return 0;
}

App::App() :
    mWindowClosed(false),
    mWindowVisible(true),
    mEglDisplay(EGL_NO_DISPLAY),
    mEglContext(EGL_NO_CONTEXT),
    mEglSurface(EGL_NO_SURFACE)
{
}

// 创建 IFrameworkView 时调用的第一个方法。
void App::Initialize(CoreApplicationView^ applicationView)
{
    // 注册应用程序生命周期的事件处理程序。此示例包括 Activated，因此我们
    // 可激活 CoreWindow 并开始在窗口上渲染。
    applicationView->Activated += 
        ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

    //可在此处找到其他事件处理程序的逻辑。
    //可以在此处找到有关“挂起”和“恢复”事件处理程序的信息:
    // http://msdn.microsoft.com/zh-cn/library/windows/apps/xaml/hh994930.aspx
}

// 创建 (或重新创建) CoreWindow 对象时调用。
void App::SetWindow(CoreWindow^ window)
{
    window->VisibilityChanged +=
        ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);

    window->Closed += 
        ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);

    // CoreWindow 已创建，因此可以初始化 EGL 了。
    InitializeEGL(window);
}

//初始化场景资源
void App::Load(Platform::String^ entryPoint)
{
    RecreateRenderer();
}

void App::RecreateRenderer()
{
    if (!mCubeRenderer)
    {
        mCubeRenderer.reset(new SimpleRenderer());
    }
}

// 将在窗口处于活动状态后调用此方法。
void App::Run()
{
    while (!mWindowClosed)
    {
        if (mWindowVisible)
        {
            CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

			EGLint panelWidth = 0;
			EGLint panelHeight = 0;
			eglQuerySurface(mEglDisplay, mEglSurface, EGL_WIDTH, &panelWidth);
			eglQuerySurface(mEglDisplay, mEglSurface, EGL_HEIGHT, &panelHeight);

            //可在此处找到更新此场景的逻辑
			mCubeRenderer->UpdateWindowSize(panelWidth, panelHeight);
            mCubeRenderer->Draw();

            //对 eglSwapBuffers 的调用可能不成功 (例如，由于设备丢失造成的调用失败)
            //如果此调用失败，那么我们必须重新初始化 EGL 和 GL 资源。
            if (eglSwapBuffers(mEglDisplay, mEglSurface) != GL_TRUE)
            {
                mCubeRenderer.reset(nullptr);
                CleanupEGL();

                InitializeEGL(CoreWindow::GetForCurrentThread());
                RecreateRenderer();
            }
        }
        else
        {
            CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
        }
    }

    CleanupEGL();
}

// 终止事件不会导致 Uninitialize 被调用。如果在应用程序在前台运行时注销 IFrameworkView
// 类，则将调用该方法。
void App::Uninitialize()
{
}

// 应用程序生命周期事件处理程序。
void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
    // Run() 在 CoreWindow 激活后才会启动。
    CoreWindow::GetForCurrentThread()->Activate();
}

// 窗口事件处理程序。
void App::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
    mWindowVisible = args->Visible;
}

void App::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
    mWindowClosed = true;
}

void App::InitializeEGL(CoreWindow^ window)
{
    const EGLint configAttributes[] = 
    {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 8,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };

    const EGLint contextAttributes[] = 
    { 
        EGL_CONTEXT_CLIENT_VERSION, 2, 
        EGL_NONE
    };

    const EGLint surfaceAttributes[] =
    {
        // EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER 与 EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER (见上文) 属于同一个优化项。
        //如果您有与该项相关的编译问题，请更新 Visual Studio 模板。
        EGL_ANGLE_SURFACE_RENDER_TO_BACK_BUFFER, EGL_TRUE,
        EGL_NONE
    };

    const EGLint defaultDisplayAttributes[] =
    {
        //这些是默认的显示属性，用于请求 ANGLE's D3D11 呈现器。
        //如果硬件支持 D3D11 功能级别 10_0+，那么仅可使用这些属性来成功执行 eglInitialize。
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,

        // EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER 是一个对移动设备性能有很大帮助的优化项。
        //但是它的语法可能会有更改。如果您遇到与它相关的编译问题，请更新 Visual Studio 模板。
        EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
        
        // EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE 是一个选项，在应用程序挂起时，它可以使 ANGLE 代表
        //应用程序自动调用 IDXGIDevice3::Trim 方法。
        //在应用程序挂起时调用 IDXGIDevice3::Trim 是 Windows 应用商店应用的认证要求。
        EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
        EGL_NONE,
    };
    
    const EGLint fl9_3DisplayAttributes[] =
    {
        //这些属性可用于请求 ANGLE 的具有 D3D11 功能级别 9_3 的  D3D11 呈现器。
        //当使用默认的显示属性调用 eglInitialize 失败时将使用这些属性。
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE, 9,
        EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE, 3,
        EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
        EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
        EGL_NONE,
    };

    const EGLint warpDisplayAttributes[] =
    {
        //这些属性可用于请求 D3D11 WARP。
        //如果使用默认的显示属性和 9_3 显示属性调用 eglInitialize 时均失败，则将使用这些属性。
        EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
        EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
        EGL_ANGLE_DISPLAY_ALLOW_RENDER_TO_BACK_BUFFER, EGL_TRUE,
        EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE, EGL_TRUE,
        EGL_NONE,
    };
    
    EGLConfig config = NULL;

    // eglGetPlatformDisplayEXT 是 eglGetDisplay 的替代项。它允许在显示属性中传递，用于配置 D3D11。
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    if (!eglGetPlatformDisplayEXT)
    {
        throw Exception::CreateException(E_FAIL, L"Failed to get function eglGetPlatformDisplayEXT");
    }

    //
    //要初始化显示，我们设定了三个调用 eglGetPlatformDisplayEXT 和 eglInitialize 的方法，其中包括
    //传递到 eglGetPlatformDisplayEXT 的变化参数:
    // 1) 第一个调用使用“defaultDisplayAttributes”作为参数。这与 D3D11 功能级别 10_0+ 相对应。
    // 2) 如果步骤 1 的 eglInitialize 调用失败 (例如，由于默认的 GPU 不支持 10_0+)，那么我们将再次尝试
    //使用“fl9_3DisplayAttributes”。这与 D3D11 功能级别 9_3 相对应。
    // 3) 如果步骤 2 的 eglInitialize 调用失败 (例如，由于默认的 GPU 不支持 9_3+)，那么我们将再次尝试
    //使用“warpDisplayAttributes”。这与 WARP (一种 D3D11 软件光栅器) 的 D3D11 功能级别 11_0 相对应。
    //
    
    //以下代码试图将 EGL 初始化为 D3D11 功能级别 10_0+。请参见上面的注释获取详细信息。
    mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, defaultDisplayAttributes);
    if (mEglDisplay == EGL_NO_DISPLAY)
    {
        throw Exception::CreateException(E_FAIL, L"Failed to get EGL display");
    }

    if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE)
    {
        //以下代码试图将 EGL 初始化为 D3D11 功能级别 9_3 (如果 10_0+ 不可用。例如，在某些移动设备上)。
        mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, fl9_3DisplayAttributes);
        if (mEglDisplay == EGL_NO_DISPLAY)
        {
            throw Exception::CreateException(E_FAIL, L"Failed to get EGL display");
        }

        if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE)
        {
            //以下代码将 EGL 初始化为 WARP 的 D3D11 功能级别 11_0 (如果 9_3+ 在默认 GPU 上不可用)。
            mEglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, warpDisplayAttributes);
            if (mEglDisplay == EGL_NO_DISPLAY)
            {
                throw Exception::CreateException(E_FAIL, L"Failed to get EGL display");
            }

            if (eglInitialize(mEglDisplay, NULL, NULL) == EGL_FALSE)
            {
                //如果对 eglInitialize 的所有调用均返回 EGL_FALSE，那么出现了一个错误。
                throw Exception::CreateException(E_FAIL, L"Failed to initialize EGL");
            }
        }
    }

    EGLint numConfigs = 0;
    if ((eglChooseConfig(mEglDisplay, configAttributes, &config, 1, &numConfigs) == EGL_FALSE) || (numConfigs == 0))
    {
        throw Exception::CreateException(E_FAIL, L"Failed to choose first EGLConfig");
    }

    //创建一个 PropertySet，并使用 EGLNativeWindowType 进行初始化。
    PropertySet^ surfaceCreationProperties = ref new PropertySet();
    surfaceCreationProperties->Insert(ref new String(EGLNativeWindowTypeProperty), window);

    //您可以将曲面配置为在较低分辨率下进行渲染，并放大到
    // 与窗口一样的大小。缩放操作在移动硬件上常常是没有固定比例的。
    //
    //配置 SwapChainPanel 的一种方法是精确指定对其进行渲染时采用的分辨率。
    //Size customRenderSurfaceSize = Size(800, 600);
    // surfaceCreationProperties->Insert(ref new String(EGLRenderSurfaceSizeProperty), PropertyValue::CreateSize(customRenderSurfaceSize));
    //
    //另一种方法是告诉 SwapChainPanel 在与其大小进行比较的特定缩放因子下进行渲染。
    //例如，如果 SwapChainPanel 为 1920x1280，那么将因子设置为 0.5f 可以在 960x640 的分辨率下渲染应用
    // float customResolutionScale = 0.5f;
    // surfaceCreationProperties->Insert(ref new String(EGLRenderResolutionScaleProperty), PropertyValue::CreateSingle(customResolutionScale));

    mEglSurface = eglCreateWindowSurface(mEglDisplay, config, reinterpret_cast<IInspectable*>(surfaceCreationProperties), surfaceAttributes);
    if (mEglSurface == EGL_NO_SURFACE)
    {
        throw Exception::CreateException(E_FAIL, L"Failed to create EGL fullscreen surface");
    }

    mEglContext = eglCreateContext(mEglDisplay, config, EGL_NO_CONTEXT, contextAttributes);
    if (mEglContext == EGL_NO_CONTEXT)
    {
        throw Exception::CreateException(E_FAIL, L"Failed to create EGL context");
    }

    if (eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext) == EGL_FALSE)
    {
        throw Exception::CreateException(E_FAIL, L"Failed to make fullscreen EGLSurface current");
    }
}

void App::CleanupEGL()
{
    if (mEglDisplay != EGL_NO_DISPLAY && mEglSurface != EGL_NO_SURFACE)
    {
        eglDestroySurface(mEglDisplay, mEglSurface);
        mEglSurface = EGL_NO_SURFACE;
    }

    if (mEglDisplay != EGL_NO_DISPLAY && mEglContext != EGL_NO_CONTEXT)
    {
        eglDestroyContext(mEglDisplay, mEglContext);
        mEglContext = EGL_NO_CONTEXT;
    }

    if (mEglDisplay != EGL_NO_DISPLAY)
    {
        eglTerminate(mEglDisplay);
        mEglDisplay = EGL_NO_DISPLAY;
    }
}
