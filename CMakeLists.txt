cmake_minimum_required(VERSION 3.21)
project(RetroJapanOverlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "RetroJapanOverlay requires Windows.")
endif()

set(SRC_FILES
    src/main.cpp
    src/Application.cpp
    src/OverlayWindow.cpp
    src/TrayController.cpp
    src/SettingsWindow.cpp
    src/D3DRenderer.cpp
    src/ParticleSystem.cpp
    src/MouseField.cpp
    src/AsciiAtlas.cpp
    src/SceneRenderer.cpp
    src/Configuration.cpp
    src/MonitorManager.cpp
    src/Utilities/Logger.cpp
    src/Utilities/Paths.cpp
    src/Utilities/ComHelpers.cpp
    src/Utilities/OlkToggle.cpp
)

add_executable(RetroJapanOverlay WIN32 ${SRC_FILES})

target_include_directories(RetroJapanOverlay PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(RetroJapanOverlay PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    $<$<CONFIG:Debug>:_DEBUG>
)

target_compile_options(RetroJapanOverlay PRIVATE
    /W4
    /permissive-
    /Zc:__cplusplus
    /MP
)

target_link_libraries(RetroJapanOverlay PRIVATE
    d3d11
    dxgi
    d3dcompiler
    user32
    gdi32
    shell32
    shlwapi
    shcore
    ole32
    comctl32
    msimg32
)

# --- HLSL build-time compilation (fxc) ---
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

find_program(FXC_EXECUTABLE fxc
    HINTS
        "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin"
)

set(COMPILED_SHADERS)
if(FXC_EXECUTABLE)
    message(STATUS "Found fxc: ${FXC_EXECUTABLE}")
    set(COMPILED_SHADERS "")
    file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

    set(_VS_OUT ${SHADER_OUT_DIR}/AsciiVertexShader.cso)
    set(_PS_OUT ${SHADER_OUT_DIR}/AsciiPixelShader.cso)

    add_custom_command(
        OUTPUT ${_VS_OUT}
        COMMAND ${FXC_EXECUTABLE} /nologo /T vs_5_0 /E main /Fo ${_VS_OUT}
                ${SHADER_DIR}/AsciiVertexShader.hlsl
        DEPENDS ${SHADER_DIR}/AsciiVertexShader.hlsl
        COMMENT "Compiling AsciiVertexShader.hlsl"
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${_PS_OUT}
        COMMAND ${FXC_EXECUTABLE} /nologo /T ps_5_0 /E main /Fo ${_PS_OUT}
                ${SHADER_DIR}/AsciiPixelShader.hlsl
        DEPENDS ${SHADER_DIR}/AsciiPixelShader.hlsl
        COMMENT "Compiling AsciiPixelShader.hlsl"
        VERBATIM
    )

    add_custom_target(CompileShaders ALL DEPENDS ${_VS_OUT} ${_PS_OUT})
    add_dependencies(RetroJapanOverlay CompileShaders)

    add_custom_command(TARGET RetroJapanOverlay POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:RetroJapanOverlay>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_VS_OUT}" "${_PS_OUT}"
            "$<TARGET_FILE_DIR:RetroJapanOverlay>/shaders/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SHADER_DIR}"
            "$<TARGET_FILE_DIR:RetroJapanOverlay>/shaders"
        COMMENT "Copying shaders next to executable"
    )
else()
    message(WARNING "fxc not found — shaders will compile at runtime via D3DCompile")
    add_custom_command(TARGET RetroJapanOverlay POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:RetroJapanOverlay>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SHADER_DIR}
            $<TARGET_FILE_DIR:RetroJapanOverlay>/shaders
        COMMENT "Copying HLSL sources next to executable"
    )
endif()

add_custom_command(TARGET RetroJapanOverlay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/default_config.ini
        $<TARGET_FILE_DIR:RetroJapanOverlay>/default_config.ini
)

set_target_properties(RetroJapanOverlay PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:RetroJapanOverlay>"
)

install(TARGETS RetroJapanOverlay RUNTIME DESTINATION bin)
