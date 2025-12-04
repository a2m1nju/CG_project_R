#version 330 core

layout(location=0) in vec3 vPos;
layout(location=1) in vec3 vColor;

out vec3 FragPos;
out vec3 Color;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

void main()
{
    FragPos = vec3(model * vec4(vPos, 1.0));
    Color = vColor;

    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

    gl_Position = projection * view * vec4(FragPos, 1.0);
}
