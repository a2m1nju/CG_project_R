#pragma once
#include "Common.h"

GameSeason getSeasonByZ(int z);
bool isTreeAt(int x, int z);
void generateLane(int z);
bool spawnItem(int x, int z, ItemType type);
void resetGame();
void spawnParticles(glm::vec3 pos, glm::vec3 color, int count, float speedScale);
void timer(int value);
void initGame();