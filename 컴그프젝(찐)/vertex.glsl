#version 330 core

layout(location=0) in vec3 vPos;   // 위치 (C++의 0번 속성)
layout(location=1) in vec3 vColor; // 색상 (C++의 1번 속성)

out vec3 outColor; // 프래그먼트 셰이더로 색상을 넘겨줌

uniform mat4 trans; // C++에서 "trans"라는 이름으로 MVP 행렬을 보냄

void main()
{
    gl_Position = trans * vec4(vPos, 1.0);
    outColor = vColor;
}