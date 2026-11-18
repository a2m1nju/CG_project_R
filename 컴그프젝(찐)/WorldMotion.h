#pragma once

#include <cmath>

namespace WorldMotion {

// 서버 좌표를 수신하는 사이에 화면 위치를 속도에 따라 진행시킨다.
inline float Advance(float x, float speed, float elapsedSeconds, float wrapAt = 0.0f) {
    x += speed * 60.0f * elapsedSeconds;
    if (wrapAt > 0.0f) {
        if (speed > 0.0f && x > wrapAt) return -wrapAt;
        if (speed < 0.0f && x < -wrapAt) return wrapAt;
    }
    return x;
}

// 작은 오차는 나누어 보정하고, 순환 이동이나 상태 전환은 즉시 반영한다.
inline float Reconcile(float displayedX, float serverX, float snapDistance) {
    const float difference = serverX - displayedX;
    if (!std::isfinite(displayedX) || std::abs(difference) > snapDistance) return serverX;
    return displayedX + difference * 0.2f;
}

}
