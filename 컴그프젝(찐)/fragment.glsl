#version 330 core

in vec3 FragPos;
in vec3 Color;
in vec4 FragPosLightSpace;

out vec4 FragColor;

uniform vec3 lightPos;
uniform sampler2D shadowMap;

float ShadowCalc(vec4 fragLightSpace)
{
    vec3 projCoords = fragLightSpace.xyz / fragLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float shadow = 0.0;
    float bias = 0.01;
    float size = 1.0 / 2048.0;

    for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    {
        float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * size).r;
        if (projCoords.z - bias > pcfDepth)
            shadow += 1.0;
    }

    shadow /= 9.0;
    return shadow;
}

void main()
{
    float shadow = ShadowCalc(FragPosLightSpace);

    vec3 ambient = Color * 0.7;
    vec3 diffuse = Color * (1.0 - shadow);

    FragColor = vec4(ambient + diffuse, 1.0);
}
