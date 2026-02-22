#version 450 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D msdfTexture;
uniform vec4 uTextColor1;
uniform float pxRange;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

// Standard msdfgen screen pixel range calculation
float screenPxRange() {
    vec2 unitRange = vec2(pxRange) / vec2(textureSize(msdfTexture, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(TexCoords);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main()
{
    vec3 msd = texture(msdfTexture, TexCoords).rgb;

    // Extract signed distance from median of three channels
    float sd = median(msd.r, msd.g, msd.b);

    // Convert to screen-space distance and compute opacity
    float spxRange = screenPxRange();
    float screenPxDist = spxRange * (sd - 0.5);
    float opacity = clamp(screenPxDist + 0.5, 0.0, 1.0);

    if (opacity < 0.005) {
        discard;
    }

    FragColor = vec4(uTextColor1.rgb, uTextColor1.a * opacity);
}
