#pragma once

#include "Common.h"
#include "ResultTypes.h"

bool IsResultScreenActive();

void OpenResultPreview();

void OpenResultScreen(
    const ResultData& data,
    std::uint32_t selfId
);

void CloseResultScreen();

void DrawResultScreen();

void HandleResultKey(unsigned char key);

void HandleResultMouse(int button, int state, int x, int y);

void DrawResultPlayer(std::uint32_t playerId, const glm::vec3& footPosition,float scale);