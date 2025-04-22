find_program(PYTHON_EXECUTABLE python3 REQUIRED)

if("${R3D_CUSTOM_SHADER_PATH}" STREQUAL "")
    set(R3D_CUSTOM_SHADER_PATH "${R3D_ROOT_PATH}/embedded/shaders" CACHE STRING "" FORCE)
endif()

message(STATUS "Using shader path: '${R3D_CUSTOM_SHADER_PATH}'")

function(ProcessShader shader_file var_name)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} "${R3D_ROOT_PATH}/scripts/glsl_minifier.py" "${shader_file}"
        OUTPUT_VARIABLE shader_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set("${var_name}" "${shader_output}" PARENT_SCOPE)
endfunction()

function(EmbedShaders generated_shaders_source shaders_source shaders_header)
    # Minify and store common shaders
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/common/screen.vs.glsl" VS_COMMON_SCREEN)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/common/cubemap.vs.glsl" VS_COMMON_CUBEMAP)
    # Minify and store generate shaders
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/generate/gaussianBlurDualPass.fs.glsl" FS_GENERATE_GAUSSIAN_BLUR_DUAL_PASS)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/generate/downsampling.fs.glsl" FS_GENERATE_DOWNSAMPLING)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/generate/upsampling.fs.glsl" FS_GENERATE_UPSAMPLING)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/generate/cubemapFromEquirectangular.fs.glsl" FS_GENERATE_CUBEMAP_FROM_EQUIRECTANGULAR)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/generate/irradianceConvolution.fs.glsl" FS_GENERATE_IRRADIANCE_CONVOLUTION)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/generate/prefilter.fs.glsl" FS_GENERATE_PREFILTER)
    # Minify and store raster shaders
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/geometry.vs.glsl" VS_RASTER_GEOMETRY)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/geometry.inst.vs.glsl" VS_RASTER_GEOMETRY_INST)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/geometry.fs.glsl" FS_RASTER_GEOMETRY)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/forward.vs.glsl" VS_RASTER_FORWARD)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/forward.inst.vs.glsl" VS_RASTER_FORWARD_INST)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/forward.fs.glsl" FS_RASTER_FORWARD)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/skybox.vs.glsl" VS_RASTER_SKYBOX)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/skybox.fs.glsl" FS_RASTER_SKYBOX)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/depth.vs.glsl" VS_RASTER_DEPTH)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/depth.inst.vs.glsl" VS_RASTER_DEPTH_INST)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/depth.fs.glsl" FS_RASTER_DEPTH)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/depthCube.vs.glsl" VS_RASTER_DEPTH_CUBE)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/depthCube.inst.vs.glsl" VS_RASTER_DEPTH_CUBE_INST)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/raster/depthCube.fs.glsl" FS_RASTER_DEPTH_CUBE)
    # Minify and store screen shaders
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/screen/ssao.fs.glsl" FS_SCREEN_SSAO)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/screen/ambient.fs.glsl" FS_SCREEN_AMBIENT)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/screen/lighting.fs.glsl" FS_SCREEN_LIGHTING)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/screen/scene.fs.glsl" FS_SCREEN_SCENE)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/screen/bloom.fs.glsl" FS_SCREEN_BLOOM)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/screen/fog.fs.glsl" FS_SCREEN_FOG)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/screen/tonemap.fs.glsl" FS_SCREEN_TONEMAP)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/screen/adjustment.fs.glsl" FS_SCREEN_ADJUSTMENT)
    ProcessShader("${R3D_CUSTOM_SHADER_PATH}/screen/fxaa.fs.glsl" FS_SCREEN_FXAA)
    # Generate shaders source file
    set(GENERATED_SHADERS_SOURCE "${CMAKE_BINARY_DIR}/generated/src/embedded/r3d_shaders.c")
    set(${generated_shaders_source} "${GENERATED_SHADERS_SOURCE}" PARENT_SCOPE)
    configure_file("${shaders_source}" "${GENERATED_SHADERS_SOURCE}")
endfunction()
