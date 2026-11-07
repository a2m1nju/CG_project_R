#pragma once
#include "Common.h"
#include "ItemSystem.h"
#include "ItemSystem.h"

// 상수
extern const int LINES_PER_SEASON;
extern const float MOVE_DURATION;
extern const float JUMP_HEIGHT;
extern const unsigned int SHADOW_WIDTH, SHADOW_HEIGHT;
// 변수
extern GameSeason currentSeason;
extern std::map<GameSeason, SeasonColors> seasonThemes;
extern int linesPassedSinceSeasonChange;

extern std::vector<Particle> particles;
extern std::vector<Train> trains;
extern std::vector<Log> logs;
extern std::vector<LilyPad> lilyPads;
extern std::vector<WeatherParticle> weatherParticles;
extern std::vector<Item> items;
extern ItemSystem localItems;
extern ObjectId nextItemId;
extern std::string itemFeedbackMessage;
extern int itemFeedbackUntilMs;
extern std::vector<Car> cars;
extern std::vector<CarDesign> carDesigns;
extern std::map<int, int> mapType;
extern std::map<int, std::vector<int>> treeMap;

extern GLuint vao, vbo;
extern GLuint transLoc;
extern glm::vec3 playerPos;
extern glm::vec3 playerTargetPos;
extern glm::vec3 playerStartPos;
extern float playerRotation;
extern bool isMoving;
extern float moveTime;
extern glm::mat4 PV;
extern GLuint depthFBO, depthMap;
extern GLuint depthShader;
extern GLuint shaderProgramID; // 메인 쉐이더
extern GLuint vertexShader;
extern GLuint fragmentShader;

extern int score;
extern int minZ;
extern int riverRemaining;

extern glm::vec3 lightPos;

extern int introState;
extern GLuint logoTexture;
extern float introTimer;
extern float logoX, logoTargetX, logoStartX, logoY;

extern float shakeTimer;
extern float shakeMagnitude;

extern bool isGameOver;

extern stbtt_bakedchar cdata[96];
extern GLuint fontTexture;

extern GLuint riverTexture;
extern GLuint grassTexture;
extern GLuint logTexture;
extern GLuint lilyPadTexture;

extern bool isGodMode;