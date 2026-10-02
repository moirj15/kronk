cmake_minimum_required(VERSION 3.28)

project(rts LANGUAGES CXX)

message("prefix ${CMAKE_PREFIX_PATH}")
include_directories(${CMAKE_SOURCE_DIR}/src)

find_package(SDL3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(Vulkan) # CMake 3.21+
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)


add_executable(rts
        src/main.cpp
        src/utils.hpp
        src/utils.cpp
        src/MessageQueue.cpp
        src/MessageQueue.hpp
        src/GfxMain.cpp
        src/GfxMain.hpp
        src/ModuleIO.hpp
        src/CppHelpers.hpp
        src/Render/MemorySystem.hpp
        src/Render/MemorySystem.cpp
        src/Render/Utils.hpp
)

include(cmake/CompilerWarnings.cmake)
myproject_set_project_warnings(
        rts
        "TRUE"
        ""
        ""
        ""
        "")

target_compile_definitions(rts PRIVATE
        GLM_FORCE_EXPLICIT_CTOR
)
target_compile_features(rts PRIVATE cxx_std_23)
target_link_libraries(rts PRIVATE
        SDL3::SDL3
        imgui::imgui
        assimp::assimp
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        vk-bootstrap::vk-bootstrap-compiler-warnings
        #        slang::gfx
        slang::slang
        #        slang::slang-llvm
        #        slang::slang-glslang


        gdi32.lib
        user32.lib
        kernel32.lib
        msvcrtd.lib
        winmm.lib
        imm32.lib
        version.lib
        libcmt.lib
        dxgi.lib
        dxguid.lib
        winmm.lib
        gdi32
        Setupapi.lib
)

