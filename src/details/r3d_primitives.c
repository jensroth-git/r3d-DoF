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

#include "./r3d_primitives.h"
#include <stddef.h>
#include <glad.h>

r3d_primitive_t r3d_primitive_load_quad(void)
{
    // Structure: Pos(3) + Normal(3) + TexCoord(2) + Color(4 uchar) + Tangent(4)
    // Stride: 16 floats + 4 unsigned char = 64 + 4 = 68 bytes par vertex
    static const struct vertex {
        float pos[3];
        float normal[3];
        float texcoord[2];
        unsigned char color[4];
        float tangent[4];
    } VERTICES[] = {
        // Vertex 0: top-left
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {255, 255, 255, 255}, {1.0f, 0.0f, 0.0f, 1.0f}},
        // Vertex 1: bottom-left
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 0.0f, 0.0f, 1.0f}},
        // Vertex 2: top-right
        {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {255, 255, 255, 255}, {1.0f, 0.0f, 0.0f, 1.0f}},
        // Vertex 3: bottom-right
        {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 0.0f, 0.0f, 1.0f}},
    };

    static const unsigned short INDICES[] =
    {
        0, 1, 2, // First triangle (bottom-left, bottom-right, top-left)
        1, 3, 2  // Second triangle (bottom-right, top-right, top-left)
    };

    r3d_primitive_t quad = { 0 };
    
    quad.vao = rlLoadVertexArray();
    rlEnableVertexArray(quad.vao);
    
    quad.ebo = rlLoadVertexBufferElement(INDICES, sizeof(INDICES), false);
    quad.vbo = rlLoadVertexBuffer(VERTICES, sizeof(VERTICES), false);
    
    quad.indexCount = sizeof(INDICES) / sizeof(*INDICES);

    const int stride = (int)sizeof(VERTICES[0]);
    
    // Attribute 0: Positions (vec3)
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, stride, (int)offsetof(struct vertex, pos));
    rlEnableVertexAttribute(0);
    
    // Attribute 1: Texcoords (vec2)
    rlSetVertexAttribute(1, 2, RL_FLOAT, false, stride, (int)offsetof(struct vertex, texcoord));
    rlEnableVertexAttribute(1);
    
    // Attribute 2: Normals (vec3)
    rlSetVertexAttribute(2, 3, RL_FLOAT, false, stride, (int)offsetof(struct vertex, normal));
    rlEnableVertexAttribute(2);
    
    // Attribute 3: Coulors (vec4 unsigned char)
    rlSetVertexAttribute(3, 4, RL_UNSIGNED_BYTE, true, stride, (int)offsetof(struct vertex, color));
    rlEnableVertexAttribute(3);
    
    // Attribute 4: Tangents (vec4)
    rlSetVertexAttribute(4, 4, RL_FLOAT, false, stride, (int)offsetof(struct vertex, tangent));
    rlEnableVertexAttribute(4);
    
    rlDisableVertexArray();
    
    return quad;
}

r3d_primitive_t r3d_primitive_load_cube(void)
{
    // Structure: Pos(3) + Normal(3) + TexCoord(2) + Color(4 uchar) + Tangent(4)
    static const struct vertex {
        float pos[3];
        float normal[3];
        float texcoord[2];
        unsigned char color[4];
        float tangent[4];
    } VERTICES[] = {
        // Front face (Z+) - tangent points right (X+)
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f,  1.0f}, {0.0f, 1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 0: Front top-left
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f,  1.0f}, {0.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 1: Front bottom-left
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f,  1.0f}, {1.0f, 1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 2: Front top-right
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f,  1.0f}, {1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 3: Front bottom-right
        
        // Back face (Z-) - tangent points left (X-)
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {255, 255, 255, 255}, {-1.0f, 0.0f, 0.0f, 1.0f}}, // 4: Back top-left
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {255, 255, 255, 255}, {-1.0f, 0.0f, 0.0f, 1.0f}}, // 5: Back bottom-left
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {255, 255, 255, 255}, {-1.0f, 0.0f, 0.0f, 1.0f}}, // 6: Back top-right
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {255, 255, 255, 255}, {-1.0f, 0.0f, 0.0f, 1.0f}}, // 7: Back bottom-right
        
        // Left face (X-) - tangent points back (Z-)
        {{-1.0f,  1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {255, 255, 255, 255}, {0.0f, 0.0f, -1.0f, 1.0f}}, // 8: Left top-back
        {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f, -1.0f, 1.0f}}, // 9: Left bottom-back
        {{-1.0f,  1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {255, 255, 255, 255}, {0.0f, 0.0f, -1.0f, 1.0f}}, // 10: Left top-front
        {{-1.0f, -1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f, -1.0f, 1.0f}}, // 11: Left bottom-front
        
        // Right face (X+) - tangent points forward (Z+)
        {{ 1.0f,  1.0f,  1.0f}, { 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {255, 255, 255, 255}, {0.0f, 0.0f,  1.0f, 1.0f}}, // 12: Right top-front
        {{ 1.0f, -1.0f,  1.0f}, { 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f,  1.0f, 1.0f}}, // 13: Right bottom-front
        {{ 1.0f,  1.0f, -1.0f}, { 1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {255, 255, 255, 255}, {0.0f, 0.0f,  1.0f, 1.0f}}, // 14: Right top-back
        {{ 1.0f, -1.0f, -1.0f}, { 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {255, 255, 255, 255}, {0.0f, 0.0f,  1.0f, 1.0f}}, // 15: Right bottom-back
        
        // Top face (Y+) - tangent points right (X+)
        {{-1.0f,  1.0f, -1.0f}, {0.0f,  1.0f, 0.0f}, {0.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 16: Top back-left
        {{-1.0f,  1.0f,  1.0f}, {0.0f,  1.0f, 0.0f}, {0.0f, 1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 17: Top front-left
        {{ 1.0f,  1.0f, -1.0f}, {0.0f,  1.0f, 0.0f}, {1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 18: Top back-right
        {{ 1.0f,  1.0f,  1.0f}, {0.0f,  1.0f, 0.0f}, {1.0f, 1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 19: Top front-right
        
        // Bottom face (Y-) - tangent points right (X+)
        {{-1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 20: Bottom front-left
        {{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 21: Bottom back-left
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 22: Bottom front-right
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {255, 255, 255, 255}, { 1.0f, 0.0f, 0.0f, 1.0f}}, // 23: Bottom back-right
    };

    static const unsigned short INDICES[] =
    {
        // Front face
        0, 1, 2, 2, 1, 3,
        // Back face
        4, 5, 6, 6, 5, 7,
        // Left face
        8, 9, 10, 10, 9, 11,
        // Right face
        12, 13, 14, 14, 13, 15,
        // Top face
        16, 17, 18, 18, 17, 19,
        // Bottom face
        20, 21, 22, 22, 21, 23
    };

    r3d_primitive_t cube = { 0 };
    
    cube.vao = rlLoadVertexArray();
    rlEnableVertexArray(cube.vao);
    
    cube.ebo = rlLoadVertexBufferElement(INDICES, sizeof(INDICES), false);
    cube.vbo = rlLoadVertexBuffer(VERTICES, sizeof(VERTICES), false);

    cube.indexCount = sizeof(INDICES) / sizeof(*INDICES);
    
    const int stride = (int)sizeof(VERTICES[0]);
    
    // Attribute 0: Positions (vec3)
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, stride, (int)offsetof(struct vertex, pos));
    rlEnableVertexAttribute(0);
    
    // Attribute 1: Texcoords (vec2)
    rlSetVertexAttribute(1, 2, RL_FLOAT, false, stride, (int)offsetof(struct vertex, texcoord));
    rlEnableVertexAttribute(1);
    
    // Attribute 2: Normals (vec3)
    rlSetVertexAttribute(2, 3, RL_FLOAT, false, stride, (int)offsetof(struct vertex, normal));
    rlEnableVertexAttribute(2);
    
    // Attribute 3: Coulors (vec4 unsigned char)
    rlSetVertexAttribute(3, 4, RL_UNSIGNED_BYTE, true, stride, (int)offsetof(struct vertex, color));
    rlEnableVertexAttribute(3);
    
    // Attribute 4: Tangents (vec4)
    rlSetVertexAttribute(4, 4, RL_FLOAT, false, stride, (int)offsetof(struct vertex, tangent));
    rlEnableVertexAttribute(4);
    
    rlDisableVertexArray();
    
    return cube;
}

void r3d_primitive_unload(const r3d_primitive_t* primitive)
{
    rlUnloadVertexBuffer(primitive->vbo);
    rlUnloadVertexBuffer(primitive->ebo);
    rlUnloadVertexArray(primitive->vao);
}

void r3d_primitive_bind(const r3d_primitive_t* primitive)
{
    if (rlEnableVertexArray(primitive->vao)) {
        return;
    }

    rlEnableVertexBuffer(primitive->vbo);

    rlSetVertexAttribute(0, 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(0);

    rlSetVertexAttribute(1, 2, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(1);

    rlSetVertexAttribute(2, 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(2);

    rlSetVertexAttribute(3, 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(3);

    rlSetVertexAttribute(4, 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(4);

    rlEnableVertexBufferElement(primitive->ebo);
}

void r3d_primitive_unbind(void)
{
    rlDisableVertexArray();
    rlDisableVertexBuffer();
    rlDisableVertexBufferElement();
}

void r3d_primitive_draw(const r3d_primitive_t* primitive)
{
    glDrawElements(GL_TRIANGLES, primitive->indexCount, GL_UNSIGNED_SHORT, NULL);
}

void r3d_primitive_draw_instanced(const r3d_primitive_t* primitive, int instances)
{
    glDrawElementsInstanced(GL_TRIANGLES, primitive->indexCount, GL_UNSIGNED_SHORT, NULL, instances);
}

void r3d_primitive_bind_and_draw(const r3d_primitive_t* primitive)
{
    r3d_primitive_bind(primitive);
    r3d_primitive_draw(primitive);
    r3d_primitive_unbind();
}
