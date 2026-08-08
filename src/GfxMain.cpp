#include "GfxMain.hpp"

#include "CppHelpers.hpp"
#include "utils.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <print>
namespace kronk
{

float vertices[] = {
  // Front face
  -0.5f,
  -0.5f,
  0.5f, // 0: Bottom-left
  0.5f,
  -0.5f,
  0.5f, // 1: Bottom-right
  0.5f,
  0.5f,
  0.5f, // 2: Top-right
  -0.5f,
  0.5f,
  0.5f, // 3: Top-left
  // Back face
  -0.5f,
  -0.5f,
  -0.5f, // 4: Bottom-left
  0.5f,
  -0.5f,
  -0.5f, // 5: Bottom-right
  0.5f,
  0.5f,
  -0.5f, // 6: Top-right
  -0.5f,
  0.5f,
  -0.5f // 7: Top-left
};

unsigned int indices[] = {
  0, 1, 2, 2, 3, 0, // Front face
  1, 5, 6, 6, 2, 1, // Right face
  7, 6, 5, 5, 4, 7, // Back face
  4, 0, 3, 3, 7, 4, // Left face
  3, 2, 6, 6, 7, 3, // Top face
  4, 5, 1, 1, 0, 4  // Bottom face
};

void RenderInit()
{
}

void RenderThreadMain(RenderThreadData &data, GameThreadOutput &in)
{
}

} // namespace kronk
