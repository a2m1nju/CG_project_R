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
};

inline glm::vec3 FlapPoint(const Flap& flap, float t, float across) {
    const float distance = 0.14f + flap.length * t;
    const float halfWidth = 0.20f * (1.0f - t) + 0.045f;
    const float curl = t * t * t;
    const float y = -0.22f - 0.24f * std::sin(t * 1.57079633f) +
                    0.07f * curl * curl + 0.035f * across * across;
    return glm::vec3(
        flap.direction.x * distance - flap.direction.y * across * halfWidth,
        y,
        flap.direction.y * distance + flap.direction.x * across * halfWidth);
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
    const Flap flaps[] = {
        { glm::normalize(glm::vec2(-1.0f, 0.05f)), 0.69f, 1.00f },
        { glm::normalize(glm::vec2(0.83f, 0.55f)), 0.73f, 0.94f },
        { glm::normalize(glm::vec2(0.08f, -1.0f)), 0.55f, 0.88f },
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
                AddQuad(vertices, a, b, c, d, peel * (flap.tint * shade));
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
