/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

// TODO: Avoid to create RBO/FBO at each calls of generation functions

#include "r3d.h"

#include "./r3d_state.h"

#include <assert.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>


/* === Internal functions === */

static TextureCubemap
r3d_skybox_load_from_panorama_hdr(const char* fileName, int size)
{
    // Load the HDR panorama texture
    Texture2D panorama = LoadTexture(fileName);

    // Create skybox cubemap texture and depth renderbuffer
    unsigned int cubemapId = rlLoadTextureCubemap(NULL, size, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16, 1);
    unsigned int rbo = rlLoadTextureDepth(size, size, true);

    // Create and configure framebuffer
    unsigned int fbo = rlLoadFramebuffer();
    rlFramebufferAttach(fbo, cubemapId, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X, 0);
    rlFramebufferAttach(fbo, rbo, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);

    // Enable shader for converting HDR to cubemap
    r3d_shader_enable(generate.cubemapFromEquirectangular);

    // Define and send projection matrix to shader
    Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, rlGetCullDistanceNear(), rlGetCullDistanceFar());
    r3d_shader_set_mat4(generate.cubemapFromEquirectangular, uMatProj, matProj);

    // Set viewport to framebuffer dimensions
    rlViewport(0, 0, size, size);
    rlDisableBackfaceCulling();

    // Bind panorama texture for drawing
    r3d_shader_bind_sampler2D(generate.cubemapFromEquirectangular, uTexEquirectangular, panorama.id);

    // Loop through and render each cubemap face
    for (int i = 0; i < 6; i++) {
        r3d_shader_set_mat4(generate.cubemapFromEquirectangular, uMatView, R3D.misc.matCubeViews[i]);
        rlFramebufferAttach(fbo, cubemapId, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X + i, 0);
        rlEnableFramebuffer(fbo);

        rlClearScreenBuffers();
        r3d_primitive_bind_and_draw_cube();
    }

    // Clean up: unbind texture and framebuffer
    r3d_shader_unbind_sampler2D(generate.cubemapFromEquirectangular, uTexEquirectangular);
    rlDisableShader();
    rlDisableTexture();
    rlDisableFramebuffer();
    rlUnloadFramebuffer(fbo);

    // Reset viewport and re-enable culling
    rlViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    rlEnableBackfaceCulling();

    // Return cubemap texture
    TextureCubemap cubemap = {
        .id = cubemapId,
        .width = size,
        .height = size,
        .mipmaps = 1,
        .format = panorama.format
    };

    return cubemap;
}

static TextureCubemap r3d_skybox_generate_irradiance(TextureCubemap sky)
{
    // Compute irradiance resolution
    int size = sky.width / 16;
    if (size < 32) size = 32;

    // Create irradiance cubemap texture and depth renderbuffer
    unsigned int irradianceId = rlLoadTextureCubemap(NULL, size, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32, 1);
    unsigned int rbo = rlLoadTextureDepth(size, size, true);

    // Create and configure framebuffer
    unsigned int fbo = rlLoadFramebuffer();
    rlFramebufferAttach(fbo, irradianceId, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X, 0);
    rlFramebufferAttach(fbo, rbo, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);

    // Set viewport to framebuffer dimensions
    rlViewport(0, 0, size, size);
    rlDisableBackfaceCulling();

    // Enable shader for irradiance convolution
    r3d_shader_enable(generate.irradianceConvolution);
    r3d_shader_set_mat4(generate.irradianceConvolution, uMatProj,
        MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0)
    );
    r3d_shader_bind_samplerCube(generate.irradianceConvolution, uCubemap, sky.id);

    // Render irradiance to cubemap faces
    for (int i = 0; i < 6; i++) {
        r3d_shader_set_mat4(generate.irradianceConvolution, uMatView, R3D.misc.matCubeViews[i]);
        rlFramebufferAttach(fbo, irradianceId, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X + i, 0);
        rlEnableFramebuffer(fbo);
        rlClearScreenBuffers();
        r3d_primitive_bind_and_draw_cube();
    }

    // Disable shader
    r3d_shader_unbind_samplerCube(generate.irradianceConvolution, uCubemap);
    r3d_shader_disable();

    // Clean up
    rlDisableFramebuffer();
    rlUnloadFramebuffer(fbo);

    // Reset viewport to default dimensions
    rlViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    rlEnableBackfaceCulling();

    // Return irradiance cubemap
    TextureCubemap irradiance = {
        .id = irradianceId,
        .width = size,
        .height = size,
        .mipmaps = 1,
        .format = RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32
    };

    return irradiance;
}

static TextureCubemap r3d_skybox_generate_prefilter(TextureCubemap sky)
{
    static const int PREFILTER_SIZE = 128;
    static const int MAX_MIP_LEVELS = 8;

    // Create prefilter cubemap texture
    unsigned int prefilterId = 0;
    glGenTextures(1, &prefilterId);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterId);
    for (int i = 0; i < 6; i++) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
            128, 128, 0, GL_RGB, GL_FLOAT, NULL
        );
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // Generate mipmaps
    rlEnableTextureCubemap(prefilterId);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    rlDisableTextureCubemap();

    // Create depth renderbuffer and framebuffer
    unsigned int rbo = 0;
    glGenRenderbuffers(1, &rbo);
    unsigned int fbo = rlLoadFramebuffer();
    rlFramebufferAttach(fbo, prefilterId, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X, 0);

    // Enable shader for prefiltering
    r3d_shader_enable(generate.prefilter);
    r3d_shader_set_mat4(generate.prefilter, uMatProj, MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0));
    r3d_shader_bind_samplerCube(generate.prefilter, uCubemap, sky.id);

    // Configure framebuffer and rendering parameters
    rlEnableFramebuffer(fbo);
    rlDisableBackfaceCulling();

    // Process each mipmap level
    for (int mip = 0; mip < MAX_MIP_LEVELS; mip++) {
        int mipWidth = (int)(PREFILTER_SIZE * powf(0.5, (float)mip));
        int mipHeight = (int)(PREFILTER_SIZE * powf(0.5, (float)mip));

        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);

        rlViewport(0, 0, mipWidth, mipHeight);
        float roughness = (float)mip / (float)(MAX_MIP_LEVELS - 1);
        r3d_shader_set_float(generate.prefilter, uRoughness, roughness);

        // Render all faces of the cubemap
        for (int i = 0; i < 6; i++) {
            r3d_shader_set_mat4(generate.prefilter, uMatView, R3D.misc.matCubeViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterId, mip);
            rlClearScreenBuffers();
            r3d_primitive_bind_and_draw_cube();
        }
    }

    // Disable shader
    r3d_shader_unbind_samplerCube(generate.prefilter, uCubemap);
    r3d_shader_disable();

    // Clean up
    rlDisableFramebuffer();
    rlUnloadFramebuffer(fbo);
    
    // Reset viewport to default dimensions
    rlViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    rlDisableBackfaceCulling();

    // Return prefiltered cubemap
    TextureCubemap prefilter = {
        .id = prefilterId,
        .width = PREFILTER_SIZE,
        .height = PREFILTER_SIZE,
        .mipmaps = MAX_MIP_LEVELS,
        .format = RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16
    };

    return prefilter;
}


/* === Public functions === */

R3D_Skybox R3D_LoadSkybox(const char* fileName, CubemapLayout layout)
{
    R3D_Skybox sky = { 0 };

    // Load the cubemap texture from the image file
    Image img = LoadImage(fileName);
    sky.cubemap = LoadTextureCubemap(img, layout);
    UnloadImage(img);

    // Generate maps
    sky.irradiance = r3d_skybox_generate_irradiance(sky.cubemap);
    sky.prefilter = r3d_skybox_generate_prefilter(sky.cubemap);

    return sky;
}

R3D_Skybox R3D_LoadSkyboxHDR(const char* fileName, int size)
{
    R3D_Skybox sky = { 0 };
    sky.cubemap = r3d_skybox_load_from_panorama_hdr(fileName, size);
    sky.irradiance = r3d_skybox_generate_irradiance(sky.cubemap);
    sky.prefilter = r3d_skybox_generate_prefilter(sky.cubemap);
    return sky;
}

void R3D_UnloadSkybox(R3D_Skybox sky)
{
    UnloadTexture(sky.cubemap);
    UnloadTexture(sky.irradiance);
    UnloadTexture(sky.prefilter);
}
