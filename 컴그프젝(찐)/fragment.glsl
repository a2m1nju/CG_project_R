#version 330 core

in vec3 outColor;   // 버텍스 셰이더에서 넘어온 색상
out vec4 FragColor; // 최종 화면 출력 색상

void main()
{
    FragColor = vec4(outColor, 1.0);
}