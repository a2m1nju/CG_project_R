#include "Global.h"

const int LINES_PER_SEASON = 30;
const float MOVE_DURATION = 0.20f;
const float JUMP_HEIGHT = 1.0f;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
const float DASH_DURATION = 1.0f;
const float DASH_COOLDOWN = 3.0f;
const int DASH_COST = 7;
const float FLY_DURATION = 3.0f;
const float FLY_SPEED = 0.8f;
const float FLY_HEIGHT = 8.0f;
const float LANDING_DURATION = 1.5f;
const float BIRD_LEAVE_DURATION = 1.0f;
const float NIGHT_DURATION = 10.0f;
const float WARNING_DURATION = 3.0f;
const float WIND_DURATION = 6.0f;
const float TAP_PER_SECOND = 3.0f;

GameSeason currentSeason = SUMMER;
std::map<GameSeason, SeasonColors> seasonThemes = {
    {SUMMER, {glm::vec3(0.47f, 0.9f, 0.42f), glm::vec3(1.0f, 0.2f, 0.0f), glm::vec3(0.2f, 0.7f, 0.2f), glm::vec3(0.1f, 0.6f, 0.1f), glm::vec3(0.0f, 0.5f, 0.0f)}},
    {AUTUMN, {glm::vec3(0.8f, 0.7f, 0.3f), glm::vec3(0.6f, 0.25f, 0.05f), glm::vec3(1.0f, 0.5f, 0.0f), glm::vec3(0.8f, 0.3f, 0.1f), glm::vec3(0.5f, 0.1f, 0.1f)}},
    {WINTER, {glm::vec3(0.9f, 0.95f, 1.0f), glm::vec3(0.6f, 0.4f, 0.2f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.9f, 0.9f, 0.9f), glm::vec3(0.8f, 0.8f, 0.8f)}},
    {SPRING, {glm::vec3(1.0f, 0.6f, 0.8f), glm::vec3(0.9f, 0.5f, 0.3f), glm::vec3(1.0f, 0.7f, 0.8f), glm::vec3(0.8f, 0.5f, 0.6f), glm::vec3(0.6f, 0.3f, 0.4f)}}
};
int linesPassedSinceSeasonChange = 0;

std::vector<Particle> particles;
std::vector<Train> trains;
std::vector<Log> logs;
std::vector<LilyPad> lilyPads;
std::vector<Cloud> clouds;
std::vector<WeatherParticle> weatherParticles;
std::vector<Item> items;
std::vector<Car> cars;
std::vector<CarDesign> carDesigns;
std::vector<Coin> coins;
std::map<int, int> mapType;
std::map<int, std::vector<int>> treeMap;

GLuint vao, vbo;
GLuint transLoc;
glm::vec3 playerPos(0.0f, 0.5f, 0.0f);
glm::vec3 playerTargetPos = playerPos;
glm::vec3 playerStartPos = playerPos;
float playerRotation = 0.0f;
bool isMoving = false;
float moveTime = 0.0f;
glm::mat4 PV;
GLuint depthFBO, depthMap;
GLuint depthShader;
GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;

int score = 0;
int minZ = 0;
int coinCount = 0;
int riverRemaining = 0;

bool isDashing = false;
float dashTimer = 0.0f;
float dashCooldownTimer = 0.0f;
glm::vec3 lightPos(-15.0f, 20.0f, 0.0f);

bool isEventActive = false;
float eventDuration = 4.0f;
float eventProgress = 0.0f;
int requiredTaps = 0;
int lastEventScore = 0;

bool isFlying = false;
float flyTimer = 0.0f;
bool isLanding = false;
float landingTime = 0.3f;
bool isBirdLeaving = false;
glm::vec3 birdStartPos;
glm::vec3 birdTargetPos;
float birdLeaveTime = 0.0f;
float recoveryTimer = 0.0f;
bool isLandingSuccess = false;

int introState = 1;
GLuint logoTexture;
float introTimer = 0.0f;
float logoX = -1300.0f;
float logoTargetX = 640.0f;
float logoStartX = -1300.0f;
float logoY = 600.0f;

bool isGlitchMode = false;
float glitchTimer = 0.0f;

bool isGiantMode = false;
float giantTimer = 0.0f;
//float giantGauge = 0.0f;

bool isNightMode = false;
float nightModeTimer = 0.0f;
float nightEventCooldown = 10.0f;
bool isNightWarning = false;
float nightWarningTimer = 0.0f;

float shakeTimer = 0.0f;
float shakeMagnitude = 0.0f;

bool isWindActive = false;
float windTimer = 0.0f;
float windForce = 0.0f;

bool hasShield = false;
bool isMagnetActive = false;
float magnetTimer = 0.0f;
bool isSlowActive = false;
float slowTimer = 0.0f;

bool isGameOver = false;

stbtt_bakedchar cdata[96];
GLuint fontTexture;

GLuint riverTexture;
GLuint grassTexture;
GLuint logTexture;
GLuint lilyPadTexture;