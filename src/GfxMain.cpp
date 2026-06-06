#include "GfxMain.hpp"

#include "CppHelpers.hpp"
#include "utils.hpp"

#include <SDL3/SDL.h>
#include <glbinding/gl/extension.h>
#include <glbinding/gl/gl.h>
#include <glbinding/glbinding.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <print>
namespace kronk
{

using namespace gl;

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

static void GLDebugOutput(
  GLenum       source,
  GLenum       type,
  unsigned int id,
  GLenum       severity,
  GLsizei      length,
  const char  *message,
  const void  *userParam)
{
  std::println("{}", message);
}

static GLuint CompileShaders(const GLchar **vertexShaderSource, const GLchar **fragmentShaderSource)
{
  // Compile vertex shader
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, vertexShaderSource, nullptr);
  glCompileShader(vertexShader);

  GLboolean success{};
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE)
  {
    GLint logLength = 0;
    glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLength);
    std::string log;
    log.resize(logLength);
    glGetShaderInfoLog(vertexShader, logLength, &logLength, log.data());
    std::println("{}", log);
  }

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

static GLuint playerShader;
static GLuint emptyVAO;

void RenderInit()
{
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(GLDebugOutput, nullptr);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
  auto          vertSrc = io::ReadFile("shaders/PlayerMesh.vert");
  auto          fragSrc = io::ReadFile("shaders/PlayerMesh.frag");
  const GLchar *vs      = vertSrc.data();
  const GLchar *fs      = fragSrc.data();
  playerShader          = CompileShaders(&vs, &fs);
  glCreateVertexArrays(1, &emptyVAO);
}

void RenderThreadMain(RenderThreadData &data, GameThreadOutput &in)
{
  in.gameToRenderQueue.BlockUntilMessages();
  bool readMoreMessages = true;
  while (readMoreMessages)
  {
    const Message m = in.gameToRenderQueue.GetMessage();
    std::visit(
      overloaded{
        [&](const ClearScreen &c) {
          glClearColor(c.color.r, c.color.g, c.color.b, c.color.a);
          glClear(GL_COLOR_BUFFER_BIT);
        },
        [&](const LoadAsset &cmd) {
          glCreateBuffers(1, &data.buffer);
          glNamedBufferStorage(
            data.buffer,
            sizeof(vertices) + sizeof(indices),
            nullptr,
            GL_DYNAMIC_STORAGE_BIT);

          glNamedBufferSubData(data.buffer, 0, sizeof(vertices), vertices);
          glNamedBufferSubData(data.buffer, sizeof(vertices), sizeof(indices), indices);
          data.indexCount  = std::size(indices);
          data.indexOffset = sizeof(vertices);
          data.indexSize   = sizeof(indices);
        },
        [&](const Draw &) {
          glUseProgram(playerShader);
          glBindVertexArray(emptyVAO);
          glm::mat4 m(1.0);
          glUniformMatrix4fv(0, 1, false, glm::value_ptr(m));
          glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, data.buffer);
          glBindBufferRange(
            GL_SHADER_STORAGE_BUFFER,
            1,
            data.buffer,
            data.indexOffset,
            data.indexSize);
          glDrawArrays(GL_TRIANGLES, 0, data.indexCount);
        },
        [&](const EndFrame &) { readMoreMessages = false; },
        [&](auto &cmd) {},
      },
      m);
  }

  SDL_GL_SwapWindow(data.window);
}

} // namespace kronk
