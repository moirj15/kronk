#pragma once

#include "dx.hpp"
#include "utils.hpp"

#include <d3d11_3.h>
#include <filesystem>
#include <vector>

using RenderProgramHandle  = u32;
using ComputeProgramHandle = u32;

struct RenderProgram
{
  ID3D11VertexShader *vertexShader;
  ID3D11PixelShader  *pixelShader;
};

template<typename T>
struct WatchPair
{
  std::vector<std::string>                     sourcePaths;
  std::vector<ComPtr<T>>                       shaders;
  std::vector<std::filesystem::file_time_type> times;

  void Add(std::string sourcePath, ComPtr<T> shader, std::filesystem::file_time_type time)
  {
    sourcePaths.emplace_back(std::move(sourcePath));
    shaders.emplace_back(shader);
    times.emplace_back(time);
  }

  void Set(u32 i, ComPtr<T> shader, std::filesystem::file_time_type time)
  {
    assert(i < shaders.size());
    shaders[i] = shader;
    times[i]   = time;
  }
};

/**
 * @brief Manages shader compilation and lifetime, while also providing recompilation of shaders on
 * modification when running a debug build.
 */
class ShaderWatcher final
{
  RenderProgramHandle  mNextRenderProgram{};
  ComputeProgramHandle mNextComputeProgram{};

  WatchPair<ID3D11VertexShader>  mVertexShaders;
  WatchPair<ID3D11PixelShader>   mPixelShaders;
  WatchPair<ID3D11ComputeShader> mComputeShaders;
  ID3D11Device3                 *mDevice;

public:
  explicit ShaderWatcher(ID3D11Device3 *device) : mDevice{device}
  {
  }

  /**
   * @brief Registers and compiles the given vertex and pixel shaders. Note: paths are relative to
   * the directory the application is started in.
   * @param vertexPath The path to the vertex shader.
   * @param pixelPath The path to the pixel shader.
   * @return A `RenderProgramHandle` that can be used to access the compiled shaders by calling
   * `GetRenderProgram.
   */
  RenderProgramHandle RegisterShader(const std::string &vertexPath, const std::string &pixelPath);

  /**
   * @brief Registers and compiles the given compute shader. Note: path is relative to the directory
   * the application is started in.
   * @param computePath The path to the compute shader.
   * @return A `ComputeProgramHandle` that can be used to access the compiled shader by calling
   * `GetComputeProgram`.
   */
  ComputeProgramHandle RegisterShader(const std::string &computePath);

  /**
   * @brief Retrieves the `RenderProgram` containing the vertex and pixel shaders corresponding to
   * the given handle. Note: if running a debug build, this will check if the shader sources have
   * been updated and recompile if necessary, so pointers may be invalidated.
   * @param handle The handle that points to the `RenderProgram`.
   * @return The `RenderProgram` containing the vertex and pixel shaders.
   */
  RenderProgram GetRenderProgram(RenderProgramHandle handle);

  /**
   * @brief Retrieves the compute shader corresponding to the given handle. Note: if running a debug
   * build, this will check if the shader sources have been updated and recompile if necessary, so
   * pointers may be invalidated.
   * @param handle The handle that points to the compute shader.
   * @return The compute shader.
   */
  ID3D11ComputeShader *GetComputeProgram(ComputeProgramHandle handle);
};