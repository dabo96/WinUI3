#version 450 core
in vec2 vUV;

out vec4 FragColor;

uniform sampler2D uFontAtlas;
uniform vec4 uTextColor;

void main()
{
    float alpha = texture(uFontAtlas, vUV).r;
    FragColor = vec4(uTextColor.rgb, uTextColor.a * alpha);
}

