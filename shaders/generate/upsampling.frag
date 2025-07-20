// This shader performs upsampling on a texture,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.

#version 330 core

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexture;
uniform vec2 uFilterRadius;

layout (location = 0) out vec3 FragColor;

/* === Main Function === */

void main()
{
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x = uFilterRadius.x;
    float y = uFilterRadius.y;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(uTexture, vec2(vTexCoord.x - x, vTexCoord.y + y)).rgb;
    vec3 b = texture(uTexture, vec2(vTexCoord.x,     vTexCoord.y + y)).rgb;
    vec3 c = texture(uTexture, vec2(vTexCoord.x + x, vTexCoord.y + y)).rgb;

    vec3 d = texture(uTexture, vec2(vTexCoord.x - x, vTexCoord.y)).rgb;
    vec3 e = texture(uTexture, vec2(vTexCoord.x,     vTexCoord.y)).rgb;
    vec3 f = texture(uTexture, vec2(vTexCoord.x + x, vTexCoord.y)).rgb;

    vec3 g = texture(uTexture, vec2(vTexCoord.x - x, vTexCoord.y - y)).rgb;
    vec3 h = texture(uTexture, vec2(vTexCoord.x,     vTexCoord.y - y)).rgb;
    vec3 iT = texture(uTexture, vec2(vTexCoord.x + x, vTexCoord.y - y)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    FragColor = e*4.0;
    FragColor += (b+d+f+h)*2.0;
    FragColor += (a+c+g+iT);
    FragColor *= 1.0 / 16.0;
}
