#version 450 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D msdfTexture;
uniform vec4 uTextColor1;
uniform float pxRange;

// Calculate the median of three values (used for MSDF)
// Optimized version for better performance
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

// Sharper step function - provides crisper edges than smoothstep
float sharpStep(float edge, float x) {
    float d = fwidth(x);
    return smoothstep(edge - d * 0.5, edge + d * 0.5, x);
}

void main()
{
    // Sample MSDF texture with nearest sampling characteristics preserved
    // The linear filter will interpolate between distance values correctly
    vec3 msdfSample = texture(msdfTexture, TexCoords).rgb;
    
    // Calculate signed distance from the median of the three channels
    float sigDist = median(msdfSample.r, msdfSample.g, msdfSample.b);
    
    // Calculate screen-space derivatives for adaptive antialiasing
    vec2 texSize = vec2(textureSize(msdfTexture, 0));
    vec2 texCoordDerivative = fwidth(TexCoords);
    
    // Calculate the screen-space pixel distance covered by one texture pixel
    // This is crucial for correct MSDF rendering at different zoom levels
    float texelSize = max(texCoordDerivative.x * texSize.x, texCoordDerivative.y * texSize.y);
    
    // Calculate the distance in pixels that the MSDF covers
    // pxRange is the distance range in the original texture
    float pixelDist = pxRange / texelSize;
    
    // Use a very tight smoothing range for maximum sharpness
    // Scale the smoothing based on actual pixel distance
    float smoothing = max(pixelDist * 0.4, 0.2); // Reduced from 0.6/0.35 to 0.4/0.2
    smoothing = min(smoothing, 0.25); // Tighter cap for ultra-sharp text
    
    // Convert signed distance to alpha
    // The edge is at 0.5 in normalized distance field space
    float edge = 0.5;
    float alpha = smoothstep(edge - smoothing, edge + smoothing, sigDist);
    
    // Apply aggressive sharpening for maximum crispness
    // This increases contrast at the edge to make text appear sharper
    // Using a steeper contrast curve
    alpha = clamp((alpha - 0.5) * 1.25 + 0.5, 0.0, 1.0);
    
    // Additional sharpening pass for extra crispness
    // This creates a steeper transition at the edge
    if (alpha > 0.01 && alpha < 0.99) {
        float sharpened = (alpha - 0.5) * 1.15 + 0.5;
        alpha = mix(alpha, sharpened, 0.7);
    }
    
    // Discard fragments that are clearly outside the glyph
    // This prevents any transparency artifacts around letters
    if (alpha <= 0.001) {
        discard;
    }
    
    FragColor = vec4(uTextColor1.rgb, uTextColor1.a * alpha);
}