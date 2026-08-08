#pragma once

#include "ModuleIO.hpp"
#include "utils.hpp"

namespace kronk
{

struct RenderThreadData
{
  SDL_Window *window;
};

void RenderInit();

void RenderThreadMain(RenderThreadData &data, GameThreadOutput &in);

} // namespace kronk
