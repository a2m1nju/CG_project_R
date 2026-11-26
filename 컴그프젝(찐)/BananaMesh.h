#pragma once

#include "Common.h"

#include <cmath>
#include <vector>

namespace BananaMesh {
struct Vertex {
    GLfloat x, y, z;
    GLfloat r, g, b;
};

inline void AddVertex(std::vector<Vertex>& vertices, const glm::vec3& point,
                      const glm::vec3& color) {
    vertices.push_back({ point.x, point.y, point.z, color.r, color.g, color.b });
}

inline void AddQuad(std::vector<Vertex>& vertices,
                    const glm::vec3& a, const glm::vec3& b,
                    const glm::vec3& c, const glm::vec3& d,
                    const glm::vec3& color) {
    AddVertex(vertices, a, color);
    AddVertex(vertices, b, color);
    AddVertex(vertices, c, color);
    AddVertex(vertices, a, color);
    AddVertex(vertices, c, color);
    AddVertex(vertices, d, color);
}

struct Flap {
    glm::vec2 direction;
    float length;
    float tint;
    float rootStart;
    float rootEnd;
};

inline float Unfold(float t) {
    const float u = (std::min)(1.0f, t * 4.0f);
    return u * u * (3.0f - 2.0f * u);
}

inline glm::vec3 FlapPoint(const Flap& flap, float t, float across) {
    const float distance = 0.14f + flap.length * t;
    const float unfold = Unfold(t);
    const float halfWidth = (0.20f * (1.0f - t) + 0.045f) *
                            (0.44f + 0.56f * unfold);
    const float curl = t * t * t;
    const float y = -0.22f - 0.24f * std::sin(t * 1.57079633f) +
                    0.07f * curl * curl + 0.035f * across * across;
    const glm::vec3 openPoint(
        flap.direction.x * distance - flap.direction.y * across * halfWidth,
        y,
        flap.direction.y * distance + flap.direction.x * across * halfWidth);
    // 세 갈래의 첫 단면은 하나의 원형 밑동을 정확히 공유한다.
    // 바깥쪽 단면으로 서서히 옮겨 갈라지는 부분에 틈이 생기지 않게 한다.
    const float angle = flap.rootStart +
                        (flap.rootEnd - flap.rootStart) * (across + 1.0f) * 0.5f;
    const glm::vec3 rootPoint(0.17f * std::cos(angle), y,
                              0.17f * std::sin(angle));
    return glm::mix(rootPoint, openPoint, unfold);
}

inline std::vector<Vertex> Build() {
    std::vector<Vertex> vertices;
    const glm::vec3 fruit(0.96f, 0.78f, 0.05f);
    const glm::vec3 peel(0.98f, 0.82f, 0.06f);
    const glm::vec3 underside(0.76f, 0.55f, 0.06f);
    const glm::vec3 stem(0.43f, 0.31f, 0.06f);
    constexpr float tau = 6.2831853f;
    constexpr int sides = 12;

    // 펼쳐진 껍질 위로 보이는 과육을 위쪽으로 갈수록 가늘게 만든다.
    struct Ring { float x, y, z, radius; };
    const Ring rings[] = {
        { 0.00f, -0.30f, 0.00f, 0.16f },
        { 0.00f, -0.16f, 0.00f, 0.22f },
        { 0.01f,  0.06f, 0.00f, 0.22f },
        { 0.05f,  0.27f, 0.00f, 0.16f },
        { 0.08f,  0.43f, 0.00f, 0.10f },
        { 0.09f,  0.52f, 0.00f, 0.045f },
    };
    for (std::size_t i = 0; i + 1 < sizeof(rings) / sizeof(rings[0]); ++i) {
        for (int side = 0; side < sides; ++side) {
            const float a = tau * side / sides;
            const float b = tau * (side + 1) / sides;
            const auto point = [](const Ring& ring, float angle) {
                return glm::vec3(ring.x + ring.radius * std::cos(angle), ring.y,
                                 ring.z + ring.radius * std::sin(angle));
            };
            const float light = 0.82f + 0.14f * (std::max)(0.0f, std::sin(a));
            AddQuad(vertices, point(rings[i], a), point(rings[i], b),
                    point(rings[i + 1], b), point(rings[i + 1], a), fruit * light);
        }
    }

    // 같은 밑동에서 세 갈래의 껍질이 서로 다른 방향으로 휘어지게 만든다.
    constexpr float cut01 = 1.84f;
    constexpr float cut12 = -0.45f;
    constexpr float cut20 = -2.34f;
    const Flap flaps[] = {
        { glm::normalize(glm::vec2(-1.0f, 0.05f)), 0.69f, 1.00f,
          cut01, cut20 + tau },
        { glm::normalize(glm::vec2(0.83f, 0.55f)), 0.73f, 0.94f,
          cut12, cut01 },
        { glm::normalize(glm::vec2(0.08f, -1.0f)), 0.55f, 0.88f,
          cut20, cut12 },
    };
    constexpr int steps = 10;
    constexpr int strips = 4;
    for (const Flap& flap : flaps) {
        for (int step = 0; step < steps; ++step) {
            const float t0 = static_cast<float>(step) / steps;
            const float t1 = static_cast<float>(step + 1) / steps;
            for (int strip = 0; strip < strips; ++strip) {
                const float u0 = 2.0f * strip / strips - 1.0f;
                const float u1 = 2.0f * (strip + 1) / strips - 1.0f;
                const glm::vec3 a = FlapPoint(flap, t0, u0);
                const glm::vec3 b = FlapPoint(flap, t1, u0);
                const glm::vec3 c = FlapPoint(flap, t1, u1);
                const glm::vec3 d = FlapPoint(flap, t0, u1);
                const float shade = 0.86f + 0.14f * (1.0f - std::abs((u0 + u1) * 0.5f));
                const float tint0 = 1.0f + (flap.tint - 1.0f) * Unfold(t0);
                const float tint1 = 1.0f + (flap.tint - 1.0f) * Unfold(t1);
                // 단면별 색 차이도 밑동에서는 같게 맞춘다.
                AddVertex(vertices, a, peel * (tint0 * shade));
                AddVertex(vertices, b, peel * (tint1 * shade));
                AddVertex(vertices, c, peel * (tint1 * shade));
                AddVertex(vertices, a, peel * (tint0 * shade));
                AddVertex(vertices, c, peel * (tint1 * shade));
                AddVertex(vertices, d, peel * (tint0 * shade));
                const glm::vec3 thickness(0.0f, -0.035f, 0.0f);
                AddQuad(vertices, d + thickness, c + thickness,
                        b + thickness, a + thickness, underside);
                if (strip == 0 || strip + 1 == strips) {
                    AddQuad(vertices, a + thickness, b + thickness, b, a,
                            underside * 0.9f);
                }
            }
        }
    }

    // 과육 위에 짧은 갈색 꼭지를 붙인다.
    const Ring stalkBottom{ 0.09f, 0.51f, 0.00f, 0.05f };
    const Ring stalkTop{ 0.11f, 0.62f, 0.00f, 0.035f };
    for (int side = 0; side < sides; ++side) {
        const float a = tau * side / sides;
        const float b = tau * (side + 1) / sides;
        const auto point = [](const Ring& ring, float angle) {
            return glm::vec3(ring.x + ring.radius * std::cos(angle), ring.y,
                             ring.z + ring.radius * std::sin(angle));
        };
        AddQuad(vertices, point(stalkBottom, a), point(stalkBottom, b),
                point(stalkTop, b), point(stalkTop, a), stem);
    }
    return vertices;
}

inline void Draw(GLuint shader, const glm::mat4& model) {
    static GLuint peelVao = 0;
    static GLuint peelVbo = 0;
    static GLsizei vertexCount = 0;
    GLint previousVao = 0;
    GLint previousArrayBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    if (peelVao == 0) {
        const std::vector<Vertex> vertices = Build();
        vertexCount = static_cast<GLsizei>(vertices.size());
        glGenVertexArrays(1, &peelVao);
        glGenBuffers(1, &peelVbo);
        glBindVertexArray(peelVao);
        glBindBuffer(GL_ARRAY_BUFFER, peelVbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              reinterpret_cast<const void*>(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);
    }

    glBindVertexArray(peelVao);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE,
                       glm::value_ptr(model));
    glUniform1i(glGetUniformLocation(shader, "useTexture"), 0);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(static_cast<GLuint>(previousVao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previousArrayBuffer));
}
}
