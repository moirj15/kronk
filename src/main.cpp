#include "MessageQueue.hpp"
#include "utils.hpp"

#include <SDL3/SDL.h>
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <print>
#include <thread>

constexpr u32 WIDTH  = 1920;
constexpr u32 HEIGHT = 1080;

using namespace gl;

int main(int argc, char **argv)
{
  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    std::println("err: {}", SDL_GetError());
    assert(0);
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_Window *window = SDL_CreateWindow("SDL3 GL", 800, 600, SDL_WINDOW_OPENGL);

  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  assert(glContext);
  SDL_GL_SetSwapInterval(1); // VSync

  glbinding::initialize(reinterpret_cast<glbinding::ProcAddress (*)(const char *)>(
    SDL_GL_GetProcAddress)); // only resolve functions that are actually used (lazy)

  glClearColor(1.0, 1.0, 0.0, 1.0);
  glClear(gl::GL_COLOR_BUFFER_BIT);
  SDL_GL_SwapWindow(window);

  kronk::MessageQueue gameToRenderQueue{};

  bool      running = true;
  SDL_Event e{};

  std::thread gameThread{[&gameToRenderQueue]() {
    while (true)
    {
      gameToRenderQueue.SendMessage({.clearScreen = {.color = {1, 0, 1, 1}}});
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
  }};

  while (running)
  {
    while (SDL_PollEvent(&e))
    {
      if (e.type == SDL_EVENT_QUIT)
      {
        running = false;
      }
    }
    SDL_PumpEvents();
    gameToRenderQueue.BlockUntilMessages();
    kronk::Message m = gameToRenderQueue.GetMessage();
    glClearColor(
      m.clearScreen.color.r,
      m.clearScreen.color.g,
      m.clearScreen.color.b,
      m.clearScreen.color.a);
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(window);
  }
  gameThread.join();
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
