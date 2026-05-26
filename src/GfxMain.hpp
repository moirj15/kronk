#pragma once

#include "ModuleIO.hpp"

#include <glbinding/gl/gl.h>

namespace kronk
{
using namespace gl;

struct RenderThreadData
{
  SDL_Window *window;
  GLuint      buffer;
};

void RenderThreadMain(RenderThreadData &data, GameThreadOutput &in);

} // namespace kronk
