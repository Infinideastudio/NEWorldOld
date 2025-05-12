module;

#include <glad/gl.h>
#include <png.h>

export module render:image;
import std;
import types;
import debug;
import math;

namespace render {

// Concept for a color type.
// The default constructor may not necessarily be trivial.
// Standard layout is required for stability of the byte representation.
export template <typename T>
concept Color = std::is_default_constructible_v<T> && std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

// Stores a tightly-packed 3D texture image.
// The dimension is `depth * height * width`. Pixels start at the bottom-left corner of the first layer.
export template <Color T>
class Image {
public:
    Image() = default;

    explicit Image(size_t depth, size_t height, size_t width):
        _pixels(std::make_unique<T[]>(depth * height * width)),
        _dims(_pixels.get(), depth, height, width) {}

    auto depth() const -> size_t {
        return _dims.extent(0);
    }

    auto height() const -> size_t {
        return _dims.extent(1);
    }

    auto width() const -> size_t {
        return _dims.extent(2);
    }

    template <typename Self>
    auto data(this Self&& self) {
        return std::forward<Self>(self)._pixels.get();
    }

    auto size() const -> size_t {
        return _dims.size();
    }

    // Pixel access.
    template <typename Self>
    auto operator[](this Self&& self, size_t layer, size_t row, size_t column) -> auto&& {
        return std::forward<Self>(self)._dims[layer, row, column];
    }

    // Reshapes the dimensions without changing total size or pixel data.
    void reshape(size_t depth, size_t height, size_t width) {
        assert(depth * height * width == size());
        _dims = std::mdspan<T, std::dextents<size_t, 3>>(_dims.data_handle(), depth, height, width);
    }

    // Fills all pixels in a single layer with the same color.
    void fill(size_t layer, T const& color) {
        assert(layer < depth());
        auto stride = _dims.stride(0);
        std::fill_n(_dims.data_handle() + layer * stride, stride, color);
    }

    // Fills a region within a single layer using pixels from another image.
    // Can be replaced by `std::submdspan` in C++26.
    void fill(size_t layer, size_t row, size_t column, Image const& src, size_t src_layer = 0) {
        assert(layer < depth() && row + src.height() <= height() && column + src.width() <= width());
        assert(src_layer < src.depth());
        for (auto i = 0uz; i < src.height(); i++) {
            for (auto j = 0uz; j < src.width(); j++) {
                _dims[layer, row + i, column + j] = src._dims[src_layer, i, j];
            }
        }
    }

private:
    std::unique_ptr<T[]> _pixels;
    std::mdspan<T, std::dextents<size_t, 3>> _dims;
};

export using ImageRGB = Image<Vec3u8>;
export using ImageRGBA = Image<Vec4u8>;

void _file_deleter(std::FILE* file) {
    if (file) {
        std::fclose(file); // NOLINT
    }
}

void _reader_deleter(png_structp reader) {
    if (reader) {
        png_destroy_read_struct(&reader, nullptr, nullptr);
    }
}

void _writer_deleter(png_structp writer) {
    if (writer) {
        png_destroy_write_struct(&writer, nullptr);
    }
}

void _info_deleter(png_structp parent, png_infop info) {
    if (info) {
        png_destroy_info_struct(parent, &info);
    }
}

// Helper function to load an image from a PNG file.
export auto load_png_image(std::filesystem::path const& filename) -> std::expected<ImageRGBA, std::string> {
    // See: http://www.libpng.org/pub/png/libpng-1.4.0-manual.pdf
    // Open the file through C standard API.
    auto file = std::unique_ptr<std::FILE, decltype(&_file_deleter)>(
        std::fopen(filename.string().c_str(), "rb"),
        &_file_deleter
    );
    if (!file) {
        return std::unexpected("failed to open input file");
    }

    // Create the read struct.
    auto reader = std::unique_ptr<png_struct, decltype(&_reader_deleter)>(
        png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr),
        &_reader_deleter
    );
    if (!reader) {
        return std::unexpected("failed to create PNG read struct");
    }

    // Create info structs.
    auto info_deleter = [&](png_infop info) {
        _info_deleter(reader.get(), info);
    };
    auto init_info =
        std::unique_ptr<png_info, decltype(info_deleter)>(png_create_info_struct(reader.get()), info_deleter);
    auto end_info =
        std::unique_ptr<png_info, decltype(info_deleter)>(png_create_info_struct(reader.get()), info_deleter);
    if (!init_info || !end_info) {
        return std::unexpected("failed to create PNG info struct");
    }

    // Set C exception handling.
    if (setjmp(png_jmpbuf(reader.get())) == 0) {
        // Read the info.
        png_init_io(reader.get(), file.get());
        png_read_info(reader.get(), init_info.get());
        auto height = png_get_image_height(reader.get(), init_info.get());
        auto width = png_get_image_width(reader.get(), init_info.get());
        auto color_type = png_get_color_type(reader.get(), init_info.get());
        auto bit_depth = png_get_bit_depth(reader.get(), init_info.get());

        // Ensure 8-bit RGBA channels.
        if (color_type & PNG_COLOR_MASK_PALETTE) {
            png_set_palette_to_rgb(reader.get());
        }
        if (color_type & PNG_COLOR_MASK_COLOR) {
            if (bit_depth > 8) {
                png_set_scale_16(reader.get());
            }
        } else {
            png_set_gray_to_rgb(reader.get());
            if (bit_depth < 8) {
                png_set_expand_gray_1_2_4_to_8(reader.get());
            }
            if (bit_depth > 8) {
                png_set_scale_16(reader.get());
            }
        }
        if (!(color_type & PNG_COLOR_MASK_ALPHA)) {
            png_set_tRNS_to_alpha(reader.get());
            png_set_add_alpha(reader.get(), 255, PNG_FILLER_AFTER);
        }

        // Update the info struct with the transformations.
        png_read_update_info(reader.get(), init_info.get());
        if (png_get_bit_depth(reader.get(), init_info.get()) != 8
            || png_get_channels(reader.get(), init_info.get()) != 4) {
            return std::unexpected("unexpected PNG color format after transformations");
        }
        if (png_get_rowbytes(reader.get(), init_info.get()) != width * sizeof(Vec4u8)) {
            return std::unexpected("unexpected PNG row size after transformations");
        }

        // Create the receiving buffer.
        // This has non-trivial destructor, so we need to reset C exception handling.
        // See: https://en.cppreference.com/w/cpp/utility/program/longjmp
        auto res = ImageRGBA(1, height, width);
        if (setjmp(png_jmpbuf(reader.get())) == 0) {
            // Read image data.
            for (auto i = height; i-- > 0uz;) {
                auto row = std::as_writable_bytes(std::span(&res[0, i, 0], width));
                assert(row.size() == width * sizeof(Vec4u8));
                png_read_row(reader.get(), reinterpret_cast<unsigned char*>(row.data()), nullptr); // NOLINT
            }
            png_read_end(reader.get(), end_info.get());
            return std::move(res);
        }
        return std::unexpected("libpng exception while reading PNG image data");
    }
    return std::unexpected("libpng exception while reading PNG info");
}

// Helper function to save an image into a PNG file.
export auto save_png_image(std::filesystem::path const& filename, ImageRGBA const& image)
    -> std::expected<void, std::string> {
    // See: http://www.libpng.org/pub/png/libpng-1.4.0-manual.pdf
    // Open the file through C standard API.
    auto file = std::unique_ptr<std::FILE, decltype(&_file_deleter)>(
        std::fopen(filename.string().c_str(), "wb"),
        &_file_deleter
    );
    if (!file) {
        return std::unexpected("failed to open output file");
    }

    // Create the write struct.
    auto writer = std::unique_ptr<png_struct, decltype(&_writer_deleter)>(
        png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr),
        &_writer_deleter
    );
    if (!writer) {
        return std::unexpected("failed to create PNG write struct");
    }

    // Create info struct.
    auto info_deleter = [&](png_infop info) {
        _info_deleter(writer.get(), info);
    };
    auto init_info =
        std::unique_ptr<png_info, decltype(info_deleter)>(png_create_info_struct(writer.get()), info_deleter);
    if (!init_info) {
        return std::unexpected("failed to create PNG info struct");
    }

    // Set C exception handling.
    if (setjmp(png_jmpbuf(writer.get())) == 0) {
        // Write the info.
        png_init_io(writer.get(), file.get());
        png_set_compression_level(writer.get(), 1); // 1 = Z_BEST_SPEED
        png_set_IHDR(
            writer.get(),
            init_info.get(),
            image.width(),
            image.height(),
            8,
            PNG_COLOR_TYPE_RGB_ALPHA,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT
        );
        png_write_info(writer.get(), init_info.get());
        if (png_get_rowbytes(writer.get(), init_info.get()) != image.width() * sizeof(Vec4u8)) {
            return std::unexpected("unexpected PNG row size for output");
        }

        // Write image data.
        for (auto i = image.height(); i-- > 0uz;) {
            auto row = std::as_bytes(std::span(&image[0, i, 0], image.width()));
            assert(row.size() == image.width() * sizeof(Vec4u8));
            png_write_row(writer.get(), reinterpret_cast<unsigned char const*>(row.data())); // NOLINT
        }
        png_write_end(writer.get(), init_info.get());
        return {};
    }
    return std::unexpected("libpng exception while writing PNG file");
}

}
