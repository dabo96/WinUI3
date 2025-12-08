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

void main()
{
    // Sample MSDF texture with linear filtering
    // The linear filter correctly interpolates distance field values
    vec3 msdfSample = texture(msdfTexture, TexCoords).rgb;
    
    // Calculate signed distance from the median of the three channels
    // This gives us the closest distance to any edge
    float sigDist = median(msdfSample.r, msdfSample.g, msdfSample.b);
    
    // Calculate screen-space pixel range for proper antialiasing
    // This adapts to the actual on-screen size of the text
    vec2 texSize = vec2(textureSize(msdfTexture, 0));
    
    // Calculate texture coordinate derivatives (rate of change in screen space)
    vec2 texCoordDerivative = fwidth(TexCoords);
    
    // Calculate how many screen pixels one texture pixel spans
    // Use the maximum derivative to handle anisotropic scaling correctly
    float texelSize = max(
        texCoordDerivative.x * texSize.x,
        texCoordDerivative.y * texSize.y
    );
    
    // Calculate the screen-space distance range
    // pxRange is the distance range in texture pixels, so we divide by texelSize
    // to convert to screen pixels
    float screenPxRange = pxRange / max(texelSize, 0.5); // Prevent division by zero
    
    // Calculate smoothing width for antialiasing
    // Smaller values = sharper edges, larger values = smoother edges
    // We use a tighter range for crisper text while maintaining smooth antialiasing
    float smoothing = max(screenPxRange * 0.5, 0.3);
    
    // Cap the smoothing to prevent over-blurring
    // This ensures text remains sharp even at very large sizes
    smoothing = min(smoothing, 0.3);
    
    // Convert signed distance to alpha using smoothstep
    // MSDF stores: 0.5 at edge, >0.5 inside, <0.5 outside
    float edge = 0.5;
    float alpha = smoothstep(edge - smoothing, edge + smoothing, sigDist);
    
    // Apply contrast enhancement for sharper appearance
    // This steepens the alpha transition without causing artifacts
    // Using a power curve for natural sharpening
    alpha = pow(alpha, 0.92); // Slight gamma correction for sharper edges
    
    // Additional edge enhancement for maximum crispness
    // Apply selective sharpening only near the edge to avoid artifacts
    if (alpha > 0.05 && alpha < 0.95) {
        // Calculate distance from edge (0.5)
        float edgeDist = abs(sigDist - edge);
        float edgeFactor = 1.0 - smoothstep(0.0, smoothing * 2.0, edgeDist);
        
        // Apply sharpening only near edges
        float sharpened = clamp((alpha - 0.5) * 1.2 + 0.5, 0.0, 1.0);
        alpha = mix(alpha, sharpened, edgeFactor * 0.6);
    }
    
    // Early discard for performance - skip fragments clearly outside glyph
    // This prevents unnecessary blending operations
    if (alpha <= 0.005) {
        discard;
    }
    
    // Output final color with proper alpha
    FragColor = vec4(uTextColor1.rgb, uTextColor1.a * alpha);
}