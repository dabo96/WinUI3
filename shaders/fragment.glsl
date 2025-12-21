#version 450 core
in vec4 vColor;
in vec2 vPos;
in vec2 vRectPos;
in vec2 vRectSize;
in float vCornerRadius;

out vec4 FragColor;

float roundedBoxSDF(vec2 centerPos, vec2 size, float radius) {
    return length(max(abs(centerPos) - size + radius, 0.0)) - radius;
}

void main() {
    if (vCornerRadius > 0.0) {
        // Calcular posición relativa al centro del rectángulo
        vec2 center = vRectPos + vRectSize * 0.5;
        vec2 halfSize = vRectSize * 0.5;
        vec2 pos = vPos - center;
        
        // Calcular distancia desde el borde redondeado
        float dist = roundedBoxSDF(pos, halfSize, vCornerRadius);
        
        // Antialiasing suave usando smoothstep
        // El ancho del antialiasing es aproximadamente 1 píxel
        float aaWidth = 1.0;
        float alpha = smoothstep(aaWidth, -aaWidth, dist);
        
        // Si está completamente fuera, descartar el píxel
        if (alpha <= 0.0) {
            discard;
        }
        
        // Aplicar el alpha calculado al color
        FragColor = vec4(vColor.rgb, vColor.a * alpha);
    } else {
        // Sin esquinas redondeadas: renderizar normalmente
        FragColor = vColor;
    }
}
