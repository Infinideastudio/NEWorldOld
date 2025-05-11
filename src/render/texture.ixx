module;

#include <glad/gl.h>

export module render:texture;
import std;
import types;
import debug;
import :image;

namespace render {

// Manages a GL texture object, similar to `std::unique_ptr`.
export class Texture {};

}
