#version 330 core

// Vertex Shader에서 받아오는 입력 변수들
in vec3 FragPos;
in vec3 Color;
in vec4 FragPosLightSpace;
in vec2 TexCoord;

// 화면에 출력할 색상 변수 선언
out vec4 FragColor;

uniform vec3 lightPos;
uniform sampler2D shadowMap;

uniform sampler2D targetTexture; 
uniform int useTexture;
uniform vec2 uvScale;

// 핀 조명 관련 유니폼 변수
uniform int enableSpotlight;   // 1이면 핀 조명 켜짐, 0이면 꺼짐
uniform vec3 spotlightPos;     // 주인공(빛의 중심) 위치

float ShadowCalc(vec4 fragLightSpace)
{
    vec3 projCoords = fragLightSpace.xyz / fragLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;

    float shadow = 0.0;
    float bias = 0.01;
    float size = 1.0 / 2048.0;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * size).r;
            if (projCoords.z - bias > pcfDepth)
                shadow += 1.0;
        }
    }
    shadow /= 9.0;
    return shadow;
}

void main()
{
    float shadow = ShadowCalc(FragPosLightSpace);
    
    vec3 objectColor;
    if (useTexture == 1) {
        vec3 texColor = texture(targetTexture, TexCoord * uvScale).rgb;
        objectColor = texColor * Color; 
    } else {
        objectColor = Color;
    }

    vec3 ambient = objectColor * 0.7;
    vec3 diffuse = objectColor * (1.0 - shadow);
    
    vec3 finalColor = ambient + diffuse;

    // 핀 조명(Spotlight) 로직
    if (enableSpotlight == 1) {
        // 현재 픽셀(FragPos)과 주인공 위치(spotlightPos) 사이의 거리 계산
        float dist = distance(FragPos, spotlightPos);
        
        // 핀 조명 반지름
        float radius = 5.0;
        
        // 부드러운 경계선 처리 
        float spotEffect = 1.0 - smoothstep(1.0, radius, dist);
        
        // 전체적으로 어둡게 만들고, 스팟 조명 효과를 더함
        finalColor = finalColor * (0.0 + spotEffect * 1.5); 
    }

    // 최종 색상을 출력 변수에 저장
    FragColor = vec4(finalColor, 1.0);
}