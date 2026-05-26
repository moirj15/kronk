#pragma once

#include "MessageQueue.hpp"

struct SDL_Window;

namespace kronk
{

struct GameThreadData
{
};

struct GameThreadOutput
{
  MessageQueue gameToRenderQueue{};
};

} // namespace kronk
