#pragma once
#include "Common.h"

// 상수
extern const int LINES_PER_SEASON;
extern const float MOVE_DURATION;
extern const float JUMP_HEIGHT;
extern const unsigned int SHADOW_WIDTH, SHADOW_HEIGHT;
extern const float DASH_DURATION;
extern const float DASH_COOLDOWN;
extern const int DASH_COST;
extern const float FLY_DURATION;
extern const float FLY_SPEED;
extern const float FLY_HEIGHT;
extern const float LANDING_DURATION;
extern const float BIRD_LEAVE_DURATION;
extern const float NIGHT_DURATION;
extern const float WARNING_DURATION;
extern const float WIND_DURATION;
extern const float TAP_PER_SECOND;

// 변수
extern GameSeason currentSeason;
extern std::map<GameSeason, SeasonColors> seasonThemes;
extern int linesPassedSinceSeasonChange;

extern std::vector<Particle> particles;
extern std::vector<Train> trains;
extern std::vector<Log> logs;
extern std::vector<LilyPad> lilyPads;
extern std::vector<Cloud> clouds;
extern std::vector<WeatherParticle> weatherParticles;
extern std::vector<Item> items;
extern std::vector<Car> cars;
extern std::vector<CarDesign> carDesigns;
extern std::vector<Coin> coins;
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
extern int coinCount;
extern int riverRemaining;

extern bool isDashing;
extern float dashTimer;
extern float dashCooldownTimer;
extern glm::vec3 lightPos;

extern bool isEventActive;
extern float eventDuration;
extern float eventProgress;
extern int requiredTaps;
extern int lastEventScore;

extern bool isFlying;
extern float flyTimer;
extern bool isLanding;
extern float landingTime;
extern bool isBirdLeaving;
extern glm::vec3 birdStartPos;
extern glm::vec3 birdTargetPos;
extern float birdLeaveTime;
extern float recoveryTimer;
extern bool isLandingSuccess;

extern int introState;
extern GLuint logoTexture;
extern float introTimer;
extern float logoX, logoTargetX, logoStartX, logoY;

extern bool isGlitchMode;
extern float glitchTimer;

extern bool isGiantMode;
extern float giantTimer;
extern float giantGauge;

extern bool isNightMode;
extern float nightModeTimer;
extern float nightEventCooldown;
extern bool isNightWarning;
extern float nightWarningTimer;

extern float shakeTimer;
extern float shakeMagnitude;

extern bool isWindActive;
extern float windTimer;
extern float windForce;

extern bool hasShield;
extern bool isMagnetActive;
extern float magnetTimer;
extern bool isSlowActive;
extern float slowTimer;

extern bool isGameOver;

extern stbtt_bakedchar cdata[96];
extern GLuint fontTexture;

extern GLuint riverTexture;
extern GLuint grassTexture;
extern GLuint logTexture;
extern GLuint lilyPadTexture;

extern bool isGodMode;