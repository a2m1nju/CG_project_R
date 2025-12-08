#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <ctime>
#include <algorithm>
#include <string>
#include <Windows.h>
#include <mmsystem.h>

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#include "stb_truetype.h" // 폰트 구조체 필요

// --- Enums ---
enum GameSeason { SPRING, SUMMER, AUTUMN, WINTER };
enum TrainState { TRAIN_IDLE, TRAIN_WARNING, TRAIN_PASSING };
enum ItemType { ITEM_SHIELD, ITEM_MAGNET, ITEM_CLOCK, ITEM_POTION };

// --- Structs ---
struct Car {
    float x, z;
    float speed;
    glm::vec3 color;
    int designID;
};

struct Coin {
    float x, z;
    bool isCollected = false;
};

struct CarPart {
    glm::vec3 offset;
    glm::vec3 scale;
    glm::vec3 color;
};

struct CarDesign {
    std::vector<CarPart> parts;
    float baseScale;
};

struct SeasonColors {
    glm::vec3 grass;
    glm::vec3 treeTrunk;
    glm::vec3 treeFoliageLight;
    glm::vec3 treeFoliageMedium;
    glm::vec3 treeFoliageDark;
};

struct Log {
    float x, z;
    float speed;
    float width;
};

struct LilyPad {
    float x, z;
};

struct Particle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 color;
    float scale;
    float life;
};

struct Train {
    float z, x, speed;
    TrainState state;
    int timer;
    bool isLightOn;
};

struct Cloud {
    glm::vec3 position;
    float scale;
    float speed;
};

struct WeatherParticle {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 scaleVec;
    float speed;
    float sway;
    float swayPhase;
};

struct Item {
    float x, z;
    ItemType type;
    bool isCollected = false;
    float rotation = 0.0f;
};