#include "r3d.h"

#include <raylib.h>
#include <raymath.h>
#include <glad.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

/* === Mesh Functions === */

R3D_Mesh R3D_GenMeshPoly(int sides, float radius, bool upload)
{
    R3D_Mesh mesh = {0};

    // Validation of parameters
    if (sides < 3 || radius <= 0.0f) return mesh;

    // Allocation mémoire
    // For a polygon: 1 central vertex + peripheral vertices
    mesh.vertexCount = sides + 1;
    mesh.indexCount = sides * 3; // sides triangles, 3 indices per triangle

    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Pre-compute some values
    const float angleStep = 2.0f * PI / sides;
    const Vector3 normal = {0.0f, 0.0f, 1.0f}; // Normal vers le haut (plan XY)
    const Vector4 defaultColor = {255, 255, 255, 255}; // Blanc opaque

    // Central vertex (index 0)
    mesh.vertices[0] = (R3D_Vertex){
        .position = {0.0f, 0.0f, 0.0f},
        .texcoord = {0.5f, 0.5f}, // Texture center
        .normal = normal,
        .color = defaultColor,
        .tangent = {1.0f, 0.0f, 0.0f, 1.0f} // Tangent to X+
    };

    // Generation of peripheral vertices and indices
    float minX = 0.0f, maxX = 0.0f, minY = 0.0f, maxY = 0.0f;

    for (int i = 0; i < sides; i++) {
        const float angle = i * angleStep;
        const float cosAngle = cosf(angle);
        const float sinAngle = sinf(angle);
    
        // Position on the circle
        const float x = radius * cosAngle;
        const float y = radius * sinAngle;
    
        // AABB Update
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    
        // Peripheral vertex
        mesh.vertices[i + 1] = (R3D_Vertex){
            .position = {x, y, 0.0f},
            .texcoord = {
                0.5f + 0.5f * cosAngle, // Circular UV mapping
                0.5f + 0.5f * sinAngle
            },
            .normal = normal,
            .color = defaultColor,
            .tangent = {-sinAngle, cosAngle, 0.0f, 1.0f} // Tangente perpendiculaire au rayon
        };
    
        // Indices for the triangle (center, current vertex, next vertex)
        const int baseIdx = i * 3;
        mesh.indices[baseIdx] = 0; // Center
        mesh.indices[baseIdx + 1] = i + 1; // Current vertex
        mesh.indices[baseIdx + 2] = (i + 1) % sides + 1; // Next vertex (with wrap)
    }

    // Final AABB calculation
    mesh.aabb = (BoundingBox){
        .min = {minX, minY, 0.0f},
        .max = {maxX, maxY, 0.0f}
    };

    // Optional upload to GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshPlane(float width, float length, int resX, int resZ, bool upload)
{
    R3D_Mesh mesh = {0};

    // Validation of parameters
    if (width <= 0.0f || length <= 0.0f || resX < 1 || resZ < 1) return mesh;

    // Calculating grid dimensions
    const int verticesPerRow = resX + 1;
    const int verticesPerCol = resZ + 1;
    mesh.vertexCount = verticesPerRow * verticesPerCol;
    mesh.indexCount = resX * resZ * 6; // 2 triangles per quad, 3 indices per triangle

    // Memory allocation
    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Pre-compute some values
    const float halfWidth = width * 0.5f;
    const float halfLength = length * 0.5f;
    const float stepX = width / resX;
    const float stepZ = length / resZ;
    const float uvStepX = 1.0f / resX;
    const float uvStepZ = 1.0f / resZ;

    const Vector3 normal = {0.0f, 1.0f, 0.0f}; // Normal vers Y+ (plan horizontal)
    const Vector4 defaultColor = {255, 255, 255, 255};
    const Vector4 tangent = {1.0f, 0.0f, 0.0f, 1.0f}; // Tangente vers X+

    // Vertex generation
    int vertexIndex = 0;
    for (int z = 0; z <= resZ; z++) {
        const float posZ = -halfLength + z * stepZ;
        const float uvZ = (float)z * uvStepZ;
    
        for (int x = 0; x <= resX; x++) {
            const float posX = -halfWidth + x * stepX;
            const float uvX = (float)x * uvStepX;
        
            mesh.vertices[vertexIndex] = (R3D_Vertex){
                .position = {posX, 0.0f, posZ},
                .texcoord = {uvX, uvZ},
                .normal = normal,
                .color = defaultColor,
                .tangent = tangent
            };
            vertexIndex++;
        }
    }

    // Generation of indices (counter-clockwise order)
    int indexOffset = 0;
    for (int z = 0; z < resZ; z++) {
        const int rowStart = z * verticesPerRow;
        const int nextRowStart = (z + 1) * verticesPerRow;
    
        for (int x = 0; x < resX; x++) {
            // Clues from the 4 corners of the quad
            const unsigned int topLeft = rowStart + x;
            const unsigned int topRight = rowStart + x + 1;
            const unsigned int bottomLeft = nextRowStart + x;
            const unsigned int bottomRight = nextRowStart + x + 1;
        
            // First triangle (topLeft, bottomLeft, topRight)
            mesh.indices[indexOffset++] = topLeft;
            mesh.indices[indexOffset++] = bottomLeft;
            mesh.indices[indexOffset++] = topRight;
        
            // Second triangle (topRight, bottomLeft, bottomRight)
            mesh.indices[indexOffset++] = topRight;
            mesh.indices[indexOffset++] = bottomLeft;
            mesh.indices[indexOffset++] = bottomRight;
        }
    }

    // AABB Calculation
    mesh.aabb = (BoundingBox){
        .min = {-halfWidth, 0.0f, -halfLength},
        .max = {halfWidth, 0.0f, halfLength}
    };

    // Optional upload to GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshCube(float width, float height, float length, bool upload)
{
    R3D_Mesh mesh = {0};

    // Validation of parameters
    if (width <= 0.0f || height <= 0.0f || length <= 0.0f) return mesh;

    // Cube dimensions
    mesh.vertexCount = 24; // 4 vertices per face, 6 faces
    mesh.indexCount = 36;  // 2 triangles per face, 3 indices per triangle, 6 faces

    // Memory allocation
    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Pre-compute some values
    const float halfW = width * 0.5f;
    const float halfH = height * 0.5f;
    const float halfL = length * 0.5f;
    const Vector4 defaultColor = {255, 255, 255, 255};

    // Standard UV coordinates for each face
    const Vector2 uvs[4] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    // Generation of the 6 faces of the cube
    int vertexOffset = 0;
    int indexOffset = 0;

    // Back face (+Z)
    const Vector3 frontNormal = {0.0f, 0.0f, 1.0f};
    const Vector4 frontTangent = {1.0f, 0.0f, 0.0f, 1.0f};
    mesh.vertices[vertexOffset + 0] = (R3D_Vertex){{-halfW, -halfH, halfL}, uvs[0], frontNormal, defaultColor, frontTangent};
    mesh.vertices[vertexOffset + 1] = (R3D_Vertex){{halfW, -halfH, halfL}, uvs[1], frontNormal, defaultColor, frontTangent};
    mesh.vertices[vertexOffset + 2] = (R3D_Vertex){{halfW, halfH, halfL}, uvs[2], frontNormal, defaultColor, frontTangent};
    mesh.vertices[vertexOffset + 3] = (R3D_Vertex){{-halfW, halfH, halfL}, uvs[3], frontNormal, defaultColor, frontTangent};
    vertexOffset += 4;

    // Front face (-Z)
    const Vector3 backNormal = {0.0f, 0.0f, -1.0f};
    const Vector4 backTangent = {-1.0f, 0.0f, 0.0f, 1.0f};
    mesh.vertices[vertexOffset + 0] = (R3D_Vertex){{halfW, -halfH, -halfL}, uvs[0], backNormal, defaultColor, backTangent};
    mesh.vertices[vertexOffset + 1] = (R3D_Vertex){{-halfW, -halfH, -halfL}, uvs[1], backNormal, defaultColor, backTangent};
    mesh.vertices[vertexOffset + 2] = (R3D_Vertex){{-halfW, halfH, -halfL}, uvs[2], backNormal, defaultColor, backTangent};
    mesh.vertices[vertexOffset + 3] = (R3D_Vertex){{halfW, halfH, -halfL}, uvs[3], backNormal, defaultColor, backTangent};
    vertexOffset += 4;

    // Right face (+X)
    const Vector3 rightNormal = {1.0f, 0.0f, 0.0f};
    const Vector4 rightTangent = {0.0f, 0.0f, -1.0f, 1.0f};
    mesh.vertices[vertexOffset + 0] = (R3D_Vertex){{halfW, -halfH, halfL}, uvs[0], rightNormal, defaultColor, rightTangent};
    mesh.vertices[vertexOffset + 1] = (R3D_Vertex){{halfW, -halfH, -halfL}, uvs[1], rightNormal, defaultColor, rightTangent};
    mesh.vertices[vertexOffset + 2] = (R3D_Vertex){{halfW, halfH, -halfL}, uvs[2], rightNormal, defaultColor, rightTangent};
    mesh.vertices[vertexOffset + 3] = (R3D_Vertex){{halfW, halfH, halfL}, uvs[3], rightNormal, defaultColor, rightTangent};
    vertexOffset += 4;

    // Left face (-X)
    const Vector3 leftNormal = {-1.0f, 0.0f, 0.0f};
    const Vector4 leftTangent = {0.0f, 0.0f, 1.0f, 1.0f};
    mesh.vertices[vertexOffset + 0] = (R3D_Vertex){{-halfW, -halfH, -halfL}, uvs[0], leftNormal, defaultColor, leftTangent};
    mesh.vertices[vertexOffset + 1] = (R3D_Vertex){{-halfW, -halfH, halfL}, uvs[1], leftNormal, defaultColor, leftTangent};
    mesh.vertices[vertexOffset + 2] = (R3D_Vertex){{-halfW, halfH, halfL}, uvs[2], leftNormal, defaultColor, leftTangent};
    mesh.vertices[vertexOffset + 3] = (R3D_Vertex){{-halfW, halfH, -halfL}, uvs[3], leftNormal, defaultColor, leftTangent};
    vertexOffset += 4;

    // Face up (+Y)
    const Vector3 topNormal = {0.0f, 1.0f, 0.0f};
    const Vector4 topTangent = {1.0f, 0.0f, 0.0f, 1.0f};
    mesh.vertices[vertexOffset + 0] = (R3D_Vertex){{-halfW, halfH, halfL}, uvs[0], topNormal, defaultColor, topTangent};
    mesh.vertices[vertexOffset + 1] = (R3D_Vertex){{halfW, halfH, halfL}, uvs[1], topNormal, defaultColor, topTangent};
    mesh.vertices[vertexOffset + 2] = (R3D_Vertex){{halfW, halfH, -halfL}, uvs[2], topNormal, defaultColor, topTangent};
    mesh.vertices[vertexOffset + 3] = (R3D_Vertex){{-halfW, halfH, -halfL}, uvs[3], topNormal, defaultColor, topTangent};
    vertexOffset += 4;

    // Face down (-Y)
    const Vector3 bottomNormal = {0.0f, -1.0f, 0.0f};
    const Vector4 bottomTangent = {1.0f, 0.0f, 0.0f, 1.0f};
    mesh.vertices[vertexOffset + 0] = (R3D_Vertex){{-halfW, -halfH, -halfL}, uvs[0], bottomNormal, defaultColor, bottomTangent};
    mesh.vertices[vertexOffset + 1] = (R3D_Vertex){{halfW, -halfH, -halfL}, uvs[1], bottomNormal, defaultColor, bottomTangent};
    mesh.vertices[vertexOffset + 2] = (R3D_Vertex){{halfW, -halfH, halfL}, uvs[2], bottomNormal, defaultColor, bottomTangent};
    mesh.vertices[vertexOffset + 3] = (R3D_Vertex){{-halfW, -halfH, halfL}, uvs[3], bottomNormal, defaultColor, bottomTangent};

    // Generation of indices (same pattern for each face)
    for (int face = 0; face < 6; face++) {
        const unsigned int baseVertex = face * 4;
        const int baseIndex = face * 6;
    
        // First triangle (0, 1, 2)
        mesh.indices[baseIndex + 0] = baseVertex + 0;
        mesh.indices[baseIndex + 1] = baseVertex + 1;
        mesh.indices[baseIndex + 2] = baseVertex + 2;
    
        // Second triangle (2, 3, 0)
        mesh.indices[baseIndex + 3] = baseVertex + 2;
        mesh.indices[baseIndex + 4] = baseVertex + 3;
        mesh.indices[baseIndex + 5] = baseVertex + 0;
    }

    // AABB Calculation
    mesh.aabb = (BoundingBox){
        .min = {-halfW, -halfH, -halfL},
        .max = {halfW, halfH, halfL}
    };

    // Optional upload to GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshSphere(float radius, int rings, int slices, bool upload)
{
    R3D_Mesh mesh = {0};

    // Validation of parameters
    if (radius <= 0.0f || rings < 2 || slices < 3) return mesh;

    // Calculating dimensions
    // rings+1 lines of vertices (including poles)
    // slices+1 vertices per line (to close the texture)
    mesh.vertexCount = (rings + 1) * (slices + 1);
    mesh.indexCount = rings * slices * 6; // 2 triangles per quad

    // Memory allocation
    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Pre-compute some values
    const float ringStep = PI / rings;        // Vertical angle between rings
    const float sliceStep = 2.0f * PI / slices; // Horizontal angle between slices
    const Vector4 defaultColor = {255, 255, 255, 255};

    // Vertice generation
    int vertexIndex = 0;
    for (int ring = 0; ring <= rings; ring++) {
        const float phi = ring * ringStep;          // Vertical angle (0 to PI)
        const float sinPhi = sinf(phi);
        const float cosPhi = cosf(phi);
        const float y = radius * cosPhi;            // Y position
        const float ringRadius = radius * sinPhi;   // Ring radius
        const float v = (float)ring / rings;        // V texture coordinate
    
        for (int slice = 0; slice <= slices; slice++) {
            const float theta = slice * sliceStep;   // Horizontal angle (0 to 2PI)
            const float sinTheta = sinf(theta);
            const float cosTheta = cosf(theta);
        
            // Position on the sphere
            const float x = ringRadius * cosTheta;
            const float z = ringRadius * sinTheta;
        
            // Normal (normalized by construction)
            const Vector3 normal = {x / radius, y / radius, z / radius};
        
            // UV coordinates
            const float u = (float)slice / slices;
        
            // Calculation of the tangent (derivative with respect to theta)
            // Tangent = d(position)/d(theta) normalized
            const Vector3 tangentDir = {-sinTheta, 0.0f, cosTheta};
            const Vector4 tangent = {tangentDir.x, tangentDir.y, tangentDir.z, 1.0f};
        
            mesh.vertices[vertexIndex] = (R3D_Vertex){
                .position = {x, y, z},
                .texcoord = {u, v},
                .normal = normal,
                .color = defaultColor,
                .tangent = tangent
            };
            vertexIndex++;
        }
    }

    // Generation of indices
    int indexOffset = 0;
    const int verticesPerRing = slices + 1;

    for (int ring = 0; ring < rings; ring++) {
        const int currentRing = ring * verticesPerRing;
        const int nextRing = (ring + 1) * verticesPerRing;
    
        for (int slice = 0; slice < slices; slice++) {
            // Indices from the 4 corners of the quad
            const unsigned int current = currentRing + slice;
            const unsigned int next = currentRing + slice + 1;
            const unsigned int currentNext = nextRing + slice;
            const unsigned int nextNext = nextRing + slice + 1;
        
            // Special handling for poles (avoid degenerate triangles)
            if (ring == 0) {
                // North Pole - one triangle per slice
                mesh.indices[indexOffset++] = current;
                mesh.indices[indexOffset++] = nextNext;
                mesh.indices[indexOffset++] = currentNext;
            } else if (ring == rings - 1) {
                // South Pole - one triangle per slice
                mesh.indices[indexOffset++] = current;
                mesh.indices[indexOffset++] = next;
                mesh.indices[indexOffset++] = currentNext;
            } else {
                // Normal quad - 2 triangles
                // First triangle (current, currentNext, next)
                mesh.indices[indexOffset++] = current;
                mesh.indices[indexOffset++] = currentNext;
                mesh.indices[indexOffset++] = next;
            
                // Second triangle (next, currentNext, nextNext)
                mesh.indices[indexOffset++] = next;
                mesh.indices[indexOffset++] = currentNext;
                mesh.indices[indexOffset++] = nextNext;
            }
        }
    }

    // Adjusted the actual number of indices (due to poles)
    mesh.indexCount = indexOffset;

    // AABB Calculation
    mesh.aabb = (BoundingBox){
        .min = {-radius, -radius, -radius},
        .max = {radius, radius, radius}
    };

    // Optional upload to GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshHemiSphere(float radius, int rings, int slices, bool upload)
{
    R3D_Mesh mesh = {0};

    // Validation des paramètres
    if (radius <= 0.0f || rings < 1 || slices < 3) return mesh;

    // Calcul des dimensions
    // rings+1 lignes de vertices pour l'hémisphère (de 0 à PI/2)
    // +1 ligne supplémentaire pour la base (y=0)
    // slices+1 vertices par ligne pour fermer la texture
    const int hemisphereVertexCount = (rings + 1) * (slices + 1);
    const int baseVertexCount = slices + 1; // Base circulaire
    mesh.vertexCount = hemisphereVertexCount + baseVertexCount;

    // Indices: hémisphère + base
    const int hemisphereIndexCount = rings * slices * 6; // 2 triangles par quad
    const int baseIndexCount = slices * 3; // 1 triangle par slice pour la base
    mesh.indexCount = hemisphereIndexCount + baseIndexCount;

    // Allocation mémoire
    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Pre-compute some values
    const float ringStep = (PI * 0.5f) / rings;  // Angle vertical (0 à PI/2)
    const float sliceStep = 2.0f * PI / slices;   // Angle horizontal (0 à 2PI)
    const Vector4 defaultColor = {255, 255, 255, 255};

    // Génération des vertices de l'hémisphère
    int vertexIndex = 0;
    for (int ring = 0; ring <= rings; ring++) {
        const float phi = ring * ringStep;          // Angle vertical (0 à PI/2)
        const float sinPhi = sinf(phi);
        const float cosPhi = cosf(phi);
        const float y = radius * cosPhi;            // Position Y (radius à 0)
        const float ringRadius = radius * sinPhi;   // Rayon de l'anneau
        const float v = (float)ring / rings;        // Coordonnée V texture (0 à 1)
    
        for (int slice = 0; slice <= slices; slice++) {
            const float theta = slice * sliceStep;   // Angle horizontal
            const float sinTheta = sinf(theta);
            const float cosTheta = cosf(theta);
        
            // Position sur l'hémisphère
            const float x = ringRadius * cosTheta;
            const float z = ringRadius * sinTheta;
        
            // Normale (normalisée par construction)
            const Vector3 normal = {x / radius, y / radius, z / radius};
        
            // Coordonnées UV
            const float u = (float)slice / slices;
        
            // Tangente (dérivée par rapport à theta)
            const Vector4 tangent = {-sinTheta, 0.0f, cosTheta, 1.0f};
        
            mesh.vertices[vertexIndex] = (R3D_Vertex){
                .position = {x, y, z},
                .texcoord = {u, v},
                .normal = normal,
                .color = defaultColor,
                .tangent = tangent
            };
            vertexIndex++;
        }
    }

    // Génération des vertices de la base (y = 0)
    const Vector3 baseNormal = {0.0f, -1.0f, 0.0f}; // Normale vers le bas
    const Vector4 baseTangent = {1.0f, 0.0f, 0.0f, 1.0f};

    for (int slice = 0; slice <= slices; slice++) {
        const float theta = slice * sliceStep;
        const float sinTheta = sinf(theta);
        const float cosTheta = cosf(theta);
    
        const float x = radius * cosTheta;
        const float z = radius * sinTheta;
    
        // UV mapping circulaire pour la base
        const float u = 0.5f + 0.5f * cosTheta;
        const float v = 0.5f + 0.5f * sinTheta;
    
        mesh.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, 0.0f, z},
            .texcoord = {u, v},
            .normal = baseNormal,
            .color = defaultColor,
            .tangent = baseTangent
        };
        vertexIndex++;
    }

    // Génération des indices pour l'hémisphère
    int indexOffset = 0;
    const int verticesPerRing = slices + 1;

    for (int ring = 0; ring < rings; ring++) {
        const int currentRing = ring * verticesPerRing;
        const int nextRing = (ring + 1) * verticesPerRing;
    
        for (int slice = 0; slice < slices; slice++) {
            const unsigned int current = currentRing + slice;
            const unsigned int next = currentRing + slice + 1;
            const unsigned int currentNext = nextRing + slice;
            const unsigned int nextNext = nextRing + slice + 1;
        
            if (ring == 0) {
                // Pôle nord - un seul triangle par slice
                mesh.indices[indexOffset++] = current;
                mesh.indices[indexOffset++] = nextNext;
                mesh.indices[indexOffset++] = currentNext;
            } else {
                // Quad normal - 2 triangles
                mesh.indices[indexOffset++] = current;
                mesh.indices[indexOffset++] = currentNext;
                mesh.indices[indexOffset++] = next;
            
                mesh.indices[indexOffset++] = next;
                mesh.indices[indexOffset++] = currentNext;
                mesh.indices[indexOffset++] = nextNext;
            }
        }
    }

    // Génération des indices pour la base
    const int baseVertexStart = hemisphereVertexCount;
    const int centerVertexIndex = baseVertexStart; // Premier vertex de la base (centre)

    for (int slice = 0; slice < slices; slice++) {
        const unsigned int current = baseVertexStart + slice;
        const unsigned int next = baseVertexStart + slice + 1;
    
        // Triangle de la base (ordre clockwise car normale vers le bas)
        mesh.indices[indexOffset++] = current;
        mesh.indices[indexOffset++] = next;
        mesh.indices[indexOffset++] = centerVertexIndex;
    }

    // Calcul AABB
    mesh.aabb = (BoundingBox){
        .min = {-radius, 0.0f, -radius},
        .max = {radius, radius, radius}
    };

    // Upload optionnel vers GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshCylinder(float radius, float height, int slices, bool upload)
{
    R3D_Mesh mesh = {0};

    // Validation des paramètres
    if (radius <= 0.0f || height <= 0.0f || slices < 3) return mesh;

    // Calcul des dimensions
    // Corps du cylindre: 2 lignes × (slices+1) vertices (haut et bas)
    // Base inférieure: slices+1 vertices (centre + périmètre)
    // Base supérieure: slices+1 vertices (centre + périmètre)
    const int bodyVertexCount = 2 * (slices + 1);
    const int capVertexCount = 2 * (slices + 1); // 2 bases
    mesh.vertexCount = bodyVertexCount + capVertexCount;

    // Indices: corps + 2 bases
    const int bodyIndexCount = slices * 6; // 2 triangles par slice
    const int capIndexCount = 2 * slices * 3; // slices triangles par base
    mesh.indexCount = bodyIndexCount + capIndexCount;

    // Allocation mémoire
    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Pre-compute some values
    const float halfHeight = height * 0.5f;
    const float sliceStep = 2.0f * PI / slices;
    const Vector4 defaultColor = {255, 255, 255, 255};

    // Génération des vertices du corps du cylindre
    int vertexIndex = 0;

    // Ligne du bas (y = -halfHeight)
    for (int slice = 0; slice <= slices; slice++) {
        const float theta = slice * sliceStep;
        const float cosTheta = cosf(theta);
        const float sinTheta = sinf(theta);
    
        const float x = radius * cosTheta;
        const float z = radius * sinTheta;
    
        // Normale horizontale (vers l'extérieur)
        const Vector3 normal = {cosTheta, 0.0f, sinTheta};
    
        // UV mapping: u = position angulaire, v = hauteur
        const float u = (float)slice / slices;
        const float v = 0.0f; // Bas du cylindre
    
        // Tangente (dérivée par rapport à theta)
        const Vector4 tangent = {-sinTheta, 0.0f, cosTheta, 1.0f};
    
        mesh.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, -halfHeight, z},
            .texcoord = {u, v},
            .normal = normal,
            .color = defaultColor,
            .tangent = tangent
        };
        vertexIndex++;
    }

    // Ligne du haut (y = halfHeight)
    for (int slice = 0; slice <= slices; slice++) {
        const float theta = slice * sliceStep;
        const float cosTheta = cosf(theta);
        const float sinTheta = sinf(theta);
    
        const float x = radius * cosTheta;
        const float z = radius * sinTheta;
    
        // Normale horizontale (vers l'extérieur)
        const Vector3 normal = {cosTheta, 0.0f, sinTheta};
    
        const float u = (float)slice / slices;
        const float v = 1.0f; // Haut du cylindre
    
        const Vector4 tangent = {-sinTheta, 0.0f, cosTheta, 1.0f};
    
        mesh.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, halfHeight, z},
            .texcoord = {u, v},
            .normal = normal,
            .color = defaultColor,
            .tangent = tangent
        };
        vertexIndex++;
    }

    // Génération des vertices de la base inférieure
    const Vector3 bottomNormal = {0.0f, -1.0f, 0.0f};
    const Vector4 bottomTangent = {1.0f, 0.0f, 0.0f, 1.0f};

    // Centre de la base inférieure
    mesh.vertices[vertexIndex] = (R3D_Vertex){
        .position = {0.0f, -halfHeight, 0.0f},
        .texcoord = {0.5f, 0.5f},
        .normal = bottomNormal,
        .color = defaultColor,
        .tangent = bottomTangent
    };
    const int bottomCenterIndex = vertexIndex;
    vertexIndex++;

    // Périmètre de la base inférieure
    for (int slice = 0; slice < slices; slice++) {
        const float theta = slice * sliceStep;
        const float cosTheta = cosf(theta);
        const float sinTheta = sinf(theta);
    
        const float x = radius * cosTheta;
        const float z = radius * sinTheta;
    
        // UV mapping circulaire
        const float u = 0.5f + 0.5f * cosTheta;
        const float v = 0.5f + 0.5f * sinTheta;
    
        mesh.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, -halfHeight, z},
            .texcoord = {u, v},
            .normal = bottomNormal,
            .color = defaultColor,
            .tangent = bottomTangent
        };
        vertexIndex++;
    }

    // Génération des vertices de la base supérieure
    const Vector3 topNormal = {0.0f, 1.0f, 0.0f};
    const Vector4 topTangent = {1.0f, 0.0f, 0.0f, 1.0f};

    // Centre de la base supérieure
    mesh.vertices[vertexIndex] = (R3D_Vertex){
        .position = {0.0f, halfHeight, 0.0f},
        .texcoord = {0.5f, 0.5f},
        .normal = topNormal,
        .color = defaultColor,
        .tangent = topTangent
    };
    const int topCenterIndex = vertexIndex;
    vertexIndex++;

    // Périmètre de la base supérieure
    for (int slice = 0; slice < slices; slice++) {
        const float theta = slice * sliceStep;
        const float cosTheta = cosf(theta);
        const float sinTheta = sinf(theta);
    
        const float x = radius * cosTheta;
        const float z = radius * sinTheta;
    
        // UV mapping circulaire
        const float u = 0.5f + 0.5f * cosTheta;
        const float v = 0.5f + 0.5f * sinTheta;
    
        mesh.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, halfHeight, z},
            .texcoord = {u, v},
            .normal = topNormal,
            .color = defaultColor,
            .tangent = topTangent
        };
        vertexIndex++;
    }

    // Génération des indices
    int indexOffset = 0;
    const int verticesPerRow = slices + 1;

    // Indices du corps du cylindre
    for (int slice = 0; slice < slices; slice++) {
        // Quad entre ligne du bas et ligne du haut
        const unsigned int bottomLeft = slice;
        const unsigned int bottomRight = slice + 1;
        const unsigned int topLeft = verticesPerRow + slice;
        const unsigned int topRight = verticesPerRow + slice + 1;
    
        // Premier triangle (bottomLeft, topLeft, bottomRight)
        mesh.indices[indexOffset++] = bottomLeft;
        mesh.indices[indexOffset++] = topLeft;
        mesh.indices[indexOffset++] = bottomRight;
    
        // Deuxième triangle (bottomRight, topLeft, topRight)
        mesh.indices[indexOffset++] = bottomRight;
        mesh.indices[indexOffset++] = topLeft;
        mesh.indices[indexOffset++] = topRight;
    }

    // Indices de la base inférieure
    const int bottomPerimeterStart = bottomCenterIndex + 1;
    for (int slice = 0; slice < slices; slice++) {
        const unsigned int current = bottomPerimeterStart + slice;
        const unsigned int next = bottomPerimeterStart + (slice + 1) % slices;
    
        // Triangle (centre, next, current) - ordre clockwise car normale vers le bas
        mesh.indices[indexOffset++] = bottomCenterIndex;
        mesh.indices[indexOffset++] = next;
        mesh.indices[indexOffset++] = current;
    }

    // Indices de la base supérieure
    const int topPerimeterStart = topCenterIndex + 1;
    for (int slice = 0; slice < slices; slice++) {
        const unsigned int current = topPerimeterStart + slice;
        const unsigned int next = topPerimeterStart + (slice + 1) % slices;
    
        // Triangle (centre, current, next) - ordre counter-clockwise
        mesh.indices[indexOffset++] = topCenterIndex;
        mesh.indices[indexOffset++] = current;
        mesh.indices[indexOffset++] = next;
    }

    // Calcul AABB
    mesh.aabb = (BoundingBox){
        .min = {-radius, -halfHeight, -radius},
        .max = {radius, halfHeight, radius}
    };

    // Upload optionnel vers GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshCone(float radius, float height, int slices, bool upload)
{
    R3D_Mesh mesh = {0};

    // Validation des paramètres
    if (radius <= 0.0f || height <= 0.0f || slices < 3) return mesh;

    // Calcul des dimensions
    // Corps du cône: 1 sommet + (slices+1) vertices base
    // Base: centre + slices vertices périmètre
    const int apexVertexCount = 1;
    const int bodyBaseVertexCount = slices + 1;
    const int baseVertexCount = slices + 1; // centre + périmètre
    mesh.vertexCount = apexVertexCount + bodyBaseVertexCount + baseVertexCount;

    // Indices: corps + base
    const int bodyIndexCount = slices * 3; // 1 triangle par slice
    const int baseIndexCount = slices * 3; // 1 triangle par slice
    mesh.indexCount = bodyIndexCount + baseIndexCount;

    // Allocation mémoire
    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Pre-compute some values
    const float halfHeight = height * 0.5f;
    const float sliceStep = 2.0f * PI / slices;
    const Vector4 defaultColor = {255, 255, 255, 255};

    // Calcul de la normale du cône (inclinée)
    // Pour un cône, la normale n'est pas perpendiculaire à la surface
    // mais inclinée selon l'angle du cône
    const float coneAngle = atanf(radius / height);
    const float normalY = cosf(coneAngle);
    const float normalRadial = sinf(coneAngle);

    int vertexIndex = 0;

    // Vertex du sommet (apex)
    mesh.vertices[vertexIndex] = (R3D_Vertex){
        .position = {0.0f, halfHeight, 0.0f},
        .texcoord = {0.5f, 1.0f}, // Centre en haut de la texture
        .normal = {0.0f, 1.0f, 0.0f}, // Normale vers le haut au sommet
        .color = defaultColor,
        .tangent = {1.0f, 0.0f, 0.0f, 1.0f}
    };
    const int apexIndex = vertexIndex;
    vertexIndex++;

    // Vertices du périmètre de la base (pour le corps du cône)
    for (int slice = 0; slice <= slices; slice++) {
        const float theta = slice * sliceStep;
        const float cosTheta = cosf(theta);
        const float sinTheta = sinf(theta);
    
        const float x = radius * cosTheta;
        const float z = radius * sinTheta;
    
        // Normale inclinée du cône
        const Vector3 normal = {
            normalRadial * cosTheta,
            normalY,
            normalRadial * sinTheta
        };
    
        // UV mapping: u = position angulaire, v = 0 (base)
        const float u = (float)slice / slices;
        const float v = 0.0f;
    
        // Tangente (direction tangentielle sur la base)
        const Vector4 tangent = {-sinTheta, 0.0f, cosTheta, 1.0f};
    
        mesh.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, -halfHeight, z},
            .texcoord = {u, v},
            .normal = normal,
            .color = defaultColor,
            .tangent = tangent
        };
        vertexIndex++;
    }

    // Génération des vertices de la base circulaire
    const Vector3 baseNormal = {0.0f, -1.0f, 0.0f};
    const Vector4 baseTangent = {1.0f, 0.0f, 0.0f, 1.0f};

    // Centre de la base
    mesh.vertices[vertexIndex] = (R3D_Vertex){
        .position = {0.0f, -halfHeight, 0.0f},
        .texcoord = {0.5f, 0.5f},
        .normal = baseNormal,
        .color = defaultColor,
        .tangent = baseTangent
    };
    const int baseCenterIndex = vertexIndex;
    vertexIndex++;

    // Périmètre de la base
    for (int slice = 0; slice < slices; slice++) {
        const float theta = slice * sliceStep;
        const float cosTheta = cosf(theta);
        const float sinTheta = sinf(theta);
    
        const float x = radius * cosTheta;
        const float z = radius * sinTheta;
    
        // UV mapping circulaire pour la base
        const float u = 0.5f + 0.5f * cosTheta;
        const float v = 0.5f + 0.5f * sinTheta;
    
        mesh.vertices[vertexIndex] = (R3D_Vertex){
            .position = {x, -halfHeight, z},
            .texcoord = {u, v},
            .normal = baseNormal,
            .color = defaultColor,
            .tangent = baseTangent
        };
        vertexIndex++;
    }

    // Génération des indices
    int indexOffset = 0;

    // Indices du corps du cône
    const int bodyBaseStart = apexIndex + 1;
    for (int slice = 0; slice < slices; slice++) {
        const unsigned int current = bodyBaseStart + slice;
        const unsigned int next = bodyBaseStart + slice + 1;
    
        // Triangle (apex, next, current) - ordre counter-clockwise
        mesh.indices[indexOffset++] = apexIndex;
        mesh.indices[indexOffset++] = next;
        mesh.indices[indexOffset++] = current;
    }

    // Indices de la base
    const int basePerimeterStart = baseCenterIndex + 1;
    for (int slice = 0; slice < slices; slice++) {
        const unsigned int current = basePerimeterStart + slice;
        const unsigned int next = basePerimeterStart + (slice + 1) % slices;
    
        // Triangle (centre, next, current) - ordre clockwise car normale vers le bas
        mesh.indices[indexOffset++] = baseCenterIndex;
        mesh.indices[indexOffset++] = next;
        mesh.indices[indexOffset++] = current;
    }

    // Calcul AABB
    mesh.aabb = (BoundingBox){
        .min = {-radius, -halfHeight, -radius},
        .max = {radius, halfHeight, radius}
    };

    // Upload optionnel vers GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshTorus(float radius, float size, int radSeg, int sides, bool upload)
{
    R3D_Mesh mesh = {0};

    // Validation des paramètres
    if (radius <= 0.0f || size <= 0.0f || radSeg < 3 || sides < 3) return mesh;

    // Calcul des dimensions
    // Un torus est composé de (radSeg+1) * (sides+1) vertices
    // pour permettre la fermeture correcte avec UV mapping
    mesh.vertexCount = (radSeg + 1) * (sides + 1);
    mesh.indexCount = radSeg * sides * 6; // 2 triangles par quad

    // Allocation mémoire
    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Pre-compute some values
    const float radStep = 2.0f * PI / radSeg;  // Angle autour de l'axe principal
    const float sideStep = 2.0f * PI / sides;  // Angle autour du tube
    const Vector4 defaultColor = {255, 255, 255, 255};

    // Génération des vertices
    int vertexIndex = 0;
    for (int i = 0; i <= radSeg; i++) {
        const float phi = i * radStep;       // Angle autour de l'axe Y
        const float cosPhi = cosf(phi);
        const float sinPhi = sinf(phi);
    
        for (int j = 0; j <= sides; j++) {
            const float theta = j * sideStep; // Angle autour du tube
            const float cosTheta = cosf(theta);
            const float sinTheta = sinf(theta);
        
            // Position du vertex
            // Le centre du tube circulaire se trouve à (radius * cos(phi), 0, radius * sin(phi))
            // Le point sur le tube est décalé de size dans la direction normale
            const float tubeX = (radius + size * cosTheta) * cosPhi;
            const float tubeY = size * sinTheta;
            const float tubeZ = (radius + size * cosTheta) * sinPhi;
        
            // Normale du torus
            // La normale pointe depuis le centre du tube vers le point
            const Vector3 normal = {
                cosTheta * cosPhi,
                sinTheta,
                cosTheta * sinPhi
            };
        
            // UV mapping
            const float u = (float)i / radSeg;
            const float v = (float)j / sides;
        
            // Tangente (direction tangentielle autour du tube principal)
            const Vector4 tangent = {-sinPhi, 0.0f, cosPhi, 1.0f};
        
            mesh.vertices[vertexIndex] = (R3D_Vertex){
                .position = {tubeX, tubeY, tubeZ},
                .texcoord = {u, v},
                .normal = normal,
                .color = defaultColor,
                .tangent = tangent
            };
            vertexIndex++;
        }
    }

    // Génération des indices
    int indexOffset = 0;
    for (int i = 0; i < radSeg; i++) {
        for (int j = 0; j < sides; j++) {
            // Indices des 4 coins du quad actuel
            const unsigned int current = i * (sides + 1) + j;
            const unsigned int next = current + sides + 1;
            const unsigned int currentNext = current + 1;
            const unsigned int nextNext = next + 1;
        
            // Premier triangle (current, next, currentNext)
            mesh.indices[indexOffset++] = current;
            mesh.indices[indexOffset++] = next;
            mesh.indices[indexOffset++] = currentNext;
        
            // Deuxième triangle (currentNext, next, nextNext)
            mesh.indices[indexOffset++] = currentNext;
            mesh.indices[indexOffset++] = next;
            mesh.indices[indexOffset++] = nextNext;
        }
    }

    // Calcul AABB
    const float outerRadius = radius + size;
    mesh.aabb = (BoundingBox){
        .min = {-outerRadius, -size, -outerRadius},
        .max = {outerRadius, size, outerRadius}
    };

    // Upload optionnel vers GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshKnot(float radius, float size, int radSeg, int sides, bool upload)
{
    R3D_Mesh mesh = {0};

    // Parameter validation
    if (radius <= 0.0f || size <= 0.0f || radSeg < 6 || sides < 3) return mesh;

    // Dimension calculation
    // A knot consists of (radSeg+1) * (sides+1) vertices
    mesh.vertexCount = (radSeg + 1) * (sides + 1);
    mesh.indexCount = radSeg * sides * 6; // 2 triangles per quad

    // Memory allocation
    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Precompute some values
    const float tStep = 2.0f * PI / radSeg;     // Parameter t along the knot
    const float sideStep = 2.0f * PI / sides;  // Angle around the tube
    const Vector4 defaultColor = {255, 255, 255, 255};

    // Trefoil knot parameters (3-loop knot)
    const float p = 2.0f; // Knot parameter p for (2,3)-torus knot
    const float q = 3.0f; // Knot parameter q

    // Calculate bounds for the AABB
    float minX = FLT_MAX, maxX = -FLT_MAX;
    float minY = FLT_MAX, maxY = -FLT_MAX;
    float minZ = FLT_MAX, maxZ = -FLT_MAX;

    // Generate vertices
    int vertexIndex = 0;
    for (int i = 0; i <= radSeg; i++) {
        const float t = i * tStep;
    
        // Parametric equations of the trefoil knot
        const float knotX = radius * (cosf(p * t) * (2.0f + cosf(q * t)));
        const float knotY = radius * (sinf(p * t) * (2.0f + cosf(q * t)));
        const float knotZ = radius * sinf(q * t);
    
        // Calculate first derivative (tangent)
        const float dxdt = radius * (-p * sinf(p * t) * (2.0f + cosf(q * t)) - q * cosf(p * t) * sinf(q * t));
        const float dydt = radius * (p * cosf(p * t) * (2.0f + cosf(q * t)) - q * sinf(p * t) * sinf(q * t));
        const float dzdt = radius * q * cosf(q * t);
    
        // Normalize the tangent
        const float tangentLength = sqrtf(dxdt * dxdt + dydt * dydt + dzdt * dzdt);
        const Vector3 tangent = {
            dxdt / tangentLength,
            dydt / tangentLength,
            dzdt / tangentLength
        };
    
        // Calculate an arbitrary normal vector (stable method)
        Vector3 up = {0.0f, 1.0f, 0.0f};
        if (fabsf(tangent.y) > 0.9f) {
            up = (Vector3){1.0f, 0.0f, 0.0f};
        }
    
        // Binormal (cross product of up × tangent)
        Vector3 binormal = {
            up.y * tangent.z - up.z * tangent.y,
            up.z * tangent.x - up.x * tangent.z,
            up.x * tangent.y - up.y * tangent.x
        };
    
        // Normalize the binormal
        const float binormalLength = sqrtf(binormal.x * binormal.x + binormal.y * binormal.y + binormal.z * binormal.z);
        binormal.x /= binormalLength;
        binormal.y /= binormalLength;
        binormal.z /= binormalLength;
    
        // Normal (cross product of tangent × binormal)
        Vector3 normal = {
            tangent.y * binormal.z - tangent.z * binormal.y,
            tangent.z * binormal.x - tangent.x * binormal.z,
            tangent.x * binormal.y - tangent.y * binormal.x
        };
    
        // Generate vertices around the tube
        for (int j = 0; j <= sides; j++) {
            const float theta = j * sideStep;
            const float cosTheta = cosf(theta);
            const float sinTheta = sinf(theta);
        
            // Vertex position on the tube
            const float tubeX = knotX + size * (cosTheta * normal.x + sinTheta * binormal.x);
            const float tubeY = knotY + size * (cosTheta * normal.y + sinTheta * binormal.y);
            const float tubeZ = knotZ + size * (cosTheta * normal.z + sinTheta * binormal.z);
        
            // Tube normal
            const Vector3 tubeNormal = {
                cosTheta * normal.x + sinTheta * binormal.x,
                cosTheta * normal.y + sinTheta * binormal.y,
                cosTheta * normal.z + sinTheta * binormal.z
            };
        
            // UV mapping
            const float u = (float)i / radSeg;
            const float v = (float)j / sides;
        
            // Tube tangent (direction along the knot)
            const Vector4 tubeTangent = {tangent.x, tangent.y, tangent.z, 1.0f};
        
            mesh.vertices[vertexIndex] = (R3D_Vertex){
                .position = {tubeX, tubeY, tubeZ},
                .texcoord = {u, v},
                .normal = tubeNormal,
                .color = defaultColor,
                .tangent = tubeTangent
            };
        
            // Update AABB bounds
            if (tubeX < minX) minX = tubeX;
            if (tubeX > maxX) maxX = tubeX;
            if (tubeY < minY) minY = tubeY;
            if (tubeY > maxY) maxY = tubeY;
            if (tubeZ < minZ) minZ = tubeZ;
            if (tubeZ > maxZ) maxZ = tubeZ;
        
            vertexIndex++;
        }
    }

    // Generate indices
    int indexOffset = 0;
    for (int i = 0; i < radSeg; i++) {
        for (int j = 0; j < sides; j++) {
            // Indices of the 4 corners of the current quad
            const unsigned int current = i * (sides + 1) + j;
            const unsigned int next = current + sides + 1;
            const unsigned int currentNext = current + 1;
            const unsigned int nextNext = next + 1;
        
            // First triangle (current, next, currentNext)
            mesh.indices[indexOffset++] = current;
            mesh.indices[indexOffset++] = next;
            mesh.indices[indexOffset++] = currentNext;
        
            // Second triangle (currentNext, next, nextNext)
            mesh.indices[indexOffset++] = currentNext;
            mesh.indices[indexOffset++] = next;
            mesh.indices[indexOffset++] = nextNext;
        }
    }

    // Compute AABB from calculated bounds
    mesh.aabb = (BoundingBox){
        .min = {minX, minY, minZ},
        .max = {maxX, maxY, maxZ}
    };

    // Optional upload to GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshHeightmap(Image heightmap, Vector3 size, bool upload)
{
    R3D_Mesh mesh = {0};

    // Parameter validation
    if (heightmap.data == NULL || heightmap.width <= 1 || heightmap.height <= 1 ||
        size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
        return mesh;
    }

    // Heightmap dimensions
    const int mapWidth = heightmap.width;
    const int mapHeight = heightmap.height;

    // Mesh dimensions calculation
    mesh.vertexCount = mapWidth * mapHeight;
    mesh.indexCount = (mapWidth - 1) * (mapHeight - 1) * 6; // 2 triangles per quad

    // Memory allocation
    mesh.vertices = (R3D_Vertex*)malloc(mesh.vertexCount * sizeof(R3D_Vertex));
    mesh.indices = (unsigned int*)malloc(mesh.indexCount * sizeof(unsigned int));

    if (!mesh.vertices || !mesh.indices) {
        if (mesh.vertices) free(mesh.vertices);
        if (mesh.indices) free(mesh.indices);
        return mesh;
    }

    // Precompute some values
    const float halfSizeX = size.x * 0.5f;
    const float halfSizeZ = size.z * 0.5f;
    const float stepX = size.x / (mapWidth - 1);
    const float stepZ = size.z / (mapHeight - 1);
    const float stepU = 1.0f / (mapWidth - 1);
    const float stepV = 1.0f / (mapHeight - 1);
    const Vector4 defaultColor = {255, 255, 255, 255};

    // Macro to extract height from a pixel
    #define GET_HEIGHT_VALUE(x, y) \
        ((x) < 0 || (x) >= mapWidth || (y) < 0 || (y) >= mapHeight) \
            ? 0.0f : ((float)GetImageColor(heightmap, x, y).r / 255)

    // Generate vertices
    int vertexIndex = 0;
    float minY = FLT_MAX, maxY = -FLT_MAX;

    for (int z = 0; z < mapHeight; z++) {
        for (int x = 0; x < mapWidth; x++) {
            // Vertex position
            const float posX = -halfSizeX + x * stepX;
            const float posZ = -halfSizeZ + z * stepZ;
            const float posY = GET_HEIGHT_VALUE(x, z);

            // Update Y bounds for AABB
            if (posY < minY) minY = posY;
            if (posY > maxY) maxY = posY;

            // Calculate normal by finite differences (gradient method)
            const float heightL = GET_HEIGHT_VALUE(x - 1, z);     // Left
            const float heightR = GET_HEIGHT_VALUE(x + 1, z);     // Right
            const float heightD = GET_HEIGHT_VALUE(x, z - 1);     // Down
            const float heightU = GET_HEIGHT_VALUE(x, z + 1);     // Up

            // Gradient in X and Z
            const float gradX = (heightR - heightL) / (2.0f * stepX);
            const float gradZ = (heightU - heightD) / (2.0f * stepZ);

            // Normal (cross product of tangent vectors)
            const Vector3 normal = {
                -gradX,
                1.0f,
                -gradZ
            };

            // Normalize the normal
            const float normalLength = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            const Vector3 normalizedNormal = {
                normal.x / normalLength,
                normal.y / normalLength,
                normal.z / normalLength
            };

            // UV mapping
            const float u = x * stepU;
            const float v = z * stepV;

            // Tangent (X direction in texture space)
            const Vector3 tangentDir = {1.0f, gradX, 0.0f};
            const float tangentLength = sqrtf(tangentDir.x * tangentDir.x + tangentDir.y * tangentDir.y + tangentDir.z * tangentDir.z);
            const Vector4 tangent = {
                tangentDir.x / tangentLength,
                tangentDir.y / tangentLength,
                tangentDir.z / tangentLength,
                1.0f
            };

            // Color based on height (optional)
            const float heightRatio = (posY - minY) / (size.y > 0.0f ? size.y : 1.0f);
            const unsigned char colorIntensity = (unsigned char)(255.0f * heightRatio);
            const Vector4 heightColor = {colorIntensity, colorIntensity, colorIntensity, 255};

            mesh.vertices[vertexIndex] = (R3D_Vertex){
                .position = {posX, posY, posZ},
                .texcoord = {u, v},
                .normal = normalizedNormal,
                .color = heightColor,
                .tangent = tangent
            };
            vertexIndex++;
        }
    }

    // Generate indices
    int indexOffset = 0;
    for (int z = 0; z < mapHeight - 1; z++) {
        for (int x = 0; x < mapWidth - 1; x++) {
            // Indices of the 4 corners of the current quad
            const unsigned int topLeft = z * mapWidth + x;
            const unsigned int topRight = topLeft + 1;
            const unsigned int bottomLeft = (z + 1) * mapWidth + x;
            const unsigned int bottomRight = bottomLeft + 1;
        
            // First triangle (topLeft, bottomLeft, topRight)
            mesh.indices[indexOffset++] = topLeft;
            mesh.indices[indexOffset++] = bottomLeft;
            mesh.indices[indexOffset++] = topRight;
        
            // Second triangle (topRight, bottomLeft, bottomRight)
            mesh.indices[indexOffset++] = topRight;
            mesh.indices[indexOffset++] = bottomLeft;
            mesh.indices[indexOffset++] = bottomRight;
        }
    }

    // Calculate AABB
    mesh.aabb = (BoundingBox){
        .min = {-halfSizeX, minY, -halfSizeZ},
        .max = {halfSizeX, maxY, halfSizeZ}
    };

    // Cleanup macro
    #undef GET_HEIGHT_VALUE

    // Optional upload to GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

R3D_Mesh R3D_GenMeshCubicmap(Image cubicmap, Vector3 cubeSize, bool upload)
{
    R3D_Mesh mesh = {0};

    // Validation of parameters
    if (cubicmap.width <= 0 || cubicmap.height <= 0 || 
        cubeSize.x <= 0.0f || cubeSize.y <= 0.0f || cubeSize.z <= 0.0f) {
        return mesh;
    }

    Color* pixels = LoadImageColors(cubicmap);
    if (!pixels) return mesh;

    // Pre-compute some values
    const float halfW = cubeSize.x * 0.5f;
    const float halfH = cubeSize.y * 0.5f;  // height
    const float halfL = cubeSize.z * 0.5f;
    const Vector4 defaultColor = {255, 255, 255, 255};

    // Normals of the 6 faces of the cube
    const Vector3 normals[6] = {
        {1.0f, 0.0f, 0.0f},   // right (+X)
        {-1.0f, 0.0f, 0.0f},  // left (-X)
        {0.0f, 1.0f, 0.0f},   // up (+Y)
        {0.0f, -1.0f, 0.0f},  // down (-Y)
        {0.0f, 0.0f, -1.0f},  // forward (-Z)
        {0.0f, 0.0f, 1.0f}    // backward (+Z)
    };

    // Corresponding tangents
    const Vector4 tangents[6] = {
        {0.0f, 0.0f, -1.0f, 1.0f},  // right
        {0.0f, 0.0f, 1.0f, 1.0f},   // left
        {1.0f, 0.0f, 0.0f, 1.0f},   // up
        {1.0f, 0.0f, 0.0f, 1.0f},   // down
        {-1.0f, 0.0f, 0.0f, 1.0f},  // forward
        {1.0f, 0.0f, 0.0f, 1.0f}    // backward
    };

    // UV coordinates for the 6 faces (2x3 atlas texture)
    typedef struct { float x, y, width, height; } RectangleF;
    const RectangleF texUVs[6] = {
        {0.0f, 0.0f, 0.5f, 0.5f},    // right
        {0.5f, 0.0f, 0.5f, 0.5f},    // left
        {0.0f, 0.5f, 0.5f, 0.5f},    // up
        {0.5f, 0.5f, 0.5f, 0.5f},    // down
        {0.5f, 0.0f, 0.5f, 0.5f},    // backward
        {0.0f, 0.0f, 0.5f, 0.5f}     // forward
    };

    // Estimate the maximum number of faces needed
    int maxFaces = 0;
    for (int z = 0; z < cubicmap.height; z++) {
        for (int x = 0; x < cubicmap.width; x++) {
            Color pixel = pixels[z * cubicmap.width + x];
            if (ColorIsEqual(pixel, WHITE)) {
                maxFaces += 6; // complete cube
            } else if (ColorIsEqual(pixel, BLACK)) {
                maxFaces += 2; // floor and ceiling only
            }
        }
    }

    // Allocation of temporary tables
    R3D_Vertex* vertices = (R3D_Vertex*)malloc(maxFaces * 4 * sizeof(R3D_Vertex));
    unsigned int* indices = (unsigned int*)malloc(maxFaces * 6 * sizeof(unsigned int));

    if (!vertices || !indices) {
        if (vertices) free(vertices);
        if (indices) free(indices);
        UnloadImageColors(pixels);
        return mesh;
    }

    int vertexCount = 0;
    int indexCount = 0;

    // Variables for calculating AABB
    float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

    // Mesh generation
    for (int z = 0; z < cubicmap.height; z++) {
        for (int x = 0; x < cubicmap.width; x++) {
            Color pixel = pixels[z * cubicmap.width + x];

            // Position of the center of the cube
            float posX = cubeSize.x * (x - cubicmap.width * 0.5f + 0.5f);
            float posZ = cubeSize.z * (z - cubicmap.height * 0.5f + 0.5f);

            // AABB Update
            minX = fminf(minX, posX - halfW);
            maxX = fmaxf(maxX, posX + halfW);
            minZ = fminf(minZ, posZ - halfL);
            maxZ = fmaxf(maxZ, posZ + halfL);

            if (ColorIsEqual(pixel, WHITE)) {
                // Complete cube - generate all necessary faces
                minY = fminf(minY, 0.0f);
                maxY = fmaxf(maxY, cubeSize.y);

                // Face up (always generated for white cubes)
                if (true) { // Top side still visible
                    Vector2 uvs[4] = {
                        {texUVs[2].x, texUVs[2].y},
                        {texUVs[2].x, texUVs[2].y + texUVs[2].height},
                        {texUVs[2].x + texUVs[2].width, texUVs[2].y + texUVs[2].height},
                        {texUVs[2].x + texUVs[2].width, texUVs[2].y}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ - halfL}, uvs[0], normals[2], defaultColor, tangents[2]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ + halfL}, uvs[1], normals[2], defaultColor, tangents[2]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ + halfL}, uvs[2], normals[2], defaultColor, tangents[2]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ - halfL}, uvs[3], normals[2], defaultColor, tangents[2]};

                    // Clues for 2 triangles
                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 2;
                    indices[indexCount + 4] = vertexCount + 3;
                    indices[indexCount + 5] = vertexCount + 0;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Face down
                if (true) {
                    Vector2 uvs[4] = {
                        {texUVs[3].x + texUVs[3].width, texUVs[3].y},
                        {texUVs[3].x, texUVs[3].y + texUVs[3].height},
                        {texUVs[3].x + texUVs[3].width, texUVs[3].y + texUVs[3].height},
                        {texUVs[3].x, texUVs[3].y}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, 0.0f, posZ - halfL}, uvs[0], normals[3], defaultColor, tangents[3]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX + halfW, 0.0f, posZ + halfL}, uvs[1], normals[3], defaultColor, tangents[3]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX - halfW, 0.0f, posZ + halfL}, uvs[2], normals[3], defaultColor, tangents[3]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, 0.0f, posZ - halfL}, uvs[3], normals[3], defaultColor, tangents[3]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 0;
                    indices[indexCount + 4] = vertexCount + 3;
                    indices[indexCount + 5] = vertexCount + 1;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Checking the lateral faces (occlusion culling)

                // Back face (+Z)
                if ((z == cubicmap.height - 1) || !ColorIsEqual(pixels[(z + 1) * cubicmap.width + x], WHITE)) {
                    Vector2 uvs[4] = {
                        {texUVs[5].x, texUVs[5].y},
                        {texUVs[5].x, texUVs[5].y + texUVs[5].height},
                        {texUVs[5].x + texUVs[5].width, texUVs[5].y},
                        {texUVs[5].x + texUVs[5].width, texUVs[5].y + texUVs[5].height}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ + halfL}, uvs[0], normals[5], defaultColor, tangents[5]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, 0.0f, posZ + halfL}, uvs[1], normals[5], defaultColor, tangents[5]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ + halfL}, uvs[2], normals[5], defaultColor, tangents[5]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, 0.0f, posZ + halfL}, uvs[3], normals[5], defaultColor, tangents[5]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 2;
                    indices[indexCount + 4] = vertexCount + 1;
                    indices[indexCount + 5] = vertexCount + 3;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Front face (-Z)
                if ((z == 0) || !ColorIsEqual(pixels[(z - 1) * cubicmap.width + x], WHITE)) {
                    Vector2 uvs[4] = {
                        {texUVs[4].x + texUVs[4].width, texUVs[4].y},
                        {texUVs[4].x, texUVs[4].y + texUVs[4].height},
                        {texUVs[4].x + texUVs[4].width, texUVs[4].y + texUVs[4].height},
                        {texUVs[4].x, texUVs[4].y}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ - halfL}, uvs[0], normals[4], defaultColor, tangents[4]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, 0.0f, posZ - halfL}, uvs[1], normals[4], defaultColor, tangents[4]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, 0.0f, posZ - halfL}, uvs[2], normals[4], defaultColor, tangents[4]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ - halfL}, uvs[3], normals[4], defaultColor, tangents[4]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 0;
                    indices[indexCount + 4] = vertexCount + 3;
                    indices[indexCount + 5] = vertexCount + 1;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Right face (+X)
                if ((x == cubicmap.width - 1) || !ColorIsEqual(pixels[z * cubicmap.width + (x + 1)], WHITE)) {
                    Vector2 uvs[4] = {
                        {texUVs[0].x, texUVs[0].y},
                        {texUVs[0].x, texUVs[0].y + texUVs[0].height},
                        {texUVs[0].x + texUVs[0].width, texUVs[0].y},
                        {texUVs[0].x + texUVs[0].width, texUVs[0].y + texUVs[0].height}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ + halfL}, uvs[0], normals[0], defaultColor, tangents[0]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX + halfW, 0.0f, posZ + halfL}, uvs[1], normals[0], defaultColor, tangents[0]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ - halfL}, uvs[2], normals[0], defaultColor, tangents[0]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, 0.0f, posZ - halfL}, uvs[3], normals[0], defaultColor, tangents[0]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 2;
                    indices[indexCount + 4] = vertexCount + 1;
                    indices[indexCount + 5] = vertexCount + 3;

                    vertexCount += 4;
                    indexCount += 6;
                }

                // Left face (-X)
                if ((x == 0) || !ColorIsEqual(pixels[z * cubicmap.width + (x - 1)], WHITE)) {
                    Vector2 uvs[4] = {
                        {texUVs[1].x, texUVs[1].y},
                        {texUVs[1].x + texUVs[1].width, texUVs[1].y + texUVs[1].height},
                        {texUVs[1].x + texUVs[1].width, texUVs[1].y},
                        {texUVs[1].x, texUVs[1].y + texUVs[1].height}
                    };

                    vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ - halfL}, uvs[0], normals[1], defaultColor, tangents[1]};
                    vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, 0.0f, posZ + halfL}, uvs[1], normals[1], defaultColor, tangents[1]};
                    vertices[vertexCount + 2] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ + halfL}, uvs[2], normals[1], defaultColor, tangents[1]};
                    vertices[vertexCount + 3] = (R3D_Vertex){{posX - halfW, 0.0f, posZ - halfL}, uvs[3], normals[1], defaultColor, tangents[1]};

                    indices[indexCount + 0] = vertexCount + 0;
                    indices[indexCount + 1] = vertexCount + 1;
                    indices[indexCount + 2] = vertexCount + 2;
                    indices[indexCount + 3] = vertexCount + 0;
                    indices[indexCount + 4] = vertexCount + 3;
                    indices[indexCount + 5] = vertexCount + 1;

                    vertexCount += 4;
                    indexCount += 6;
                }
            }
            else if (ColorIsEqual(pixel, BLACK)) {
                // Black pixel - generate only the floor and ceiling
                minY = fminf(minY, 0.0f);
                maxY = fmaxf(maxY, cubeSize.y);

                // Ceiling face (inverted to be visible from below)
                Vector2 uvs_top[4] = {
                    {texUVs[2].x, texUVs[2].y},
                    {texUVs[2].x + texUVs[2].width, texUVs[2].y + texUVs[2].height},
                    {texUVs[2].x, texUVs[2].y + texUVs[2].height},
                    {texUVs[2].x + texUVs[2].width, texUVs[2].y}
                };

                vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ - halfL}, uvs_top[0], normals[3], defaultColor, tangents[3]};
                vertices[vertexCount + 1] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ + halfL}, uvs_top[1], normals[3], defaultColor, tangents[3]};
                vertices[vertexCount + 2] = (R3D_Vertex){{posX - halfW, cubeSize.y, posZ + halfL}, uvs_top[2], normals[3], defaultColor, tangents[3]};
                vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, cubeSize.y, posZ - halfL}, uvs_top[3], normals[3], defaultColor, tangents[3]};

                indices[indexCount + 0] = vertexCount + 0;
                indices[indexCount + 1] = vertexCount + 1;
                indices[indexCount + 2] = vertexCount + 2;
                indices[indexCount + 3] = vertexCount + 0;
                indices[indexCount + 4] = vertexCount + 3;
                indices[indexCount + 5] = vertexCount + 1;

                vertexCount += 4;
                indexCount += 6;

                // Ground face
                Vector2 uvs_bottom[4] = {
                    {texUVs[3].x + texUVs[3].width, texUVs[3].y},
                    {texUVs[3].x + texUVs[3].width, texUVs[3].y + texUVs[3].height},
                    {texUVs[3].x, texUVs[3].y + texUVs[3].height},
                    {texUVs[3].x, texUVs[3].y}
                };

                vertices[vertexCount + 0] = (R3D_Vertex){{posX - halfW, 0.0f, posZ - halfL}, uvs_bottom[0], normals[2], defaultColor, tangents[2]};
                vertices[vertexCount + 1] = (R3D_Vertex){{posX - halfW, 0.0f, posZ + halfL}, uvs_bottom[1], normals[2], defaultColor, tangents[2]};
                vertices[vertexCount + 2] = (R3D_Vertex){{posX + halfW, 0.0f, posZ + halfL}, uvs_bottom[2], normals[2], defaultColor, tangents[2]};
                vertices[vertexCount + 3] = (R3D_Vertex){{posX + halfW, 0.0f, posZ - halfL}, uvs_bottom[3], normals[2], defaultColor, tangents[2]};

                indices[indexCount + 0] = vertexCount + 0;
                indices[indexCount + 1] = vertexCount + 1;
                indices[indexCount + 2] = vertexCount + 2;
                indices[indexCount + 3] = vertexCount + 2;
                indices[indexCount + 4] = vertexCount + 3;
                indices[indexCount + 5] = vertexCount + 0;

                vertexCount += 4;
                indexCount += 6;
            }
        }
    }

    // Final mesh allocation
    mesh.vertexCount = vertexCount;
    mesh.indexCount = indexCount;
    mesh.vertices = vertices;
    mesh.indices = indices;

    // Copy of final data
    memcpy(mesh.vertices, vertices, vertexCount * sizeof(R3D_Vertex));
    memcpy(mesh.indices, indices, indexCount * sizeof(unsigned int));

    // AABB Configuration
    mesh.aabb = (BoundingBox){
        .min = {minX, minY, minZ},
        .max = {maxX, maxY, maxZ}
    };

    // Cleaning
    UnloadImageColors(pixels);

    // Optional upload to GPU
    if (upload) {
        R3D_UploadMesh(&mesh, false);
    }

    return mesh;
}

bool R3D_UploadMesh(R3D_Mesh* mesh, bool dynamic)
{
    if (!mesh || mesh->vertexCount <= 0 || !mesh->vertices) {
        TraceLog(LOG_WARNING, "R3D: Invalid mesh data passed to R3D_UploadMesh");
        return false;
    }

    // Prevent re-upload if VAO already generated
    if (mesh->vao != 0) {
        TraceLog(LOG_WARNING, "R3D: Mesh already uploaded, use R3D_UpdateMesh to update the mesh");
        return false;
    }

    const size_t vertexSize = sizeof(R3D_Vertex);
    GLenum usage = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

    // Creation of the VAO
    glGenVertexArrays(1, &mesh->vao);
    glBindVertexArray(mesh->vao);

    // Creation of the VBO
    glGenBuffers(1, &mesh->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertexCount * vertexSize, mesh->vertices, usage);

    // Definition of attributes
    size_t offset = 0;

    // position (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexSize, (void*)offset);
    offset += sizeof(Vector3);

    // texcoord (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vertexSize, (void*)offset);
    offset += sizeof(Vector2);

    // normal (vec3)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, vertexSize, (void*)offset);
    offset += sizeof(Vector3);

    // color (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, vertexSize, (void*)offset);
    offset += sizeof(Vector4);

    // tangent (vec4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, vertexSize, (void*)offset);

    // EBO if indices present
    if (mesh->indexCount > 0 && mesh->indices) {
        glGenBuffers(1, &mesh->ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCount * sizeof(unsigned int), mesh->indices, usage);
    } else {
        mesh->ebo = 0;
    }

    // Cleaning
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (mesh->ebo != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    return true;
}

R3DAPI bool R3D_UpdateMesh(R3D_Mesh* mesh)
{
    if (!mesh || mesh->vao == 0 || mesh->vbo == 0) {
        TraceLog(LOG_WARNING, "R3D: Cannot update mesh - mesh not uploaded yet");
        return false;
    }

    if (mesh->vertexCount <= 0 || !mesh->vertices) {
        TraceLog(LOG_WARNING, "R3D: Invalid vertex data in R3D_UpdateMesh");
        return false;
    }

    glBindVertexArray(mesh->vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);

    const size_t vertexSize = sizeof(R3D_Vertex);
    glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->vertexCount * vertexSize, mesh->vertices);

    // Updates indices if provided
    if (mesh->indexCount > 0 && mesh->indices) {
        if (mesh->ebo == 0) {
            // Generate an EBO if there was none
            glGenBuffers(1, &mesh->ebo);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indexCount * sizeof(unsigned int), mesh->indices, GL_DYNAMIC_DRAW);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (mesh->ebo != 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    return true;
}

/* === Material Functions === */

R3D_Material R3D_GetDefaultMaterial(void)
{
    R3D_Material material = { 0 };

    // Albedo map
    material.albedo.texture = R3D_GetWhiteTexture();
    material.albedo.color = WHITE;

    // Emission map
    material.emission.texture = R3D_GetBlackTexture();
    material.emission.color = WHITE;
    material.emission.multiplier = 1.0f;

    // Normal map
    material.normal.texture = R3D_GetNormalTexture();

    // ORM map
    material.orm.texture = R3D_GetWhiteTexture();
    material.orm.occlusion = 1.0f;
    material.orm.roughness = 1.0f;
    material.orm.metalness = 0.0f;

    // Misc
    material.blendMode = R3D_BLEND_OPAQUE;
    material.cullMode = R3D_CULL_BACK;
    material.shadowCastMode = R3D_SHADOW_CAST_FRONT_FACES;
    material.billboardMode = R3D_BILLBOARD_DISABLED;

    return material;
}

/* === Model Functions === */

R3D_Model R3D_LoadModel(const char* filePath, bool upload)
{

}

R3D_Model R3D_LoadModelFromMemory(const void* data, unsigned int size, bool upload)
{

}

R3D_Model R3D_LoadModelFromMesh(const R3D_Mesh* mesh)
{

}
