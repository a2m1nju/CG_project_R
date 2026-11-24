#pragma once

#include "Common.h"
#include <cstdint>

bool IsResultPreviewActive();

void OpenResultPreview();

void DrawResultPreview();

void HandleResultPreviewKey(unsigned char key);

void HandleResultPreviewMouse(
    int button,
    int state,
    int x,
    int y
);

// footPosition은 모델 중심이 아니라 발바닥 위치.
void DrawResultPlayer(
    std::uint32_t playerId,
    const glm::vec3& footPosition,
    float scale
);