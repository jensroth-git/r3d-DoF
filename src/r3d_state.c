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

#include "./r3d_state.h"

#include <assert.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

#include "./details/misc/r3d_dds_loader_ext.h"
#include "./details/misc/r3d_half.h"

#include "shaders.h"
#include "assets.h"

/* === Global state definition === */

struct R3D_State R3D = { 0 };

/* === Internal Functions === */

static char* r3d_shader_inject_defines(const char* code, const char* defines[], int count)
{
    if (!code) return NULL;

    // Calculate the size of the final buffer
    size_t codeLen = strlen(code);
    size_t definesLen = 0;

    // Calculate the total size of the #define statements
    for (int i = 0; i < count; i++) {
        definesLen += strlen(defines[i]) + 1;  // +1 for '\n'
    }

    // Allocate memory for the new shader
    size_t newSize = codeLen + definesLen + 1;
    char* newShader = (char*)RL_MALLOC(newSize);
    if (!newShader) return NULL;

    const char* versionStart = strstr(code, "#version");
    assert(versionStart && "Shader must have version");

    // Copy everything up to the end of the `#version` line
    const char* afterVersion = strchr(versionStart, '\n');
    if (!afterVersion) afterVersion = versionStart + strlen(versionStart);

    size_t prefix_len = afterVersion - code + 1;
    strncpy(newShader, code, prefix_len);
    newShader[prefix_len] = '\0';

    // Add the `#define` statements
    for (int i = 0; i < count; i++) {
        strcat(newShader, defines[i]);
        strcat(newShader, "\n");
    }

    // Add the rest of the shader after `#version`
    strcat(newShader, afterVersion + 1);

    return newShader;
}

static const char* r3d_get_internal_format_name(GLenum format)
{
    switch (format) {
        case GL_R8: return "GL_R8";
        case GL_R16F: return "GL_R16F";
        case GL_R32F: return "GL_R32F";
        case GL_RG8: return "GL_RG8";
        case GL_RG16F: return "GL_RG16F";
        case GL_RG32F: return "GL_RG32F";
        case GL_RGB565: return "GL_RGB565";
        case GL_RGB8: return "GL_RGB8";
        case GL_SRGB8: return "GL_SRGB8";
        case GL_RGB12: return "GL_RGB12";
        case GL_RGB16: return "GL_RGB16";
        case GL_RGB9_E5: return "GL_RGB9_E5";
        case GL_R11F_G11F_B10F: return "GL_R11F_G11F_B10F";
        case GL_RGB16F: return "GL_RGB16F";
        case GL_RGB32F: return "GL_RGB32F";
        case GL_RGBA4: return "GL_RGBA4";
        case GL_RGB5_A1: return "GL_RGB5_A1";
        case GL_RGBA8: return "GL_RGBA8";
        case GL_SRGB8_ALPHA8: return "GL_SRGB8_ALPHA8";
        case GL_RGB10_A2: return "GL_RGB10_A2";
        case GL_RGBA12: return "GL_RGBA12";
        case GL_RGBA16: return "GL_RGBA16";
        case GL_RGBA16F: return "GL_RGBA16F";
        case GL_RGBA32F: return "GL_RGBA32F";
        default: return "UNKNOWN";
    }
}


/* === Helper functions === */

// Try to allocate a small texture with a specific internal format.
// Returns true if no GL error occurred during allocation, false otherwise.
static bool r3d_try_internal_format(GLenum internalFormat, GLenum format, GLenum type)
{
    GLuint tex = 0;
    GLenum err = GL_NO_ERROR;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 4, 4, 0, format, type, NULL);
    err = glGetError();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tex);

    return (err == GL_NO_ERROR);
}

// Returns the best format in case of incompatibility
int r3d_texture_get_best_internal_format(int internalFormat)
{
    // Macro to simplify the definition of supports
    #define SUPPORT(fmt) { GL_##fmt, &R3D.support.tex##fmt, #fmt }
    #define END_ALTERNATIVES { GL_NONE, NULL, NULL }

    // Structure for defining format alternatives
    struct format_info {
        GLenum format;
        int* supportFlag;
        const char* name;
    };

    // Structure for defining fallbacks of a format
    struct format_fallback {
        GLenum requested_format;
        struct format_info alternatives[8];
    };
    
    // Table of fallbacks for each format
    static const struct format_fallback fallbacks[] =
    {
        // Single Channel Formats
        { GL_R8, {
            SUPPORT(R8),
            END_ALTERNATIVES
        }},
        
        { GL_R16F, {
            SUPPORT(R16F),
            SUPPORT(R32F),
            SUPPORT(R8),
            END_ALTERNATIVES
        }},
        
        { GL_R32F, {
            SUPPORT(R32F),
            SUPPORT(R16F),
            SUPPORT(R8),
            END_ALTERNATIVES
        }},
        
        // Dual Channel Formats
        { GL_RG8, {
            SUPPORT(RG8),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        
        { GL_RG16F, {
            SUPPORT(RG16F),
            SUPPORT(RG32F),
            SUPPORT(RGBA16F),
            SUPPORT(RG8),
            END_ALTERNATIVES
        }},
        
        { GL_RG32F, {
            SUPPORT(RG32F),
            SUPPORT(RG16F),
            SUPPORT(RGBA32F),
            SUPPORT(RG8),
            END_ALTERNATIVES
        }},
        
        // Triple Channel Formats (RGB)
        { GL_RGB565, {
            SUPPORT(RGB565),
            SUPPORT(RGB8),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        
        { GL_RGB8, {
            SUPPORT(RGB8),
            SUPPORT(SRGB8),
            SUPPORT(RGBA8),
            SUPPORT(RGB565),
            END_ALTERNATIVES
        }},
        
        { GL_SRGB8, {
            SUPPORT(SRGB8),
            SUPPORT(RGB8),
            SUPPORT(SRGB8_ALPHA8),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        
        { GL_RGB12, {
            SUPPORT(RGB12),
            SUPPORT(RGB16),
            SUPPORT(RGBA12),
            SUPPORT(RGB8),
            END_ALTERNATIVES
        }},
        
        { GL_RGB16, {
            SUPPORT(RGB16),
            SUPPORT(RGB12),
            SUPPORT(RGBA16),
            SUPPORT(RGB8),
            END_ALTERNATIVES
        }},
        
        { GL_RGB9_E5, {
            SUPPORT(RGB9_E5),
            SUPPORT(R11F_G11F_B10F),
            SUPPORT(RGB16F),
            SUPPORT(RGB32F),
            END_ALTERNATIVES
        }},
        
        { GL_R11F_G11F_B10F, {
            SUPPORT(R11F_G11F_B10F),
            SUPPORT(RGB9_E5),
            SUPPORT(RGB16F),
            SUPPORT(RGB32F),
            END_ALTERNATIVES
        }},
        
        { GL_RGB16F, {
            SUPPORT(RGB16F),
            SUPPORT(RGB32F),
            SUPPORT(RGBA16F),
            SUPPORT(R11F_G11F_B10F),
            SUPPORT(RGB9_E5),
            END_ALTERNATIVES
        }},
        
        { GL_RGB32F, {
            SUPPORT(RGB32F),
            SUPPORT(RGB16F),
            SUPPORT(RGBA32F),
            SUPPORT(R11F_G11F_B10F),
            END_ALTERNATIVES
        }},
        
        // Quad Channel Formats (RGBA)
        { GL_RGBA4, {
            SUPPORT(RGBA4),
            SUPPORT(RGB5_A1),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        
        { GL_RGB5_A1, {
            SUPPORT(RGB5_A1),
            SUPPORT(RGBA4),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        
        { GL_RGBA8, {
            SUPPORT(RGBA8),
            SUPPORT(SRGB8_ALPHA8),
            SUPPORT(RGB10_A2),
            SUPPORT(RGB5_A1),
            END_ALTERNATIVES
        }},
        
        { GL_SRGB8_ALPHA8, {
            SUPPORT(SRGB8_ALPHA8),
            SUPPORT(RGBA8),
            SUPPORT(SRGB8),
            END_ALTERNATIVES
        }},
        
        { GL_RGB10_A2, {
            SUPPORT(RGB10_A2),
            SUPPORT(RGBA16),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        
        { GL_RGBA12, {
            SUPPORT(RGBA12),
            SUPPORT(RGBA16),
            SUPPORT(RGB10_A2),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        
        { GL_RGBA16, {
            SUPPORT(RGBA16),
            SUPPORT(RGBA12),
            SUPPORT(RGB10_A2),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        
        { GL_RGBA16F, {
            SUPPORT(RGBA16F),
            SUPPORT(RGBA32F),
            SUPPORT(RGB16F),
            SUPPORT(RGB10_A2),
            END_ALTERNATIVES
        }},
        
        { GL_RGBA32F, {
            SUPPORT(RGBA32F),
            SUPPORT(RGBA16F),
            SUPPORT(RGB32F),
            SUPPORT(RGB10_A2),
            END_ALTERNATIVES
        }},
        
        // Sentinel
        { GL_NONE, { END_ALTERNATIVES } }
    };
    
    // Search for format in table
    for (const struct format_fallback* fallback = fallbacks; fallback->requested_format != GL_NONE; fallback++) {
        if (fallback->requested_format == internalFormat) {
            // Test each alternative in order
            for (int i = 0; fallback->alternatives[i].format != GL_NONE; i++) {
                const struct format_info* alt = &fallback->alternatives[i];
                if (*(alt->supportFlag)) {
                    // Log if this is not the exact format requested
                    if (i > 0) {
                        TraceLog(LOG_WARNING, "R3D: %s not supported, using %s instead", r3d_get_internal_format_name(internalFormat), alt->name);
                    }
                    return alt->format;
                }
            }
            
            // No alternatives found
            TraceLog(LOG_FATAL, "R3D: Texture format [0x%04x] is not supported and no fallback could be found", internalFormat);
            return GL_NONE;
        }
    }
    
    // Unknown format...
    assert(false && "Unknown or unsupported texture format requested");
    return GL_NONE;
    
    #undef SUPPORT
    #undef END_ALTERNATIVES
}

bool r3d_texture_is_default(unsigned int id)
{
    for (int i = 0; i < sizeof(R3D.texture) / sizeof(unsigned int); i++) {
        if (id == ((unsigned int*)(&R3D.texture))[i]) {
            return true;
        }
    }

    return false;
}

void r3d_calculate_bloom_prefilter_data()
{
    float knee = R3D.env.bloomThreshold * R3D.env.bloomSoftThreshold;
    R3D.env.bloomPrefilter.x = R3D.env.bloomThreshold;
    R3D.env.bloomPrefilter.y = R3D.env.bloomPrefilter.x - knee;
    R3D.env.bloomPrefilter.z = 2.0f * knee;
    R3D.env.bloomPrefilter.w = 0.25f / (knee + 0.00001f);
}


/* === Main loading functions === */

void r3d_support_check_texture_internal_formats(void)
{
    memset(&R3D.support, 0, sizeof(R3D.support));

    // Prefer the modern internalformat query when available (GL 4.2+ or ARB_internalformat_query).
    // On macOS OpenGL 4.1 this query is not available and returns false, so we fall back to probing.
#if defined(GLAD_GL_VERSION_4_2)
    if (GLAD_GL_VERSION_4_2) {
        struct {
            GLenum format;
            int* outFlag;
            const char* name;
        } formats[] = {
            // Single Channel Formats
            { GL_R8, &R3D.support.texR8, "R8" },
            { GL_R16F, &R3D.support.texR16F, "R16F" },
            { GL_R32F, &R3D.support.texR32F, "R32F" },

            // Dual Channel Formats
            { GL_RG8, &R3D.support.texRG8, "RG8" },
            { GL_RG16F, &R3D.support.texRG16F, "RG16F" },
            { GL_RG32F, &R3D.support.texRG32F, "RG32F" },

            // Triple Channel Formats (RGB)
            { GL_RGB565, &R3D.support.texRGB565, "RGB565" },
            { GL_RGB8, &R3D.support.texRGB8, "RGB8" },
            { GL_SRGB8, &R3D.support.texSRGB8, "SRGB8" },
            { GL_RGB12, &R3D.support.texRGB12, "RGB12" },
            { GL_RGB16, &R3D.support.texRGB16, "RGB16" },
            { GL_RGB9_E5, &R3D.support.texRGB9_E5, "RGB9_E5" },
            { GL_R11F_G11F_B10F, &R3D.support.texR11F_G11F_B10F, "R11F_G11F_B10F" },
            { GL_RGB16F, &R3D.support.texRGB16F, "RGB16F" },
            { GL_RGB32F, &R3D.support.texRGB32F, "RGB32F" },

            // Quad Channel Formats (RGBA)
            { GL_RGBA4, &R3D.support.texRGBA4, "RGBA4" },
            { GL_RGB5_A1, &R3D.support.texRGB5_A1, "RGB5_A1" },
            { GL_RGBA8, &R3D.support.texRGBA8, "RGBA8" },
            { GL_SRGB8_ALPHA8, &R3D.support.texSRGB8_ALPHA8, "SRGB8_ALPHA8" },
            { GL_RGB10_A2, &R3D.support.texRGB10_A2, "RGB10_A2" },
            { GL_RGBA12, &R3D.support.texRGBA12, "RGBA12" },
            { GL_RGBA16, &R3D.support.texRGBA16, "RGBA16" },
            { GL_RGBA16F, &R3D.support.texRGBA16F, "RGBA16F" },
            { GL_RGBA32F, &R3D.support.texRGBA32F, "RGBA32F" },
        };

        for (int i = 0; i < (int)(sizeof(formats)/sizeof(formats[0])); ++i) {
            glGetInternalformativ(GL_TEXTURE_2D, formats[i].format, GL_INTERNALFORMAT_SUPPORTED, 1, formats[i].outFlag);
            if (*formats[i].outFlag) {
                TraceLog(LOG_INFO, "R3D: Texture format %s is supported", formats[i].name);
            } else {
                TraceLog(LOG_WARNING, "R3D: Texture format %s is NOT supported", formats[i].name);
            }
        }
        return;
    }
#endif

    // Fallback probing path (works on macOS OpenGL 4.1): try to allocate a small texture for each format
    struct probe {
        GLenum internal;
        GLenum format;
        GLenum type;
        int* outFlag;
        const char* name;
    } probes[] = {
        // Single Channel Formats
        { GL_R8,                 GL_RED,   GL_UNSIGNED_BYTE,                &R3D.support.texR8,              "R8" },
        { GL_R16F,               GL_RED,   GL_HALF_FLOAT,                   &R3D.support.texR16F,            "R16F" },
        { GL_R32F,               GL_RED,   GL_FLOAT,                        &R3D.support.texR32F,            "R32F" },

        // Dual Channel Formats
        { GL_RG8,                GL_RG,    GL_UNSIGNED_BYTE,                &R3D.support.texRG8,             "RG8" },
        { GL_RG16F,              GL_RG,    GL_HALF_FLOAT,                   &R3D.support.texRG16F,           "RG16F" },
        { GL_RG32F,              GL_RG,    GL_FLOAT,                        &R3D.support.texRG32F,           "RG32F" },

        // Triple Channel Formats (RGB)
        { GL_RGB565,             GL_RGB,   GL_UNSIGNED_SHORT_5_6_5,         &R3D.support.texRGB565,          "RGB565" },
        { GL_RGB8,               GL_RGB,   GL_UNSIGNED_BYTE,                &R3D.support.texRGB8,            "RGB8" },
        { GL_SRGB8,              GL_RGB,   GL_UNSIGNED_BYTE,                &R3D.support.texSRGB8,           "SRGB8" },
        { GL_RGB12,              GL_RGB,   GL_UNSIGNED_BYTE,                &R3D.support.texRGB12,           "RGB12" },
        { GL_RGB16,              GL_RGB,   GL_UNSIGNED_BYTE,                &R3D.support.texRGB16,           "RGB16" },
        { GL_RGB9_E5,            GL_RGB,   GL_UNSIGNED_INT_5_9_9_9_REV,     &R3D.support.texRGB9_E5,         "RGB9_E5" },
        { GL_R11F_G11F_B10F,     GL_RGB,   GL_UNSIGNED_INT_10F_11F_11F_REV, &R3D.support.texR11F_G11F_B10F,  "R11F_G11F_B10F" },
        { GL_RGB16F,             GL_RGB,   GL_HALF_FLOAT,                   &R3D.support.texRGB16F,          "RGB16F" },
        { GL_RGB32F,             GL_RGB,   GL_FLOAT,                        &R3D.support.texRGB32F,          "RGB32F" },

        // Quad Channel Formats (RGBA)
        { GL_RGBA4,              GL_RGBA,  GL_UNSIGNED_SHORT_4_4_4_4,       &R3D.support.texRGBA4,           "RGBA4" },
        { GL_RGB5_A1,            GL_RGBA,  GL_UNSIGNED_SHORT_5_5_5_1,       &R3D.support.texRGB5_A1,         "RGB5_A1" },
        { GL_RGBA8,              GL_RGBA,  GL_UNSIGNED_BYTE,                &R3D.support.texRGBA8,           "RGBA8" },
        { GL_SRGB8_ALPHA8,       GL_RGBA,  GL_UNSIGNED_BYTE,                &R3D.support.texSRGB8_ALPHA8,    "SRGB8_ALPHA8" },
        { GL_RGB10_A2,           GL_RGBA,  GL_UNSIGNED_INT_10_10_10_2,      &R3D.support.texRGB10_A2,        "RGB10_A2" },
        { GL_RGBA12,             GL_RGBA,  GL_UNSIGNED_BYTE,                &R3D.support.texRGBA12,          "RGBA12" },
        { GL_RGBA16,             GL_RGBA,  GL_UNSIGNED_BYTE,                &R3D.support.texRGBA16,          "RGBA16" },
        { GL_RGBA16F,            GL_RGBA,  GL_HALF_FLOAT,                   &R3D.support.texRGBA16F,         "RGBA16F" },
        { GL_RGBA32F,            GL_RGBA,  GL_FLOAT,                        &R3D.support.texRGBA32F,         "RGBA32F" },
    };

    for (int i = 0; i < (int)(sizeof(probes)/sizeof(probes[0])); ++i) {
        *probes[i].outFlag = r3d_try_internal_format(probes[i].internal, probes[i].format, probes[i].type);
        if (*probes[i].outFlag) {
            TraceLog(LOG_INFO, "R3D: Texture format %s is supported", probes[i].name);
        } else {
            TraceLog(LOG_WARNING, "R3D: Texture format %s is NOT supported", probes[i].name);
        }
    }
}

void r3d_framebuffers_load(int width, int height)
{
    r3d_framebuffer_load_gbuffer(width, height);
    r3d_framebuffer_load_deferred(width, height);
    r3d_framebuffer_load_pingpong(width, height);

    if (R3D.env.ssaoEnabled) {
        r3d_framebuffer_load_pingpong_ssao(width, height);
    }

    if (R3D.env.bloomMode != R3D_BLOOM_DISABLED) {
        r3d_framebuffer_load_mipchain_bloom(width, height);
    }
}

void r3d_framebuffers_unload(void)
{
    r3d_framebuffer_unload_gbuffer();
    r3d_framebuffer_unload_deferred();
    r3d_framebuffer_unload_pingpong();

    if (R3D.framebuffer.pingPongSSAO.id != 0) {
        r3d_framebuffer_unload_pingpong_ssao();
    }

    if (R3D.framebuffer.mipChainBloom.id != 0) {
        r3d_framebuffer_unload_mipchain_bloom();
    }
}

void r3d_textures_load(void)
{
    r3d_texture_load_white();
    r3d_texture_load_black();
    r3d_texture_load_normal();
    r3d_texture_load_blue_noise();
    r3d_texture_load_ibl_brdf_lut();

    if (R3D.env.ssaoEnabled) {
        r3d_texture_load_ssao_noise();
        r3d_texture_load_ssao_kernel();
    }
}

void r3d_textures_unload(void)
{
    rlUnloadTexture(R3D.texture.white);
    rlUnloadTexture(R3D.texture.black);
    rlUnloadTexture(R3D.texture.normal);
    rlUnloadTexture(R3D.texture.blueNoise);
    rlUnloadTexture(R3D.texture.iblBrdfLut);

    if (R3D.texture.ssaoNoise != 0) {
        rlUnloadTexture(R3D.texture.ssaoNoise);
    }

    if (R3D.texture.ssaoKernel != 0) {
        rlUnloadTexture(R3D.texture.ssaoKernel);
    }
}

void r3d_shaders_load(void)
{
    /* --- Generation shader passes --- */

    r3d_shader_load_generate_cubemap_from_equirectangular();
    r3d_shader_load_generate_irradiance_convolution();
    r3d_shader_load_generate_prefilter();

    /* --- Scene shader passes --- */

    r3d_shader_load_raster_geometry();
    r3d_shader_load_raster_geometry_inst();
    r3d_shader_load_raster_forward();
    r3d_shader_load_raster_forward_inst();
    r3d_shader_load_raster_skybox();
    r3d_shader_load_raster_depth_volume();
    r3d_shader_load_raster_depth();
    r3d_shader_load_raster_depth_inst();
    r3d_shader_load_raster_depth_cube();
    r3d_shader_load_raster_depth_cube_inst();

    /* --- Screen shader passes --- */

    r3d_shader_load_screen_ambient_ibl();
    r3d_shader_load_screen_ambient();
    r3d_shader_load_screen_lighting();
    r3d_shader_load_screen_scene();

    // NOTE: Don't load the output shader here to avoid keeping an unused tonemap mode
    //       in memory in case the tonemap mode changes after initialization.
    //       It is loaded on demand during `R3D_End()`

    // TODO: Revisit the shader loading mechanism. Constantly checking and loading
    //       it during `R3D_End()` doesn't feel like the cleanest approach

    //r3d_shader_load_screen_output(R3D.env.tonemapMode);

    /* --- Additional screen shader passes --- */

    if (R3D.env.ssaoEnabled) {
        r3d_shader_load_generate_gaussian_blur_dual_pass();
        r3d_shader_load_screen_ssao();
    }
    if (R3D.env.bloomMode != R3D_BLOOM_DISABLED) {
        r3d_shader_load_generate_downsampling();
        r3d_shader_load_generate_upsampling();
        r3d_shader_load_screen_bloom();
    }
    if (R3D.env.fogMode != R3D_FOG_DISABLED) {
        r3d_shader_load_screen_fog();
    }
    if (R3D.env.dofMode != R3D_DOF_DISABLED) {
        r3d_shader_load_screen_dof();
    }
    if (R3D.state.flags & R3D_FLAG_FXAA) {
        r3d_shader_load_screen_fxaa();
    }
}

void r3d_shaders_unload(void)
{
    // Unload generation shaders
    rlUnloadShaderProgram(R3D.shader.generate.gaussianBlurDualPass.id);
    rlUnloadShaderProgram(R3D.shader.generate.cubemapFromEquirectangular.id);
    rlUnloadShaderProgram(R3D.shader.generate.irradianceConvolution.id);
    rlUnloadShaderProgram(R3D.shader.generate.prefilter.id);

    // Unload raster shaders
    rlUnloadShaderProgram(R3D.shader.raster.geometry.id);
    rlUnloadShaderProgram(R3D.shader.raster.geometryInst.id);
    rlUnloadShaderProgram(R3D.shader.raster.forward.id);
    rlUnloadShaderProgram(R3D.shader.raster.forwardInst.id);
    rlUnloadShaderProgram(R3D.shader.raster.skybox.id);
    rlUnloadShaderProgram(R3D.shader.raster.depthVolume.id);
    rlUnloadShaderProgram(R3D.shader.raster.depth.id);
    rlUnloadShaderProgram(R3D.shader.raster.depthInst.id);
    rlUnloadShaderProgram(R3D.shader.raster.depthCube.id);
    rlUnloadShaderProgram(R3D.shader.raster.depthCubeInst.id);

    // Unload screen shaders
    rlUnloadShaderProgram(R3D.shader.screen.ambientIbl.id);
    rlUnloadShaderProgram(R3D.shader.screen.ambient.id);
    rlUnloadShaderProgram(R3D.shader.screen.lighting.id);
    rlUnloadShaderProgram(R3D.shader.screen.scene.id);

    for (int i = 0; i < R3D_TONEMAP_COUNT; i++) {
        if (R3D.shader.screen.output[i].id != 0) {
            rlUnloadShaderProgram(R3D.shader.screen.output[i].id);
        }
    }

    if (R3D.shader.screen.ssao.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.ssao.id);
    }
    if (R3D.shader.screen.bloom.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.bloom.id);
    }
    if (R3D.shader.screen.fog.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.fog.id);
    }
    if (R3D.shader.screen.dof.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.dof.id);
    }
    if (R3D.shader.screen.fxaa.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.fxaa.id);
    }
}

void r3d_shader_load_screen_dof(void)
{
    R3D.shader.screen.dof.id = rlLoadShaderCode(
        SCREEN_VERT, DOF_FRAG
    );

    r3d_shader_get_location(screen.dof, uTexColor);
    r3d_shader_get_location(screen.dof, uTexDepth);
    r3d_shader_get_location(screen.dof, uNear);
    r3d_shader_get_location(screen.dof, uFar);
    r3d_shader_get_location(screen.dof, uFocusPoint);
    r3d_shader_get_location(screen.dof, uFocusScale);
    r3d_shader_get_location(screen.dof, uMaxBlurSize);
    r3d_shader_get_location(screen.dof, uDebugMode);

    r3d_shader_enable(screen.dof);
    r3d_shader_set_sampler2D_slot(screen.dof, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(screen.dof, uTexDepth, 1);
    r3d_shader_disable();
}

/* === Framebuffer loading functions === */

void r3d_framebuffer_load_gbuffer(int width, int height)
{
    struct r3d_fb_gbuffer* gBuffer = &R3D.framebuffer.gBuffer;

    gBuffer->id = rlLoadFramebuffer();
    if (gBuffer->id == 0) {
        TraceLog(LOG_FATAL, "R3D: Failed to create G-Buffer");
        return;
    }

    rlEnableFramebuffer(gBuffer->id);

    // Determines the HDR color buffers precision
    GLenum hdrFormat = (R3D.state.flags & R3D_FLAG_LOW_PRECISION_BUFFERS)
        ? GL_R11F_G11F_B10F : GL_RGB16F;

    // Generate (albedo / orm) buffers
    gBuffer->albedo = rlLoadTexture(NULL, width, height, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
    gBuffer->orm = rlLoadTexture(NULL, width, height, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);

    // Generate emission buffer
    glGenTextures(1, &gBuffer->emission);
    glBindTexture(GL_TEXTURE_2D, gBuffer->emission);
    glTexImage2D(GL_TEXTURE_2D, 0, r3d_texture_get_best_internal_format(hdrFormat), width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Generate normal buffer
    // Normals will be encoded and decoded using octahedral mapping
    glGenTextures(1, &gBuffer->normal);
    glBindTexture(GL_TEXTURE_2D, gBuffer->normal);
    if ((R3D.state.flags & R3D_FLAG_8_BIT_NORMALS) || (R3D.support.texRG16F == false)) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
    }
    else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, NULL);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Generate depth stencil texture
    glGenTextures(1, &gBuffer->depth);
    glBindTexture(GL_TEXTURE_2D, gBuffer->depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Unbind last texture
    glBindTexture(GL_TEXTURE_2D, 0);

    // Activate the draw buffers for all the attachments
    rlActiveDrawBuffers(R3D_GBUFFER_COUNT);

    // Attach the textures to the framebuffer
    rlFramebufferAttach(gBuffer->id, gBuffer->albedo, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(gBuffer->id, gBuffer->emission, RL_ATTACHMENT_COLOR_CHANNEL1, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(gBuffer->id, gBuffer->normal, RL_ATTACHMENT_COLOR_CHANNEL2, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(gBuffer->id, gBuffer->orm, RL_ATTACHMENT_COLOR_CHANNEL3, RL_ATTACHMENT_TEXTURE2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer->id); // rlFramebufferAttach unbind the framebuffer...
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, gBuffer->depth, 0);

    // Check if the framebuffer is complete
    if (!rlFramebufferComplete(gBuffer->id)) {
        TraceLog(LOG_WARNING, "R3D: The G-Buffer is not complete");
    }
}

void r3d_framebuffer_load_pingpong_ssao(int width, int height)
{
    struct r3d_fb_pingpong_ssao* ssao = &R3D.framebuffer.pingPongSSAO;

    width /= 2, height /= 2; // Half resolution

    ssao->id = rlLoadFramebuffer();
    if (ssao->id == 0) {
        TraceLog(LOG_FATAL, "R3D: Failed to create the SSAO ping-pong buffer");
        return;
    }

    rlEnableFramebuffer(ssao->id);

    // Generate (ssao) buffers
    GLuint textures[2];
    glGenTextures(2, textures);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    ssao->target = textures[0];
    ssao->source = textures[1];

    // Activate the draw buffers for all the attachments
    rlActiveDrawBuffers(1);

    // Attach the textures to the framebuffer
    rlFramebufferAttach(ssao->id, ssao->target, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);

    // Check if the framebuffer is complete
    if (!rlFramebufferComplete(ssao->id)) {
        TraceLog(LOG_WARNING, "R3D: The SSAO ping-pong buffer is not complete");
    }
}

void r3d_framebuffer_load_deferred(int width, int height)
{
    struct r3d_fb_deferred* deferred = &R3D.framebuffer.deferred;

    deferred->id = rlLoadFramebuffer();
    if (deferred->id == 0) {
        TraceLog(LOG_FATAL, "R3D: Failed to create the deferred pass framebuffer");
        return;
    }

    rlEnableFramebuffer(deferred->id);

    // Generate diffuse/specular textures
    GLuint textures[2];
    glGenTextures(2, textures);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, r3d_texture_get_best_internal_format(GL_RGB16F), width, height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    deferred->diffuse = textures[0];
    deferred->specular = textures[1];

    // Activate the draw buffers for all the attachments
    rlActiveDrawBuffers(2);

    // Attach the textures to the framebuffer
    rlFramebufferAttach(deferred->id, deferred->diffuse, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(deferred->id, deferred->specular, RL_ATTACHMENT_COLOR_CHANNEL1, RL_ATTACHMENT_TEXTURE2D, 0);

    // Check if the framebuffer is complete
    if (!rlFramebufferComplete(deferred->id)) {
        TraceLog(LOG_WARNING, "R3D: The deferred pass framebuffer is not complete");
    }
}

void r3d_framebuffer_load_mipchain_bloom(int width, int height)
{
    struct r3d_fb_mipchain_bloom* bloom = &R3D.framebuffer.mipChainBloom;

    width /= 2, height /= 2; // Half resolution

    glGenFramebuffers(1, &bloom->id);
    if (bloom->id == 0) {
        TraceLog(LOG_FATAL, "R3D: Failed to create the bloom mipchain framebuffer");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, bloom->id);

    // Determines the HDR color buffers precision
    GLenum hdrFormat = (R3D.state.flags & R3D_FLAG_LOW_PRECISION_BUFFERS)
        ? GL_R11F_G11F_B10F : GL_RGB16F;

    // Minimum mip size to avoid tiny levels
    const int minSize = 8;

    // Calculate max mip levels based on smallest dimension
    int minDimension = (width < height) ? width : height;
    int maxMipChainLength = (int)floor(log2((float)minDimension));

    // Determine mip chain length stopping at minSize
    int mipChainLength = 0;
    for (; mipChainLength < maxMipChainLength; mipChainLength++) {
        if ((minDimension >> mipChainLength) < minSize) break;
    }

    // Allocate the array containing the mipmaps
    bloom->mipChain = MemAlloc(mipChainLength * sizeof(struct r3d_mip_bloom));
    if (bloom->mipChain == NULL) {
        TraceLog(LOG_ERROR, "R3D: Failed to allocate memory to store bloom mip chain");
        return;
    }
    bloom->mipCount = mipChainLength;

    // Dynamic value copy
    uint32_t wMip = (uint32_t)width;
    uint32_t hMip = (uint32_t)height;

    // Create the mip chain
    for (GLuint i = 0; i < mipChainLength; i++, wMip /= 2, hMip /= 2)
    {
        struct r3d_mip_bloom* mip = &bloom->mipChain[i];

        mip->w = wMip;
        mip->h = hMip;
        mip->tx = 1.0f / (float)wMip;
        mip->ty = 1.0f / (float)hMip;

        glGenTextures(1, &mip->id);
        glBindTexture(GL_TEXTURE_2D, mip->id);
        glTexImage2D(GL_TEXTURE_2D, 0, r3d_texture_get_best_internal_format(hdrFormat), wMip, hMip, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Attach first mip to the framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom->mipChain[0].id, 0);

    GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_WARNING, "R3D: The bloom mipchain framebuffer is not complete");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void r3d_framebuffer_load_pingpong(int width, int height)
{
    struct r3d_fb_pingpong* pingPong = &R3D.framebuffer.pingPong;

    pingPong->id = rlLoadFramebuffer();
    if (pingPong->id == 0) {
        TraceLog(LOG_FATAL, "R3D: Failed to create the final ping-pong framebuffer");
        return;
    }

    rlEnableFramebuffer(pingPong->id);

    // Determines the HDR color buffers precision
    GLenum hdrFormat = (R3D.state.flags & R3D_FLAG_LOW_PRECISION_BUFFERS)
        ? GL_R11F_G11F_B10F : GL_RGB16F;

    // Generate (color) buffers
    GLuint textures[2];
    glGenTextures(2, textures);
    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, r3d_texture_get_best_internal_format(hdrFormat), width, height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    pingPong->target = textures[0];
    pingPong->source = textures[1];

    // Activate the draw buffers for all the attachments
    rlActiveDrawBuffers(1);

    // Attach the textures to the framebuffer
    rlFramebufferAttach(pingPong->id, pingPong->target, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);

    // Check if the framebuffer is complete
    if (!rlFramebufferComplete(pingPong->id)) {
        TraceLog(LOG_WARNING, "R3D: The final ping-pong framebuffer is not complete");
    }
}

void r3d_framebuffer_unload_gbuffer(void)
{
    struct r3d_fb_gbuffer* gBuffer = &R3D.framebuffer.gBuffer;

    rlUnloadTexture(gBuffer->albedo);
    rlUnloadTexture(gBuffer->emission);
    rlUnloadTexture(gBuffer->normal);
    rlUnloadTexture(gBuffer->orm);
    rlUnloadTexture(gBuffer->depth);

    rlUnloadFramebuffer(gBuffer->id);

    memset(gBuffer, 0, sizeof(struct r3d_fb_gbuffer));
}

void r3d_framebuffer_unload_pingpong_ssao(void)
{
    struct r3d_fb_pingpong_ssao* ssao = &R3D.framebuffer.pingPongSSAO;

    rlUnloadTexture(ssao->source);
    rlUnloadTexture(ssao->target);

    rlUnloadFramebuffer(ssao->id);

    memset(ssao, 0, sizeof(struct r3d_fb_pingpong_ssao));
}

void r3d_framebuffer_unload_deferred(void)
{
    struct r3d_fb_deferred* deferred = &R3D.framebuffer.deferred;

    rlUnloadTexture(deferred->diffuse);
    rlUnloadTexture(deferred->specular);

    rlUnloadFramebuffer(deferred->id);

    memset(deferred, 0, sizeof(struct r3d_fb_deferred));
}

void r3d_framebuffer_unload_mipchain_bloom(void)
{
    struct r3d_fb_mipchain_bloom* bloom = &R3D.framebuffer.mipChainBloom;

    for (int i = 0; i < bloom->mipCount; i++) {
        glDeleteTextures(1, &bloom->mipChain[i].id);
    }
    glDeleteFramebuffers(1, &bloom->id);

    MemFree(bloom->mipChain);

    bloom->mipChain = NULL;
    bloom->mipCount = 0;
    bloom->id = 0;
}

void r3d_framebuffer_unload_pingpong(void)
{
    struct r3d_fb_pingpong* pingPong = &R3D.framebuffer.pingPong;

    rlUnloadTexture(pingPong->source);
    rlUnloadTexture(pingPong->target);

    rlUnloadFramebuffer(pingPong->id);

    memset(pingPong, 0, sizeof(struct r3d_fb_pingpong));
}


/* === Shader loading functions === */

void r3d_shader_load_generate_gaussian_blur_dual_pass(void)
{
    R3D.shader.generate.gaussianBlurDualPass.id = rlLoadShaderCode(
        SCREEN_VERT, GAUSSIAN_BLUR_DUAL_PASS_FRAG
    );

    r3d_shader_get_location(generate.gaussianBlurDualPass, uTexture);
    r3d_shader_get_location(generate.gaussianBlurDualPass, uTexelDir);

    r3d_shader_enable(generate.gaussianBlurDualPass);
    r3d_shader_set_sampler2D_slot(generate.gaussianBlurDualPass, uTexture, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_downsampling(void)
{
    R3D.shader.generate.downsampling.id = rlLoadShaderCode(
        SCREEN_VERT, DOWNSAMPLING_FRAG
    );

    r3d_shader_get_location(generate.downsampling, uTexture);
    r3d_shader_get_location(generate.downsampling, uTexelSize);
    r3d_shader_get_location(generate.downsampling, uMipLevel);
    r3d_shader_get_location(generate.downsampling, uPrefilter);

    r3d_shader_enable(generate.downsampling);
    r3d_shader_set_sampler2D_slot(generate.downsampling, uTexture, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_upsampling(void)
{
    R3D.shader.generate.upsampling.id = rlLoadShaderCode(
        SCREEN_VERT, UPSAMPLING_FRAG
    );

    r3d_shader_get_location(generate.upsampling, uTexture);
    r3d_shader_get_location(generate.upsampling, uFilterRadius);

    r3d_shader_enable(generate.upsampling);
    r3d_shader_set_sampler2D_slot(generate.upsampling, uTexture, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_cubemap_from_equirectangular(void)
{
    R3D.shader.generate.cubemapFromEquirectangular.id = rlLoadShaderCode(
        CUBEMAP_VERT, CUBEMAP_FROM_EQUIRECTANGULAR_FRAG
    );

    r3d_shader_get_location(generate.cubemapFromEquirectangular, uMatProj);
    r3d_shader_get_location(generate.cubemapFromEquirectangular, uMatView);
    r3d_shader_get_location(generate.cubemapFromEquirectangular, uTexEquirectangular);

    r3d_shader_enable(generate.cubemapFromEquirectangular);
    r3d_shader_set_sampler2D_slot(generate.cubemapFromEquirectangular, uTexEquirectangular, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_irradiance_convolution(void)
{
    R3D.shader.generate.irradianceConvolution.id = rlLoadShaderCode(
        CUBEMAP_VERT, IRRADIANCE_CONVOLUTION_FRAG
    );

    r3d_shader_get_location(generate.irradianceConvolution, uMatProj);
    r3d_shader_get_location(generate.irradianceConvolution, uMatView);
    r3d_shader_get_location(generate.irradianceConvolution, uCubemap);

    r3d_shader_enable(generate.irradianceConvolution);
    r3d_shader_set_samplerCube_slot(generate.irradianceConvolution, uCubemap, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_prefilter(void)
{
    R3D.shader.generate.prefilter.id = rlLoadShaderCode(
        CUBEMAP_VERT, PREFILTER_FRAG
    );

    r3d_shader_get_location(generate.prefilter, uMatProj);
    r3d_shader_get_location(generate.prefilter, uMatView);
    r3d_shader_get_location(generate.prefilter, uCubemap);
    r3d_shader_get_location(generate.prefilter, uRoughness);

    r3d_shader_enable(generate.prefilter);
    r3d_shader_set_samplerCube_slot(generate.prefilter, uCubemap, 0);
    r3d_shader_disable();
}

void r3d_shader_load_raster_geometry(void)
{
    R3D.shader.raster.geometry.id = rlLoadShaderCode(
        GEOMETRY_VERT, GEOMETRY_FRAG
    );

    for (int i = 0; i < R3D_SHADER_MAX_BONES; i++) {
        R3D.shader.raster.geometry.uBoneMatrices[i].loc = rlGetLocationUniform(
            R3D.shader.raster.geometry.id, TextFormat("uBoneMatrices[%i]", i)
        );
    }

    r3d_shader_get_location(raster.geometry, uUseSkinning);
    r3d_shader_get_location(raster.geometry, uMatNormal);
    r3d_shader_get_location(raster.geometry, uMatModel);
    r3d_shader_get_location(raster.geometry, uMatMVP);
    r3d_shader_get_location(raster.geometry, uTexCoordOffset);
    r3d_shader_get_location(raster.geometry, uTexCoordScale);
    r3d_shader_get_location(raster.geometry, uTexAlbedo);
    r3d_shader_get_location(raster.geometry, uTexNormal);
    r3d_shader_get_location(raster.geometry, uTexEmission);
    r3d_shader_get_location(raster.geometry, uTexORM);
    r3d_shader_get_location(raster.geometry, uEmissionEnergy);
    r3d_shader_get_location(raster.geometry, uNormalScale);
    r3d_shader_get_location(raster.geometry, uOcclusion);
    r3d_shader_get_location(raster.geometry, uRoughness);
    r3d_shader_get_location(raster.geometry, uMetalness);
    r3d_shader_get_location(raster.geometry, uAlbedoColor);
    r3d_shader_get_location(raster.geometry, uEmissionColor);

    r3d_shader_enable(raster.geometry);
    r3d_shader_set_sampler2D_slot(raster.geometry, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(raster.geometry, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(raster.geometry, uTexEmission, 2);
    r3d_shader_set_sampler2D_slot(raster.geometry, uTexORM, 3);
    r3d_shader_disable();
}

void r3d_shader_load_raster_geometry_inst(void)
{
    R3D.shader.raster.geometryInst.id = rlLoadShaderCode(
        GEOMETRY_INSTANCED_VERT, GEOMETRY_FRAG
    );

    for (int i = 0; i < R3D_SHADER_MAX_BONES; i++) {
        R3D.shader.raster.geometryInst.uBoneMatrices[i].loc = rlGetLocationUniform(
            R3D.shader.raster.geometryInst.id, TextFormat("uBoneMatrices[%i]", i)
        );
    }

    r3d_shader_get_location(raster.geometryInst, uUseSkinning);
    r3d_shader_get_location(raster.geometryInst, uMatInvView);
    r3d_shader_get_location(raster.geometryInst, uMatModel);
    r3d_shader_get_location(raster.geometryInst, uMatVP);
    r3d_shader_get_location(raster.geometryInst, uTexCoordOffset);
    r3d_shader_get_location(raster.geometryInst, uTexCoordScale);
    r3d_shader_get_location(raster.geometryInst, uBillboardMode);
    r3d_shader_get_location(raster.geometryInst, uTexAlbedo);
    r3d_shader_get_location(raster.geometryInst, uTexNormal);
    r3d_shader_get_location(raster.geometryInst, uTexEmission);
    r3d_shader_get_location(raster.geometryInst, uTexORM);
    r3d_shader_get_location(raster.geometryInst, uEmissionEnergy);
    r3d_shader_get_location(raster.geometryInst, uNormalScale);
    r3d_shader_get_location(raster.geometryInst, uOcclusion);
    r3d_shader_get_location(raster.geometryInst, uRoughness);
    r3d_shader_get_location(raster.geometryInst, uMetalness);
    r3d_shader_get_location(raster.geometryInst, uAlbedoColor);
    r3d_shader_get_location(raster.geometryInst, uEmissionColor);

    r3d_shader_enable(raster.geometryInst);
    r3d_shader_set_sampler2D_slot(raster.geometryInst, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(raster.geometryInst, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(raster.geometryInst, uTexEmission, 2);
    r3d_shader_set_sampler2D_slot(raster.geometryInst, uTexORM, 3);
    r3d_shader_disable();
}

void r3d_shader_load_raster_forward(void)
{
    R3D.shader.raster.forward.id = rlLoadShaderCode(
        FORWARD_VERT, FORWARD_FRAG
    );

    r3d_shader_raster_forward_t* shader = &R3D.shader.raster.forward;

    for (int i = 0; i < R3D_SHADER_MAX_BONES; i++) {
        R3D.shader.raster.forward.uBoneMatrices[i].loc = rlGetLocationUniform(
            R3D.shader.raster.forward.id, TextFormat("uBoneMatrices[%i]", i)
        );
    }

    r3d_shader_get_location(raster.forward, uUseSkinning);
    r3d_shader_get_location(raster.forward, uMatNormal);
    r3d_shader_get_location(raster.forward, uMatModel);
    r3d_shader_get_location(raster.forward, uMatMVP);
    r3d_shader_get_location(raster.forward, uTexCoordOffset);
    r3d_shader_get_location(raster.forward, uTexCoordScale);
    r3d_shader_get_location(raster.forward, uTexAlbedo);
    r3d_shader_get_location(raster.forward, uTexEmission);
    r3d_shader_get_location(raster.forward, uTexNormal);
    r3d_shader_get_location(raster.forward, uTexORM);
    r3d_shader_get_location(raster.forward, uTexNoise);
    r3d_shader_get_location(raster.forward, uEmissionEnergy);
    r3d_shader_get_location(raster.forward, uNormalScale);
    r3d_shader_get_location(raster.forward, uOcclusion);
    r3d_shader_get_location(raster.forward, uRoughness);
    r3d_shader_get_location(raster.forward, uMetalness);
    r3d_shader_get_location(raster.forward, uAmbientColor);
    r3d_shader_get_location(raster.forward, uAlbedoColor);
    r3d_shader_get_location(raster.forward, uEmissionColor);
    r3d_shader_get_location(raster.forward, uCubeIrradiance);
    r3d_shader_get_location(raster.forward, uCubePrefilter);
    r3d_shader_get_location(raster.forward, uTexBrdfLut);
    r3d_shader_get_location(raster.forward, uQuatSkybox);
    r3d_shader_get_location(raster.forward, uHasSkybox);
    r3d_shader_get_location(raster.forward, uSkyboxAmbientIntensity);
    r3d_shader_get_location(raster.forward, uSkyboxReflectIntensity);
    r3d_shader_get_location(raster.forward, uAlphaCutoff);
    r3d_shader_get_location(raster.forward, uViewPosition);

    r3d_shader_enable(raster.forward);

    r3d_shader_set_sampler2D_slot(raster.forward, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexEmission, 1);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexNormal, 2);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexORM, 3);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexNoise, 4);
    r3d_shader_set_samplerCube_slot(raster.forward, uCubeIrradiance, 5);
    r3d_shader_set_samplerCube_slot(raster.forward, uCubePrefilter, 6);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexBrdfLut, 7);

    int shadowMapSlot = 10;
    for (int i = 0; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++) {
        shader->uMatLightVP[i].loc = rlGetLocationUniform(shader->id, TextFormat("uMatLightVP[%i]", i));
        shader->uLights[i].shadowMap.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowMap", i));
        shader->uLights[i].shadowCubemap.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowCubemap", i));
        shader->uLights[i].color.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].color", i));
        shader->uLights[i].position.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].position", i));
        shader->uLights[i].direction.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].direction", i));
        shader->uLights[i].specular.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].specular", i));
        shader->uLights[i].energy.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].energy", i));
        shader->uLights[i].range.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].range", i));
        shader->uLights[i].near.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].near", i));
        shader->uLights[i].far.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].far", i));
        shader->uLights[i].attenuation.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].attenuation", i));
        shader->uLights[i].innerCutOff.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].innerCutOff", i));
        shader->uLights[i].outerCutOff.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].outerCutOff", i));
        shader->uLights[i].shadowSoftness.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowSoftness", i));
        shader->uLights[i].shadowMapTxlSz.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowMapTxlSz", i));
        shader->uLights[i].shadowBias.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowBias", i));
        shader->uLights[i].type.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].type", i));
        shader->uLights[i].enabled.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].enabled", i));
        shader->uLights[i].shadow.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadow", i));

        r3d_shader_set_sampler2D_slot(raster.forward, uLights[i].shadowMap, shadowMapSlot++);
        r3d_shader_set_samplerCube_slot(raster.forward, uLights[i].shadowCubemap, shadowMapSlot++);
    }

    r3d_shader_disable();
}

void r3d_shader_load_raster_forward_inst(void)
{
    R3D.shader.raster.forwardInst.id = rlLoadShaderCode(
        FORWARD_INSTANCED_VERT, FORWARD_FRAG
    );

    r3d_shader_raster_forward_inst_t* shader = &R3D.shader.raster.forwardInst;

    for (int i = 0; i < R3D_SHADER_MAX_BONES; i++) {
        R3D.shader.raster.forwardInst.uBoneMatrices[i].loc = rlGetLocationUniform(
            R3D.shader.raster.forwardInst.id, TextFormat("uBoneMatrices[%i]", i)
        );
    }

    r3d_shader_get_location(raster.forwardInst, uUseSkinning);
    r3d_shader_get_location(raster.forwardInst, uMatInvView);
    r3d_shader_get_location(raster.forwardInst, uMatModel);
    r3d_shader_get_location(raster.forwardInst, uMatVP);
    r3d_shader_get_location(raster.forwardInst, uTexCoordOffset);
    r3d_shader_get_location(raster.forwardInst, uTexCoordScale);
    r3d_shader_get_location(raster.forwardInst, uBillboardMode);
    r3d_shader_get_location(raster.forwardInst, uTexAlbedo);
    r3d_shader_get_location(raster.forwardInst, uTexEmission);
    r3d_shader_get_location(raster.forwardInst, uTexNormal);
    r3d_shader_get_location(raster.forwardInst, uTexORM);
    r3d_shader_get_location(raster.forwardInst, uTexNoise);
    r3d_shader_get_location(raster.forwardInst, uEmissionEnergy);
    r3d_shader_get_location(raster.forwardInst, uNormalScale);
    r3d_shader_get_location(raster.forwardInst, uOcclusion);
    r3d_shader_get_location(raster.forwardInst, uRoughness);
    r3d_shader_get_location(raster.forwardInst, uMetalness);
    r3d_shader_get_location(raster.forwardInst, uAmbientColor);
    r3d_shader_get_location(raster.forwardInst, uAlbedoColor);
    r3d_shader_get_location(raster.forwardInst, uEmissionColor);
    r3d_shader_get_location(raster.forwardInst, uCubeIrradiance);
    r3d_shader_get_location(raster.forwardInst, uCubePrefilter);
    r3d_shader_get_location(raster.forwardInst, uTexBrdfLut);
    r3d_shader_get_location(raster.forwardInst, uQuatSkybox);
    r3d_shader_get_location(raster.forwardInst, uHasSkybox);
    r3d_shader_get_location(raster.forwardInst, uSkyboxAmbientIntensity);
    r3d_shader_get_location(raster.forwardInst, uSkyboxReflectIntensity);
    r3d_shader_get_location(raster.forwardInst, uAlphaCutoff);
    r3d_shader_get_location(raster.forwardInst, uViewPosition);

    r3d_shader_enable(raster.forwardInst);

    r3d_shader_set_sampler2D_slot(raster.forwardInst, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(raster.forwardInst, uTexEmission, 1);
    r3d_shader_set_sampler2D_slot(raster.forwardInst, uTexNormal, 2);
    r3d_shader_set_sampler2D_slot(raster.forwardInst, uTexORM, 3);
    r3d_shader_set_sampler2D_slot(raster.forwardInst, uTexNoise, 4);
    r3d_shader_set_samplerCube_slot(raster.forwardInst, uCubeIrradiance, 5);
    r3d_shader_set_samplerCube_slot(raster.forwardInst, uCubePrefilter, 6);
    r3d_shader_set_sampler2D_slot(raster.forwardInst, uTexBrdfLut, 7);

    int shadowMapSlot = 10;
    for (int i = 0; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++) {
        shader->uMatLightVP[i].loc = rlGetLocationUniform(shader->id, TextFormat("uMatLightVP[%i]", i));
        shader->uLights[i].shadowMap.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowMap", i));
        shader->uLights[i].shadowCubemap.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowCubemap", i));
        shader->uLights[i].color.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].color", i));
        shader->uLights[i].position.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].position", i));
        shader->uLights[i].direction.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].direction", i));
        shader->uLights[i].specular.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].specular", i));
        shader->uLights[i].energy.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].energy", i));
        shader->uLights[i].range.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].range", i));
        shader->uLights[i].near.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].near", i));
        shader->uLights[i].far.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].far", i));
        shader->uLights[i].attenuation.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].attenuation", i));
        shader->uLights[i].innerCutOff.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].innerCutOff", i));
        shader->uLights[i].outerCutOff.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].outerCutOff", i));
        shader->uLights[i].shadowSoftness.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowSoftness", i));
        shader->uLights[i].shadowMapTxlSz.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowMapTxlSz", i));
        shader->uLights[i].shadowBias.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowBias", i));
        shader->uLights[i].type.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].type", i));
        shader->uLights[i].enabled.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].enabled", i));
        shader->uLights[i].shadow.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadow", i));

        r3d_shader_set_sampler2D_slot(raster.forwardInst, uLights[i].shadowMap, shadowMapSlot++);
        r3d_shader_set_samplerCube_slot(raster.forwardInst, uLights[i].shadowCubemap, shadowMapSlot++);
    }

    r3d_shader_disable();
}

void r3d_shader_load_raster_skybox(void)
{
    R3D.shader.raster.skybox.id = rlLoadShaderCode(
        SKYBOX_VERT, SKYBOX_FRAG
    );

    r3d_shader_get_location(raster.skybox, uMatProj);
    r3d_shader_get_location(raster.skybox, uMatView);
    r3d_shader_get_location(raster.skybox, uRotation);
    r3d_shader_get_location(raster.skybox, uSkyIntensity);
    r3d_shader_get_location(raster.skybox, uCubeSky);

    r3d_shader_enable(raster.skybox);
    r3d_shader_set_samplerCube_slot(raster.skybox, uCubeSky, 0);
    r3d_shader_disable();
}

void r3d_shader_load_raster_depth_volume(void)
{
    R3D.shader.raster.depthVolume.id = rlLoadShaderCode(
        DEPTH_VOLUME_VERT, DEPTH_VOLUME_FRAG
    );

    r3d_shader_get_location(raster.depthVolume, uMatMVP);
}

void r3d_shader_load_raster_depth(void)
{
    R3D.shader.raster.depth.id = rlLoadShaderCode(
        DEPTH_VERT, DEPTH_FRAG
    );

    for (int i = 0; i < R3D_SHADER_MAX_BONES; i++) {
        R3D.shader.raster.depth.uBoneMatrices[i].loc = rlGetLocationUniform(
            R3D.shader.raster.depth.id, TextFormat("uBoneMatrices[%i]", i)
        );
    }

    r3d_shader_get_location(raster.depth, uUseSkinning);
    r3d_shader_get_location(raster.depth, uMatMVP);
    r3d_shader_get_location(raster.depth, uAlpha);
    r3d_shader_get_location(raster.depth, uTexAlbedo);
    r3d_shader_get_location(raster.depth, uAlphaCutoff);
}

void r3d_shader_load_raster_depth_inst(void)
{
    R3D.shader.raster.depthInst.id = rlLoadShaderCode(
        DEPTH_INSTANCED_VERT, DEPTH_FRAG
    );

    for (int i = 0; i < R3D_SHADER_MAX_BONES; i++) {
        R3D.shader.raster.depthInst.uBoneMatrices[i].loc = rlGetLocationUniform(
            R3D.shader.raster.depthInst.id, TextFormat("uBoneMatrices[%i]", i)
        );
    }

    r3d_shader_get_location(raster.depthInst, uUseSkinning);
    r3d_shader_get_location(raster.depthInst, uMatInvView);
    r3d_shader_get_location(raster.depthInst, uMatModel);
    r3d_shader_get_location(raster.depthInst, uMatVP);
    r3d_shader_get_location(raster.depthInst, uBillboardMode);
    r3d_shader_get_location(raster.depthInst, uAlpha);
    r3d_shader_get_location(raster.depthInst, uTexAlbedo);
    r3d_shader_get_location(raster.depthInst, uAlphaCutoff);
}

void r3d_shader_load_raster_depth_cube(void)
{
    R3D.shader.raster.depthCube.id = rlLoadShaderCode(
        DEPTH_CUBE_VERT, DEPTH_CUBE_FRAG
    );

    for (int i = 0; i < R3D_SHADER_MAX_BONES; i++) {
        R3D.shader.raster.depthCube.uBoneMatrices[i].loc = rlGetLocationUniform(
            R3D.shader.raster.depthCube.id, TextFormat("uBoneMatrices[%i]", i)
        );
    }

    r3d_shader_get_location(raster.depthCube, uUseSkinning);
    r3d_shader_get_location(raster.depthCube, uViewPosition);
    r3d_shader_get_location(raster.depthCube, uMatModel);
    r3d_shader_get_location(raster.depthCube, uMatMVP);
    r3d_shader_get_location(raster.depthCube, uFar);
    r3d_shader_get_location(raster.depthCube, uAlpha);
    r3d_shader_get_location(raster.depthCube, uTexAlbedo);
    r3d_shader_get_location(raster.depthCube, uAlphaCutoff);
}

void r3d_shader_load_raster_depth_cube_inst(void)
{
    R3D.shader.raster.depthCubeInst.id = rlLoadShaderCode(
        DEPTH_CUBE_INSTANCED_VERT, DEPTH_CUBE_FRAG
    );

    for (int i = 0; i < R3D_SHADER_MAX_BONES; i++) {
        R3D.shader.raster.depthCubeInst.uBoneMatrices[i].loc = rlGetLocationUniform(
            R3D.shader.raster.depthCubeInst.id, TextFormat("uBoneMatrices[%i]", i)
        );
    }

    r3d_shader_get_location(raster.depthCubeInst, uUseSkinning);
    r3d_shader_get_location(raster.depthCubeInst, uViewPosition);
    r3d_shader_get_location(raster.depthCubeInst, uMatInvView);
    r3d_shader_get_location(raster.depthCubeInst, uMatModel);
    r3d_shader_get_location(raster.depthCubeInst, uMatVP);
    r3d_shader_get_location(raster.depthCubeInst, uFar);
    r3d_shader_get_location(raster.depthCubeInst, uBillboardMode);
    r3d_shader_get_location(raster.depthCubeInst, uAlpha);
    r3d_shader_get_location(raster.depthCubeInst, uTexAlbedo);
    r3d_shader_get_location(raster.depthCubeInst, uAlphaCutoff);
}

void r3d_shader_load_screen_ssao(void)
{
    R3D.shader.screen.ssao.id = rlLoadShaderCode(
        SCREEN_VERT, SSAO_FRAG
    );

    r3d_shader_get_location(screen.ssao, uTexDepth);
    r3d_shader_get_location(screen.ssao, uTexNormal);
    r3d_shader_get_location(screen.ssao, uTexKernel);
    r3d_shader_get_location(screen.ssao, uTexNoise);
    r3d_shader_get_location(screen.ssao, uMatInvProj);
    r3d_shader_get_location(screen.ssao, uMatInvView);
    r3d_shader_get_location(screen.ssao, uMatProj);
    r3d_shader_get_location(screen.ssao, uMatView);
    r3d_shader_get_location(screen.ssao, uResolution);
    r3d_shader_get_location(screen.ssao, uNear);
    r3d_shader_get_location(screen.ssao, uFar);
    r3d_shader_get_location(screen.ssao, uRadius);
    r3d_shader_get_location(screen.ssao, uBias);

    r3d_shader_enable(screen.ssao);
    r3d_shader_set_sampler2D_slot(screen.ssao, uTexDepth, 0);
    r3d_shader_set_sampler2D_slot(screen.ssao, uTexNormal, 1);
    r3d_shader_set_sampler1D_slot(screen.ssao, uTexKernel, 2);
    r3d_shader_set_sampler2D_slot(screen.ssao, uTexNoise, 3);
    r3d_shader_disable();
}

void r3d_shader_load_screen_ambient_ibl(void)
{
    const char* defines[] = { "#define IBL" };
    char* fsCode = r3d_shader_inject_defines(AMBIENT_FRAG, defines, 1);
    R3D.shader.screen.ambientIbl.id = rlLoadShaderCode(SCREEN_VERT, fsCode);

    RL_FREE(fsCode);

    r3d_shader_screen_ambient_ibl_t* shader = &R3D.shader.screen.ambientIbl;

    r3d_shader_get_location(screen.ambientIbl, uTexAlbedo);
    r3d_shader_get_location(screen.ambientIbl, uTexNormal);
    r3d_shader_get_location(screen.ambientIbl, uTexDepth);
    r3d_shader_get_location(screen.ambientIbl, uTexSSAO);
    r3d_shader_get_location(screen.ambientIbl, uTexORM);
    r3d_shader_get_location(screen.ambientIbl, uCubeIrradiance);
    r3d_shader_get_location(screen.ambientIbl, uCubePrefilter);
    r3d_shader_get_location(screen.ambientIbl, uTexBrdfLut);
    r3d_shader_get_location(screen.ambientIbl, uQuatSkybox);
    r3d_shader_get_location(screen.ambientIbl, uSkyboxAmbientIntensity);
    r3d_shader_get_location(screen.ambientIbl, uSkyboxReflectIntensity);
    r3d_shader_get_location(screen.ambientIbl, uViewPosition);
    r3d_shader_get_location(screen.ambientIbl, uMatInvProj);
    r3d_shader_get_location(screen.ambientIbl, uMatInvView);

    r3d_shader_enable(screen.ambientIbl);

    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexDepth, 2);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexSSAO, 3);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexORM, 4);

    r3d_shader_set_samplerCube_slot(screen.ambientIbl, uCubeIrradiance, 5);
    r3d_shader_set_samplerCube_slot(screen.ambientIbl, uCubePrefilter, 6);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexBrdfLut, 7);

    r3d_shader_disable();
}

void r3d_shader_load_screen_ambient(void)
{
    R3D.shader.screen.ambient.id = rlLoadShaderCode(
        SCREEN_VERT, AMBIENT_FRAG
    );

    r3d_shader_get_location(screen.ambient, uTexAlbedo);
    r3d_shader_get_location(screen.ambient, uTexSSAO);
    r3d_shader_get_location(screen.ambient, uTexORM);
    r3d_shader_get_location(screen.ambient, uAmbientColor);

    r3d_shader_enable(screen.ambient);
    r3d_shader_set_sampler2D_slot(screen.ambient, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(screen.ambient, uTexSSAO, 1);
    r3d_shader_set_sampler2D_slot(screen.ambient, uTexORM, 2);
    r3d_shader_disable();
}

void r3d_shader_load_screen_lighting(void)
{
    R3D.shader.screen.lighting.id = rlLoadShaderCode(SCREEN_VERT, LIGHTING_FRAG);
    r3d_shader_screen_lighting_t* shader = &R3D.shader.screen.lighting;

    r3d_shader_get_location(screen.lighting, uTexAlbedo);
    r3d_shader_get_location(screen.lighting, uTexNormal);
    r3d_shader_get_location(screen.lighting, uTexDepth);
    r3d_shader_get_location(screen.lighting, uTexORM);
    r3d_shader_get_location(screen.lighting, uTexNoise);
    r3d_shader_get_location(screen.lighting, uViewPosition);
    r3d_shader_get_location(screen.lighting, uMatInvProj);
    r3d_shader_get_location(screen.lighting, uMatInvView);

    r3d_shader_get_location(screen.lighting, uLight.matVP);
    r3d_shader_get_location(screen.lighting, uLight.shadowMap);
    r3d_shader_get_location(screen.lighting, uLight.shadowCubemap);
    r3d_shader_get_location(screen.lighting, uLight.color);
    r3d_shader_get_location(screen.lighting, uLight.position);
    r3d_shader_get_location(screen.lighting, uLight.direction);
    r3d_shader_get_location(screen.lighting, uLight.specular);
    r3d_shader_get_location(screen.lighting, uLight.energy);
    r3d_shader_get_location(screen.lighting, uLight.range);
    r3d_shader_get_location(screen.lighting, uLight.near);
    r3d_shader_get_location(screen.lighting, uLight.far);
    r3d_shader_get_location(screen.lighting, uLight.attenuation);
    r3d_shader_get_location(screen.lighting, uLight.innerCutOff);
    r3d_shader_get_location(screen.lighting, uLight.outerCutOff);
    r3d_shader_get_location(screen.lighting, uLight.shadowSoftness);
    r3d_shader_get_location(screen.lighting, uLight.shadowMapTxlSz);
    r3d_shader_get_location(screen.lighting, uLight.shadowBias);
    r3d_shader_get_location(screen.lighting, uLight.type);
    r3d_shader_get_location(screen.lighting, uLight.shadow);

    r3d_shader_enable(screen.lighting);

    r3d_shader_set_sampler2D_slot(screen.lighting, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(screen.lighting, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(screen.lighting, uTexDepth, 2);
    r3d_shader_set_sampler2D_slot(screen.lighting, uTexORM, 3);
    r3d_shader_set_sampler2D_slot(screen.lighting, uTexNoise, 4);

    r3d_shader_set_sampler2D_slot(screen.lighting, uLight.shadowMap, 5);
    r3d_shader_set_samplerCube_slot(screen.lighting, uLight.shadowCubemap, 6);

    r3d_shader_disable();
}

void r3d_shader_load_screen_scene(void)
{
    R3D.shader.screen.scene.id = rlLoadShaderCode(SCREEN_VERT, SCENE_FRAG);
    r3d_shader_screen_scene_t* shader = &R3D.shader.screen.scene;

    r3d_shader_get_location(screen.scene, uTexAlbedo);
    r3d_shader_get_location(screen.scene, uTexEmission);
    r3d_shader_get_location(screen.scene, uTexDiffuse);
    r3d_shader_get_location(screen.scene, uTexSpecular);

    r3d_shader_enable(screen.scene);

    r3d_shader_set_sampler2D_slot(screen.scene, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(screen.scene, uTexEmission, 1);
    r3d_shader_set_sampler2D_slot(screen.scene, uTexDiffuse, 2);
    r3d_shader_set_sampler2D_slot(screen.scene, uTexSpecular, 3);

    r3d_shader_disable();
}

void r3d_shader_load_screen_bloom(void)
{
    R3D.shader.screen.bloom.id = rlLoadShaderCode(
        SCREEN_VERT, BLOOM_FRAG
    );

    r3d_shader_get_location(screen.bloom, uTexColor);
    r3d_shader_get_location(screen.bloom, uTexBloomBlur);
    r3d_shader_get_location(screen.bloom, uBloomMode);
    r3d_shader_get_location(screen.bloom, uBloomIntensity);

    r3d_shader_enable(screen.bloom);
    r3d_shader_set_sampler2D_slot(screen.bloom, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(screen.bloom, uTexBloomBlur, 1);
    r3d_shader_disable();
}

void r3d_shader_load_screen_fog(void)
{
    R3D.shader.screen.fog.id = rlLoadShaderCode(
        SCREEN_VERT, FOG_FRAG
    );

    r3d_shader_get_location(screen.fog, uTexColor);
    r3d_shader_get_location(screen.fog, uTexDepth);
    r3d_shader_get_location(screen.fog, uNear);
    r3d_shader_get_location(screen.fog, uFar);
    r3d_shader_get_location(screen.fog, uFogMode);
    r3d_shader_get_location(screen.fog, uFogColor);
    r3d_shader_get_location(screen.fog, uFogStart);
    r3d_shader_get_location(screen.fog, uFogEnd);
    r3d_shader_get_location(screen.fog, uFogDensity);

    r3d_shader_enable(screen.fog);
    r3d_shader_set_sampler2D_slot(screen.fog, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(screen.fog, uTexDepth, 1);
    r3d_shader_disable();
}

void r3d_shader_load_screen_output(R3D_Tonemap tonemap)
{
    assert(R3D.shader.screen.output[tonemap].id == 0);

    const char* defines[] = {
        TextFormat("#define TONEMAPPER %i", tonemap)
    };

    char* fsCode = r3d_shader_inject_defines(OUTPUT_FRAG, defines, 1);
    R3D.shader.screen.output[tonemap].id = rlLoadShaderCode(SCREEN_VERT, fsCode);

    RL_FREE(fsCode);

    r3d_shader_get_location(screen.output[tonemap], uTexColor);
    r3d_shader_get_location(screen.output[tonemap], uTonemapExposure);
    r3d_shader_get_location(screen.output[tonemap], uTonemapWhite);
    r3d_shader_get_location(screen.output[tonemap], uBrightness);
    r3d_shader_get_location(screen.output[tonemap], uContrast);
    r3d_shader_get_location(screen.output[tonemap], uSaturation);
    r3d_shader_get_location(screen.output[tonemap], uResolution);

    r3d_shader_enable(screen.output[tonemap]);
    r3d_shader_set_sampler2D_slot(screen.output[tonemap], uTexColor, 0);
    r3d_shader_disable();
}

void r3d_shader_load_screen_fxaa(void)
{
    R3D.shader.screen.fxaa.id = rlLoadShaderCode(
        SCREEN_VERT, FXAA_FRAG
    );

    r3d_shader_get_location(screen.fxaa, uTexture);
    r3d_shader_get_location(screen.fxaa, uTexelSize);

    r3d_shader_enable(screen.fxaa);
    r3d_shader_set_sampler2D_slot(screen.fxaa, uTexture, 0);
    r3d_shader_disable();
}

/* === Texture loading functions === */

void r3d_texture_load_white(void)
{
    static const char DATA = 0xFF;
    R3D.texture.white = rlLoadTexture(&DATA, 1, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE, 1);
}

void r3d_texture_load_black(void)
{
    static const char DATA = 0x00;
    R3D.texture.black = rlLoadTexture(&DATA, 1, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE, 1);
}

void r3d_texture_load_normal(void)
{
    static const unsigned char DATA[3] = { 127, 127, 255 };
    R3D.texture.normal = rlLoadTexture(&DATA, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
}

void r3d_texture_load_blue_noise(void)
{
    Image image = LoadImageFromMemory(".png", (unsigned char*)BLUE_NOISE_128_PNG, BLUE_NOISE_128_PNG_SIZE);
    R3D.texture.blueNoise = rlLoadTexture(image.data, image.width, image.height, image.format, 1);
    UnloadImage(image);
}

void r3d_texture_load_ssao_noise(void)
{
#   define R3D_RAND_NOISE_RESOLUTION 4

    r3d_half_t noise[3 * R3D_RAND_NOISE_RESOLUTION * R3D_RAND_NOISE_RESOLUTION] = { 0 };

    for (int i = 0; i < R3D_RAND_NOISE_RESOLUTION * R3D_RAND_NOISE_RESOLUTION; i++) {
        noise[i * 3 + 0] = r3d_cvt_fh(((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f);
        noise[i * 3 + 1] = r3d_cvt_fh(((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f);
        noise[i * 3 + 2] = r3d_cvt_fh((float)GetRandomValue(0, INT16_MAX) / INT16_MAX);
    }

    R3D.texture.ssaoNoise = rlLoadTexture(noise,
        R3D_RAND_NOISE_RESOLUTION,
        R3D_RAND_NOISE_RESOLUTION,
        PIXELFORMAT_UNCOMPRESSED_R16G16B16,
        1
    );
}

void r3d_texture_load_ssao_kernel(void)
{
#   define R3D_SSAO_KERNEL_SIZE 32

    r3d_half_t kernel[3 * R3D_SSAO_KERNEL_SIZE] = { 0 };

    for (int i = 0; i < R3D_SSAO_KERNEL_SIZE; i++)
    {
        Vector3 sample = { 0 };

        sample.x = ((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f;
        sample.y = ((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f;
        sample.z = (float)GetRandomValue(0, INT16_MAX) / INT16_MAX;

        sample = Vector3Normalize(sample);
        sample = Vector3Scale(sample, (float)GetRandomValue(0, INT16_MAX) / INT16_MAX);

        float scale = (float)i / R3D_SSAO_KERNEL_SIZE;
        scale = Lerp(0.1f, 1.0f, scale * scale);
        sample = Vector3Scale(sample, scale);

        kernel[i * 3 + 0] = r3d_cvt_fh(sample.x);
        kernel[i * 3 + 1] = r3d_cvt_fh(sample.y);
        kernel[i * 3 + 2] = r3d_cvt_fh(sample.z);
    }

    glGenTextures(1, &R3D.texture.ssaoKernel);
    glBindTexture(GL_TEXTURE_1D, R3D.texture.ssaoKernel);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB16F, R3D_SSAO_KERNEL_SIZE, 0, GL_RGB, GL_HALF_FLOAT, kernel);

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
}

void r3d_texture_load_ibl_brdf_lut(void)
{
    // TODO: Review in case 'R3D.support.TEX_RG16F' is false

    Image img = { 0 };

    uint32_t width = 0, height = 0;
    uint32_t special_format_size = 0; // should be 4 or 8 (RG16F or RG32F)

    img.data = r3d_load_dds_from_memory_ext(
        (unsigned char*)IBL_BRDF_256_DDS, IBL_BRDF_256_DDS_SIZE,
        &width, &height, &special_format_size
    );

    img.width = (int)width;
    img.height = (int)height;

    if (img.data && (special_format_size == 4 || special_format_size == 8)) {
        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);

        GLenum internal_format = (special_format_size == 4) ? GL_RG16F : GL_RG32F;
        GLenum data_type = (special_format_size == 4) ? GL_HALF_FLOAT : GL_FLOAT;

        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, img.width, img.height, 0, GL_RG, data_type, img.data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        R3D.texture.iblBrdfLut = texId;
        RL_FREE(img.data);
    }
    else {
        img = LoadImageFromMemory(".dds", (unsigned char*)IBL_BRDF_256_DDS, IBL_BRDF_256_DDS_SIZE);
        R3D.texture.iblBrdfLut = rlLoadTexture(img.data, img.width, img.height, img.format, img.mipmaps);
        UnloadImage(img);
    }
}
