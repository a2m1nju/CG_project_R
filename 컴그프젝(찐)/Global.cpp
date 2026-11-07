#include "Global.h"

const int LINES_PER_SEASON = 30;
const float MOVE_DURATION = 0.20f;
const float JUMP_HEIGHT = 1.0f;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
const float DASH_DURATION = 1.0f;
const float DASH_COOLDOWN = 3.0f;
const int DASH_COST = 7;
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
std::vector<WeatherParticle> weatherParticles;
std::vector<Item> items;
ItemSystem localItems;
ObjectId nextItemId = 1;
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

float recoveryTimer = 0.0f;

int introState = 1;
GLuint logoTexture;
float introTimer = 0.0f;
float logoX = -1300.0f;
float logoTargetX = 640.0f;
float logoStartX = -1300.0f;
float logoY = 600.0f;

float shakeTimer = 0.0f;
float shakeMagnitude = 0.0f;

bool isGameOver = false;

stbtt_bakedchar cdata[96];
GLuint fontTexture;

GLuint riverTexture;
GLuint grassTexture;
GLuint logTexture;
GLuint lilyPadTexture;

bool isGodMode = false;