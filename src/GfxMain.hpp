#pragma once

#include "ModuleIO.hpp"
#include "utils.hpp"

#include <glbinding/gl/gl.h>

namespace kronk
{
using namespace gl;

struct RenderThreadData
{
  SDL_Window *window;
  GLuint      buffer;
  u32         indexCount{};
  u32         indexOffset{};
  u32         indexSize{};
};

void RenderInit();

void RenderThreadMain(RenderThreadData &data, GameThreadOutput &in);

} // namespace kronk
