#include "GfxMain.hpp"
#include "MessageQueue.hpp"
#include "ModuleIO.hpp"
#include "utils.hpp"

#include <SDL3/SDL.h>
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <print>

constexpr u32 WIDTH  = 1920;
constexpr u32 HEIGHT = 1080;

using namespace gl;

namespace kronk
{

GLuint CompileShaders(const GLchar **vertexShaderSource, const GLchar **fragmentShaderSource)
{
  // Compile vertex shader
  GLuint vertexShader(glCreateShader(GL_VERTEX_SHADER));
  glShaderSource(vertexShader, 1, vertexShaderSource, nullptr);
  glCompileShader(vertexShader);

  // Compile fragment shader
  GLuint fragmentShader(glCreateShader(GL_FRAGMENT_SHADER));
  glShaderSource(fragmentShader, 1, fragmentShaderSource, nullptr);
  glCompileShader(fragmentShader);

  // Link vertex and fragment shader together
  GLuint program(glCreateProgram());
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);

  // Delete shaders objects
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return program;
}

void GameThreadMain(GameThreadData &in, GameThreadOutput &out)
{
  bool running = true;
  out.gameToRenderQueue.SendMessage(ClearScreen{.color = {1, 0, 1, 1}});
}

} // namespace kronk
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

  glbinding::initialize(
    SDL_GL_GetProcAddress); // only resolve functions that are actually used (lazy)

  glClearColor(1.0, 1.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  SDL_GL_SwapWindow(window);

  kronk::MessageQueue gameToRenderQueue{};
  // kronk::MessageQueue renderToGameQueue{};

  auto vSrc = io::ReadFile("shaders/2dRect.vert");
  auto fSrc = io::ReadFile("shaders/2dRect.frag");
  auto vP   = vSrc.data();
  auto fP   = fSrc.data();

  GLuint shader = kronk::CompileShaders((const GLchar **)&vP, (const GLchar **)&fP);

  bool                    running = true;
  SDL_Event               e{};
  kronk::GameThreadData   gameThreadData{};
  kronk::GameThreadOutput gameThreadOutput{};
  kronk::RenderThreadData renderThreadData{.window = window};

  gameThreadOutput.gameToRenderQueue.SendMessage(kronk::LoadAsset{.asset = kronk::Asset::Goose});

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

    GameThreadMain(gameThreadData, gameThreadOutput);
    kronk::RenderThreadMain(renderThreadData, gameThreadOutput);
  }
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
