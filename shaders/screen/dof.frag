//
// reference impl. from SmashHit iOS game dev blog (https://blog.voxagon.se/2018/05/04/bokeh-depth-of-field-in-single-pass.html)
// we need to verify that we use the correct depth values, correctly linearized etc.
//

#version 330 core

/* === Varyings === */
noperspective in vec2 vTexCoord;

/* === Uniforms === */
uniform sampler2D uTexColor;
uniform sampler2D uTexDepth;

uniform float uNear;
uniform float uFar;

uniform float uFocusPoint;
uniform float uFocusScale;
uniform float uMaxBlurSize;

// 0 off, 1 green/black/blue, 2 linear depth
uniform int uDebugMode;  

/* === Output === */
out vec4 FragColor;

const float RAD_SCALE = 0.5; // Smaller = nicer blur, larger = faster
const float GOLDEN_ANGLE = 2.39996323;

/* === Helpers (mirrors fog.frag style) === */
float LinearizeDepth01(float d01)
{
    return (2.0 * uNear * uFar) / (uFar + uNear - (2.0 * d01 - 1.0) * (uFar - uNear));
}

float getBlurSize(float depth) {
    float coc = clamp((1.0 / uFocusPoint - 1.0 / depth) * uFocusScale, -1.0, 1.0);
    return abs(coc) * uMaxBlurSize;
}

void main()
{
    vec3 color = texture(uTexColor, vTexCoord).rgb;

    // Center depth and CoC
    float depthRaw = texture(uTexDepth, vTexCoord).r;
    float depthCenter01 = LinearizeDepth01(depthRaw);
   
    float centerSize  = getBlurSize(depthCenter01);

    // Compute texel size without a uniform (keeps plumbing simple)
    vec2 uPixelSize = 1.0 / vec2(textureSize(uTexColor, 0));

    //scatter as gather
    float tot = 1.0;

    float radius = RAD_SCALE;
    for (float ang = 0.0; radius < uMaxBlurSize; ang += GOLDEN_ANGLE) {
        vec2 tc = vTexCoord + vec2(cos(ang), sin(ang)) * uPixelSize * radius;

        vec3 sampleColor = texture(uTexColor, tc).rgb;
        float sampleDepth01 = LinearizeDepth01(texture(uTexDepth, tc).r);
        float sampleSize  = getBlurSize(sampleDepth01);

        if (sampleDepth01 > depthCenter01)
            sampleSize = clamp(sampleSize, 0.0, centerSize * 2.0);

        float m = smoothstep(radius - 0.5, radius + 0.5, sampleSize);
        color += mix(color / tot, sampleColor, m);
        tot += 1.0;
        radius += RAD_SCALE / max(radius, 0.001);
    }

    FragColor = vec4(color / tot, 1.0);

	if (uDebugMode == 1) {
		float cocSigned = clamp((1.0 / uFocusPoint - 1.0 / depthCenter01) * uFocusScale, -1.0, 1.0);
		float front = clamp(-cocSigned, 0.0, 1.0); // in front of focus plane (near)
		float back = clamp(cocSigned, 0.0, 1.0); // behind the focus plane (far)
		vec3 tint = vec3(0.0, front, back); // green front, blue back, black at focus
		FragColor = vec4(tint, 1.0);
	}
	else if (uDebugMode == 2) {
		float depthNorm = clamp((depthCenter01 - uNear) / (uFar - uNear), 0.0, 1.0);
		FragColor = vec4(depthNorm, depthNorm, depthNorm, 1.0);
	}
}
