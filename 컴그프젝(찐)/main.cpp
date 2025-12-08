#define _CRT_SECURE_NO_WARNINGS
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_truetype.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <ctime>
#include <algorithm>
#include <string>
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "freeglut.lib")
#pragma warning(disable: 4711 4710 4100)

// 구조체 정의
struct Car {
	float x, z;
	float speed;
	glm::vec3 color;
	int designID;
};

struct Coin {
	float x, z;
	bool isCollected = false; // 획득 여부
};

//자동차 부품
struct CarPart {
	glm::vec3 offset; // 차량 중앙을 기준으로 한 상대적 위치
	glm::vec3 scale; // 크기
	glm::vec3 color; // 부품의 색상
};

// 자동차 모델 정의
struct CarDesign {
	std::vector<CarPart> parts;
	float baseScale; //전체크기조절
};

enum GameSeason {
	SPRING,
	SUMMER,
	AUTUMN,
	WINTER
};
GameSeason currentSeason = SUMMER;

// 계절별 색상 정의 구조체
struct SeasonColors {
	glm::vec3 grass;
	glm::vec3 treeTrunk;
	glm::vec3 treeFoliageLight;
	glm::vec3 treeFoliageMedium;
	glm::vec3 treeFoliageDark;
};
//계절 색상 정의
std::map<GameSeason, SeasonColors> seasonThemes = {
{SUMMER, {
glm::vec3(0.47f, 0.9f, 0.42f), // grass (연한 초록)
glm::vec3(1.0f, 0.2f, 0.0f), // treeTrunk (갈색/주황)
glm::vec3(0.2f, 0.7f, 0.2f), // foliageLight (밝은 초록)
glm::vec3(0.1f, 0.6f, 0.1f), // foliageMedium
glm::vec3(0.0f, 0.5f, 0.0f) // foliageDark (진한 초록)
}},
{AUTUMN, {
glm::vec3(0.8f, 0.7f, 0.3f), // grass (황토색/누런색)
glm::vec3(0.6f, 0.25f, 0.05f), // treeTrunk (진한 갈색)
glm::vec3(1.0f, 0.5f, 0.0f), // foliageLight (주황)
glm::vec3(0.8f, 0.3f, 0.1f), // foliageMedium (빨강/벽돌)
glm::vec3(0.5f, 0.1f, 0.1f) // foliageDark (진한 빨강)
}},
{WINTER, {
glm::vec3(0.9f, 0.95f, 1.0f), // grass (흰색/옅은 하늘색 눈밭)
glm::vec3(0.6f, 0.4f, 0.2f), // treeTrunk
glm::vec3(1.0f, 1.0f, 1.0f), // foliageLight (하얀 눈 덮인 나뭇잎)
glm::vec3(0.9f, 0.9f, 0.9f), // foliageMedium
glm::vec3(0.8f, 0.8f, 0.8f) // foliageDark
}},
{SPRING, {
glm::vec3(1.0f, 0.6f, 0.8f), // grass
glm::vec3(0.9f, 0.5f, 0.3f), // treeTrunk
glm::vec3(1.0f, 0.7f, 0.8f), // foliageLight (벚꽃 핑크)
glm::vec3(0.8f, 0.5f, 0.6f), // foliageMedium
glm::vec3(0.6f, 0.3f, 0.4f) // foliageDark
}}
};
const int LINES_PER_SEASON = 30; // 30 라인마다 계절 전환
int linesPassedSinceSeasonChange = 0; // 계절이 바뀐 후 통과한 라인 수 (minZ 기준)

// 통나무 구조체
struct Log {
	float x, z;
	float speed;
	float width; // 통나무 길이
};

// 연잎 구조체
struct LilyPad {
	float x, z;
};

// 파티클 구조체
struct Particle {
	glm::vec3 position;
	glm::vec3 velocity; // 이동 속도 및 방향
	glm::vec3 color;
	float scale;
	float life; // 수명 (1.0에서 시작해 0.0이 되면 사라짐)
};

// 파티클 관리 벡터
std::vector<Particle> particles;

// 기차 상태 열거형
enum TrainState {
	TRAIN_IDLE, // 평상시 (아무것도 없음)
	TRAIN_WARNING, // 경고 (신호등 깜빡임)
	TRAIN_PASSING // 통과 (기차가 매우 빠르게 지나감)
};

// 기차 구조체
struct Train {
	float z;
	float x;
	float speed;
	TrainState state;
	int timer; // 상태 변경을 위한 타이머
	bool isLightOn; // 신호등 깜빡임용
};

// 전역 벡터
std::vector<Train> trains;

// 전역 벡터
std::vector<Log> logs;
std::vector<LilyPad> lilyPads;

struct Cloud {
	glm::vec3 position;
	float scale;
	float speed; // 구름이 옆으로 흘러가는 속도
};
std::vector<Cloud> clouds; // 전역 구름 벡터

// 날씨 파티클 구조체 
struct WeatherParticle {
	glm::vec3 pos;       // 위치
	glm::vec3 color;     // 색상 (계절별로 다름)
	glm::vec3 scaleVec;  // 크기/모양 (눈은 정육면체, 꽃잎은 납작하게)
	float speed;         // 떨어지는 속도
	float sway;          // 흔들림 정도
	float swayPhase;     // 흔들림 주기
};
std::vector<WeatherParticle> weatherParticles;

// [추가] 아이템 종류 열거형
enum ItemType {
	ITEM_SHIELD, // 보호막 (파란색)
	ITEM_MAGNET, // 자석 (빨간색)
	ITEM_CLOCK   // 시간 느려짐 (하늘색)
};

// [추가] 아이템 구조체
struct Item {
	float x, z;
	ItemType type;
	bool isCollected = false;
	float rotation = 0.0f; // 빙글빙글 돌리기용
};
std::vector<Item> items;

// 전역 변수
GLuint vao, vbo;
GLuint transLoc;
glm::vec3 playerPos(0.0f, 0.5f, 0.0f);
glm::vec3 playerTargetPos = playerPos;
glm::vec3 playerStartPos = playerPos; //애니메이션 시작 위치
float playerRotation = 0.0f;
bool isMoving = false;
float moveTime = 0.0f;
const float MOVE_DURATION = 0.20f; //한칸이동시간
const float JUMP_HEIGHT = 1.0f; //최대점프높이
glm::mat4 PV; // 나무 그리기 함수와 공유할 전역 행렬
GLuint depthFBO, depthMap;
GLuint depthShader;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;

int score = 0;
int minZ = 0; // 플레이어가 도달한 최대 전진 위치
int coinCount = 0; // 획득한 코인 개수

int riverRemaining = 0; // 현재 강 구간이 진행 중인지 저장하는 전역 변수

bool isDashing = false;
float dashTimer = 0.0f; // 대쉬 남은 시간
const float DASH_DURATION = 1.0f; // 대쉬 지속 시간 (1초)
const float DASH_COOLDOWN = 3.0f; // 대쉬 재사용 대기 시간 (3초)
float dashCooldownTimer = 0.0f; // 재사용 대기 시간 타이머
const int DASH_COST = 7; // 대쉬에 필요한 코인 개수
glm::vec3 lightPos(-15.0f, 20.0f, 0.0f);

bool isEventActive = false; // 현재 스페이스 연타 이벤트가 활성화되었는가
float eventDuration = 4.0f; // 연타 이벤트 지속 시간 (4초)
float eventProgress = 0.0f; // 연타 진행도 (0.0 ~ 1.0)
const float TAP_PER_SECOND = 3.0f; // 1초당 필요한 연타 횟수 (게이지 상승률)
int requiredTaps = 0; // 연타 이벤트 중에 눌린 횟수 카운트

bool isFlying = false; // 현재 새를 타고 공중 비행 중인가
float flyTimer = 0.0f; // 비행 남은 시간
const float FLY_DURATION = 3.0f; // 비행 지속 시간 (3초)
const float FLY_SPEED = 0.8f; // 비행 중 전진 속도
int lastEventScore = 0;

const float FLY_HEIGHT = 8.0f;
bool isLanding = false;
float landingTime = 0.3f;
const float LANDING_DURATION = 0.5f;

bool isBirdLeaving = false;
glm::vec3 birdStartPos;
glm::vec3 birdTargetPos;
float birdLeaveTime = 0.0f;
const float BIRD_LEAVE_DURATION = 1.0f; // 새가 떠나는 데 걸리는 시간

// 핀 조명 이벤트
bool isNightMode = false;       // 현재 핀 조명 모드인가?
float nightModeTimer = 0.0f;    // 핀 조명 지속 시간
float nightEventCooldown = 10.0f; // 다음 이벤트 발생까지 남은 시간
const float NIGHT_DURATION = 10.0f;

// 핀 조명 경고(예고) 관련 변수
bool isNightWarning = false;        // 현재 경고 중인가?
float nightWarningTimer = 0.0f;     // 경고 지속 시간
const float WARNING_DURATION = 3.0f; // 3초 동안 경고 후 암전

// 기차 카메라 흔들림 관련 변수
float shakeTimer = 0.0f;      // 흔들림 지속 시간
float shakeMagnitude = 0.0f;  // 흔들림 강도 (0.0 ~ 1.0)

// 바람 이벤트 관련 변수
bool isWindActive = false;      // 바람이 불고 있는가?
float windTimer = 0.0f;         // 바람 지속 시간
float windForce = 0.0f;         // 바람의 세기 및 방향 (+는 우측, -는 좌측)
const float WIND_DURATION = 6.0f; // 바람 지속 시간 (6초)

// 아이템 관련 전역 변수
bool hasShield = false;          // 보호막 보유 여부
bool isMagnetActive = false;     // 자석 활성화 여부
float magnetTimer = 0.0f;        // 자석 남은 시간
bool isSlowActive = false;       // 슬로우 모션 활성화 여부
float slowTimer = 0.0f;          // 슬로우 모션 남은 시간

std::map<int, int> mapType; // 0=잔디 1=도로
std::map<int, std::vector<int>> treeMap;
std::vector<Car> cars;
std::vector<CarDesign> carDesigns;
std::vector<Coin> coins; // 코인 목록

// 폰트 관련 전역 변수
stbtt_bakedchar cdata[96];
GLuint fontTexture;

// 함수 선언
GLvoid drawScene(GLvoid);
GLvoid Reshape(int w, int h);
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
char* filetobuf(const char* file);

void initGame();
void generateLane(int z);
void initFont(const char* filename, float pixelHeight);
void specialKeyboard(int key, int x, int y);
void timer(int value);
bool isTreeAt(int x, int z);
void drawTree(int x, int z); // 복셀 나무 그리기 함수
void renderTextTTF(float x, float y, const char* text, float r, float g, float b);
void keyboard(unsigned char key, int x, int y);
void loadDepthShader();
void drawCloud(const Cloud& cloud, GLuint shader); // drawCloud가 다른 곳에서 호출된다면 추가
void drawClouds(GLuint shader);

GLuint riverTexture;
GLuint grassTexture;
GLuint logTexture;
GLuint lilyPadTexture;

GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;

void loadTexture(const char* path, GLuint* textureID) {
	glGenTextures(1, textureID);
	glBindTexture(GL_TEXTURE_2D, *textureID);

	// 텍스처 래핑 및 필터링 설정
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // 이미지 Y축 뒤집기 [cite: 456]

	unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
	if (data) {
		GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else {
		std::cout << "Failed to load texture: " << path << std::endl;
	}
	stbi_image_free(data);
}

int main(int argc, char** argv)
{
	srand((unsigned int)time(NULL));

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(1280, 960);
	glutCreateWindow("Crossy Road - Blocky Trees");

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		std::cerr << "Unable to initialize GLEW" << std::endl;
		exit(EXIT_FAILURE);
	}

	make_vertexShaders();
	make_fragmentShaders();
	shaderProgramID = make_shaderProgram();
	loadDepthShader();

	initGame();
	initFont("Cafe24PROUP.ttf", 60.0f);

	glEnable(GL_DEPTH_TEST);

	glutDisplayFunc(drawScene);
	glutReshapeFunc(Reshape);
	glutSpecialFunc(specialKeyboard);
	glutKeyboardFunc(keyboard);
	glutTimerFunc(16, timer, 0);

	glutMainLoop();
}

GameSeason getSeasonByZ(int z) {
	//z는 음수 -> 양수로 변환
	int linesPassed = -z;
	int seasonIndex = (linesPassed / LINES_PER_SEASON) % 4; // 0, 1, 2, 3 로 순환

	// SUMMBER=1, AUTUMN=2, WINTER=3, SPRING=0

	// 초기 계절: SUMMER (0~29라인)
	if (seasonIndex == 0) return SUMMER;
	// 다음 계절: AUTUMN (30~59라인)
	if (seasonIndex == 1) return AUTUMN;
	// 다음 계절: WINTER (60~89라인)
	if (seasonIndex == 2) return WINTER;
	// 마지막 계절: SPRING (90~119라인)
	if (seasonIndex == 3) return SPRING;

	return SUMMER; // 안전장치
}

void drawTree(int x, int z, GLuint shader) {

	const SeasonColors& colors = seasonThemes[getSeasonByZ(z)];
	glm::mat4 model;
	//glm::mat4 MVP;

	// 1. 나무 기둥-갈색
	for (int i = 0; i < 1; ++i) {
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3((float)x, 0.5f + (float)i, (float)z));
		model = glm::scale(model, glm::vec3(0.4f, 1.0f, 0.4f)); // 얇은 기둥

		//glUniformMatrix4fv(transLoc, 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model)); //

		if (shader == shaderProgramID) {
			glVertexAttrib3f(1, colors.treeTrunk.r, colors.treeTrunk.g, colors.treeTrunk.b);
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 2. 나뭇잎 위로 반듯하게 쌓은 형태
	for (int yOffset = 0; yOffset < 4; ++yOffset) { // 3층 높이
		for (int dx = -1; dx <= 1; ++dx) { // 가로 3칸
			for (int dz = -1; dz <= 1; ++dz) { // 세로 3칸

				model = glm::mat4(1.0f);

				float scale = 0.35f;
				model = glm::translate(model, glm::vec3(x + dx * scale, 1.2f + yOffset * scale, z + dz * scale));
				model = glm::scale(model, glm::vec3(scale, scale, scale));

				glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model)); //

				if (shader == shaderProgramID) {
					glm::vec3 foliageColor;
					if (yOffset == 0) foliageColor = colors.treeFoliageDark;
					else if (yOffset == 1) foliageColor = colors.treeFoliageMedium;
					else foliageColor = colors.treeFoliageLight;
					glVertexAttrib3f(1, foliageColor.r, foliageColor.g, foliageColor.b);
				}
				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}
	}
}

void drawCar(const Car& car, GLuint shader) {
	if (car.designID < 0 || car.designID >= carDesigns.size()) return;

	const CarDesign& design = carDesigns[car.designID];
	glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), glm::vec3(car.x, 0.5f, car.z));

	//차량크기조절
	baseModel = glm::scale(baseModel, glm::vec3(design.baseScale));

	for (const auto& part : design.parts) {
		glm::mat4 model = baseModel;

		// 상대적 위치 이동
		model = glm::translate(model, part.offset);
		// 부품 크기 조절
		model = glm::scale(model, part.scale);

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

		if (shader == shaderProgramID) {
			// 부품에 정의된 색상이 없으면 Car 구조체의 기본 색상 사용
			if (part.color == glm::vec3(0.0f)) {
				glVertexAttrib3f(1, car.color.r, car.color.g, car.color.b);
			}
			else {
				glVertexAttrib3f(1, part.color.r, part.color.g, part.color.b);
			}
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

void generateLane(int z)
{
	// 현재 줄이 이미 생성되어 있다면 패스
	if (mapType.find(z) != mapType.end()) return;

	// 시작 지점 안전지대 (-2 ~ 2)
	if (z >= -2 && z <= 2) {
		mapType[z] = 0;
		return;
	}

	static int lastRiverZ = 0; // 마지막으로 강이 생성된 위치 기억

	int growDir = (z < 0) ? -1 : 1;

	int prevZ = z - growDir;
	bool prevWasRiver = (mapType.count(prevZ) && mapType[prevZ] == 2);

	bool forceRiver = (abs(z - lastRiverZ) > 15);

	int randVal = rand() % 10;

	if (!forceRiver && randVal < 6) {
		mapType[z] = 1; // 도로

		int numCars = 1 + rand() % 2;
		float speed = (0.05f + (rand() % 3) / 50.0f);
		if (rand() % 2 == 0) speed *= -1.0f;

		for (int i = 0; i < numCars; ++i) {
			Car newCar;
			newCar.z = (float)z;
			newCar.x = (float)(rand() % 30 - 15);
			newCar.speed = speed;
			newCar.color = glm::vec3((rand() % 10) / 10.f, (rand() % 10) / 10.f, (rand() % 10) / 10.f);
			if (newCar.color.r < 0.2f) newCar.color.r += 0.5f;
			newCar.designID = (!carDesigns.empty()) ? rand() % carDesigns.size() : 0;
			cars.push_back(newCar);
		}
	}

	// 강
	else if ((forceRiver || randVal < 7) && !prevWasRiver) {

		int riverWidth = 2 + rand() % 3; // 2~4줄

		bool canPlaceRiver = true;
		for (int k = 1; k < riverWidth; ++k) {

			if (mapType.count(z + (k * growDir))) {
				canPlaceRiver = false;
				break;
			}
		}

		if (canPlaceRiver) {
			lastRiverZ = z; // 마지막 강 위치 갱신
			int pathX = (rand() % 9) - 4; // 정답 경로

			for (int k = 0; k < riverWidth; ++k) {
				int currentZ = z + (k * growDir);
				mapType[currentZ] = 2; // 강 타입

				bool isLogLane = (rand() % 2 == 0);

				if (isLogLane) { // 통나무
					float speed = 0.03f + (rand() % 3) / 100.0f;
					if (rand() % 2 == 0) speed *= -1.0f;

					// 안전 통나무
					Log safeLog;
					safeLog.z = (float)currentZ;
					safeLog.width = 5.0f;
					safeLog.speed = speed;
					safeLog.x = (float)pathX;
					logs.push_back(safeLog);

					// 랜덤 통나무
					int extraLogs = 1 + rand() % 2;
					for (int i = 0; i < extraLogs; i++) {
						Log log;
						log.z = (float)currentZ;
						log.width = 2.0f + (rand() % 3) * 0.5f;
						log.speed = speed;
						int randX = (rand() % 30 - 15);
						if (abs(randX - pathX) > 5) {
							log.x = (float)randX;
							logs.push_back(log);
						}
					}
					pathX += (speed > 0) ? 1 : -1;
				}
				else { // 연잎
					// 안전 연잎 뭉치
					for (int offset = -1; offset <= 1; ++offset) {
						LilyPad pad;
						pad.z = (float)currentZ;
						pad.x = (float)(pathX + offset);
						if (pad.x > -15 && pad.x < 15) lilyPads.push_back(pad);
					}
					// 장식 연잎
					for (int x = -14; x <= 14; x++) {
						if (abs(x - pathX) <= 2) continue;
						if (rand() % 100 < 35) {
							LilyPad pad;
							pad.z = (float)currentZ;
							pad.x = (float)x;
							lilyPads.push_back(pad);
						}
					}
				}
				pathX += (rand() % 3 - 1);
				if (pathX < -12) pathX = -12;
				if (pathX > 12) pathX = 12;
			}
		}
		else {
			goto MAKE_GRASS; // 공간이 없으면 잔디로 변경
		}
	}
	// 철길
	else if (randVal < 8) {
		mapType[z] = 3; // 철길 타입 3번

		// 기차 객체 생성
		Train newTrain;
		newTrain.z = (float)z;
		newTrain.x = -50.0f; // 화면 밖 대기
		newTrain.speed = 0.0f;
		newTrain.state = TRAIN_IDLE;
		newTrain.timer = rand() % 200 + 100; // 100~300 프레임 후 발동
		newTrain.isLightOn = false;

		trains.push_back(newTrain);
	}

	// 잔디
	else {
	MAKE_GRASS:
		mapType[z] = 0; // 잔디

		// 나무 심기 (기존 코드)
		for (int x = -15; x <= 15; ++x) {
			if (rand() % 10 < 1) {
				treeMap[z].push_back(x);
			}
		}

		// 코인 및 아이템 생성
		if (rand() % 2 == 0) {
			int spawnX = (rand() % 21) - 10;
			if (!isTreeAt(spawnX, z)) {

				int luck = rand() % 100;

				if (luck < 20) {
					Item newItem;
					newItem.x = (float)spawnX;
					newItem.z = (float)z;
					// 랜덤 아이템 타입 (0, 1, 2 중 하나)
					newItem.type = (ItemType)(rand() % 3);
					items.push_back(newItem);
				}
				// 나머지 확률로 코인 생성 (기존 로직)
				else {
					Coin newCoin;
					newCoin.x = (float)spawnX;
					newCoin.z = (float)z;
					coins.push_back(newCoin);
				}
			}
		}
	}
}

// 큐브 그리기 헬퍼 함수 (부품 하나하나 그릴 때 사용)
void drawPart(GLuint shader, glm::mat4 parentModel, glm::vec3 offset, glm::vec3 scale, glm::vec3 color) {
	glm::mat4 model = glm::translate(parentModel, offset);
	model = glm::scale(model, scale);

	glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

	// 쉐이더가 메인 쉐이더일 때만 색상 적용 (그림자 맵 만들 때는 색상 필요 없음)
	if (shader == shaderProgramID) {
		glVertexAttrib3f(1, color.r, color.g, color.b);
	}
	glDrawArrays(GL_TRIANGLES, 0, 36);
}

// 닭 캐릭터 그리기 함수
void drawChicken(GLuint shader, glm::mat4 baseModel) {

	// 색상 정의
	glm::vec3 white(1.0f, 1.0f, 1.0f);
	glm::vec3 red(1.0f, 0.2f, 0.2f);
	glm::vec3 orange(1.0, 0.270588f, 0.0f);
	glm::vec3 black(0.1f, 0.1f, 0.1f);

	// 1. 몸통 (Body) - 중심점 조정
	drawPart(shader, baseModel, glm::vec3(0.0f, -0.1f, 0.0f), glm::vec3(0.6f, 0.6f, 0.6f), white);

	// 2. 벼슬 (Comb)
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.25f, 0.1f), glm::vec3(0.15f, 0.15f, 0.2f), red);

	// 3. 부리 (Beak)
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, -0.35f), glm::vec3(0.15f, 0.15f, 0.15f), orange);

	// 4. 와틀 (Wattle)
	drawPart(shader, baseModel, glm::vec3(0.0f, -0.15f, -0.35f), glm::vec3(0.1f, 0.12f, 0.1f), red);

	// 5. 눈 (Eyes)
	drawPart(shader, baseModel, glm::vec3(0.31f, 0.0f, -0.2f), glm::vec3(0.05f, 0.05f, 0.05f), black);
	drawPart(shader, baseModel, glm::vec3(-0.31f, 0.0f, -0.2f), glm::vec3(0.05f, 0.05f, 0.05f), black);

	// 6. 날개 (Wings)
	drawPart(shader, baseModel, glm::vec3(0.32f, -0.2f, 0.05f), glm::vec3(0.1f, 0.3f, 0.4f), white);
	drawPart(shader, baseModel, glm::vec3(-0.32f, -0.2f, 0.05f), glm::vec3(0.1f, 0.3f, 0.4f), white);

	// 8. 다리 (Legs)
	drawPart(shader, baseModel, glm::vec3(0.2f, -0.25f, 0.0f), glm::vec3(0.1f, 0.5f, 0.1f), orange);
	drawPart(shader, baseModel, glm::vec3(-0.2f, -0.25f, 0.0f), glm::vec3(0.1f, 0.5f, 0.1f), orange);

	// 9. 발 (Feet)
	drawPart(shader, baseModel, glm::vec3(0.2f, -0.49f, -0.3f), glm::vec3(0.15f, 0.02f, 0.25f), orange);
	drawPart(shader, baseModel, glm::vec3(-0.2f, -0.49f, -0.3f), glm::vec3(0.15f, 0.02f, 0.25f), orange);

	// 9. 꼬리 (Tail)
	drawPart(shader, baseModel, glm::vec3(0.0f, -0.1f, 0.35f), glm::vec3(0.3f, 0.3f, 0.1f), white);
}

void drawBird(GLuint shader, glm::mat4 baseModel) {
	// 색상 정의 (독수리 같은 갈색/흰색 새)
	glm::vec3 brown(0.4f, 0.2f, 0.0f);
	glm::vec3 white(0.9f, 0.9f, 0.9f);
	glm::vec3 yellow(1.0f, 0.8f, 0.0f);

	// 1. 몸통
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.5f, 0.6f), brown);

	// 2. 머리
	drawPart(shader, baseModel, glm::vec3(0.5f, 0.15f, 0.0f), glm::vec3(0.4f, 0.4f, 0.4f), white);

	// 3. 부리
	drawPart(shader, baseModel, glm::vec3(0.7f, 0.15f, 0.0f), glm::vec3(0.2f, 0.1f, 0.1f), yellow);

	// 4. 꼬리
	drawPart(shader, baseModel, glm::vec3(-0.55f, 0.0f, 0.0f), glm::vec3(0.3f, 0.2f, 0.6f), brown);

	// 5. 날개 (양쪽)
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, 0.4f), glm::vec3(0.1f, 0.1f, 1.2f), brown);
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, -0.4f), glm::vec3(0.1f, 0.1f, 1.2f), brown);
}

void drawCoin(const Coin& coin, GLuint shader) {
	if (coin.isCollected) return; // 이미 획득했으면 그리지 않음

	glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), glm::vec3(coin.x, 0.5f, coin.z));

	baseModel = glm::scale(baseModel, glm::vec3(0.5f)); // 코인 크기 조절 (0.5f)

	// 노란색 본체 및 빨간색 'C'의 높이
	glm::vec3 yellowColor = glm::vec3(1.0f, 0.9f, 0.0f); // 노란색
	glm::vec3 redColor = glm::vec3(1.0f, 0.0f, 0.0f); // 빨간색
	float C_height = 0.1f; // 'C' 모양 부품의 두께

	// 노란색 베이스의 Y 오프셋: baseModel의 Y=0.5f 위에서 시작
	float base_Y_offset = 0.0f;
	// 'C' 모양의 Y 오프셋: 노란색 베이스 (0.1f 높이) 위에 놓이도록
	float C_Y_offset = base_Y_offset + C_height;

	// Y축 기준으로 코인 전체를 살짝 더 띄웁니다. (코인 중앙이 Y=0.5f + 0.1f에 오도록 조정)
	// baseModel에 이미 y=0.5f가 있으므로, 부품 오프셋을 0.1f 위로 올립니다.
	C_Y_offset = 0.1f;
	base_Y_offset = 0.0f;

	float C_Y_POS = 0.1f; // 코인 베이스 위에 놓일 높이
	float C_Y_THICKNESS = 0.1f; // C 부품 자체의 두께

	//노란색
	// Y 오프셋은 0.0f, 스케일 Y는 0.1f로 얇게
	drawPart(shader, baseModel, glm::vec3(0.0f, base_Y_offset, 0.0f), glm::vec3(0.9f, C_Y_THICKNESS, 0.9f), yellowColor);

	// 2. 'C' 모양 구현 (빨간색 조각들)
	glm::vec3 redColor_C = glm::vec3(1.0f, 0.0f, 0.0f);
	float C_Y_offset_final = C_Y_POS; // 노란색 베이스 위에 놓일 높이
	float C_thick = 0.15f;
	float C_length = 0.4f;
	float C_span = 0.25f; // C자의 폭

	// A. 세로 막대 (왼쪽)
	drawPart(shader, baseModel,
		glm::vec3(-C_span, C_Y_offset_final, 0.0f),
		glm::vec3(C_thick, C_Y_THICKNESS, C_length), redColor_C);

	// B. 위쪽 막대 (위)
	drawPart(shader, baseModel,
		glm::vec3(-C_span / 2.0f + C_thick / 2.0f, C_Y_offset_final, C_length / 2.0f - C_thick / 2.0f),
		glm::vec3(C_span - C_thick / 2.0f, C_Y_THICKNESS, C_thick), redColor_C);

	// C. 아래쪽 막대 (아래)
	drawPart(shader, baseModel,
		glm::vec3(-C_span / 2.0f + C_thick / 2.0f, C_Y_offset_final, -C_length / 2.0f + C_thick / 2.0f),
		glm::vec3(C_span - C_thick / 2.0f, C_Y_THICKNESS, C_thick), redColor_C);
}

// 아이템 그리기 함수
void drawItem(const Item& item, GLuint shader) {
	if (item.isCollected) return;

	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(item.x, 0.5f, item.z));
	model = glm::rotate(model, glm::radians(item.rotation), glm::vec3(0.0f, 1.0f, 0.0f)); // 회전
	model = glm::scale(model, glm::vec3(0.4f)); // 크기 0.4

	glm::vec3 color;
	if (item.type == ITEM_SHIELD) color = glm::vec3(0.0f, 0.5f, 1.0f); // 파랑
	else if (item.type == ITEM_MAGNET) color = glm::vec3(1.0f, 0.0f, 0.0f); // 빨강
	else if (item.type == ITEM_CLOCK) color = glm::vec3(0.0f, 1.0f, 1.0f); // 하늘색

	// 큐브 모양 아이템
	glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
	if (shader == shaderProgramID) {
		glVertexAttrib3f(1, color.r, color.g, color.b);
	}
	glDrawArrays(GL_TRIANGLES, 0, 36);
}

// 철길 및 신호등 그리기
void drawRail(int z, bool isWarning, bool isLightOn, GLuint shader) {

	// 침목 (Sleepers) - 나무색
	glm::vec3 woodColor = glm::vec3(0.35f, 0.2f, 0.1f);

	for (float x = -16.0f; x <= 16.0f; x += 1.2f) { // 침목 간격
		glm::mat4 model = glm::mat4(1.0f);
		// Y좌표를 0.01f(바닥 바로 위)로 설정
		model = glm::translate(model, glm::vec3(x, 0.01f, (float)z));
		// 크기: 가로 0.6, 높이 0.05, 세로 1.4
		model = glm::scale(model, glm::vec3(0.3f, 0.05f, 1.4f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) glVertexAttrib3f(1, woodColor.r, woodColor.g, woodColor.b);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 철로 (Rails) - 은색
	glm::vec3 railColor = glm::vec3(0.6f, 0.6f, 0.7f);
	float railZPositions[] = { -0.3f, 0.3f }; // 두 줄

	for (float rz : railZPositions) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.06f, (float)z + rz));
		// 맵 전체를 가로지르는 긴 막대
		model = glm::scale(model, glm::vec3(32.0f, 0.08f, 0.1f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) glVertexAttrib3f(1, railColor.r, railColor.g, railColor.b);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 신호등 (Signal) - 오른쪽 배치
	float signalX = 7.0f;

	// (줄무늬 기둥
	for (int i = 0; i < 5; ++i) {
		glm::mat4 model = glm::mat4(1.0f);
		// 기둥 높이 쌓기
		model = glm::translate(model, glm::vec3(signalX, 0.0f + (i * 0.4f), (float)z - 0.8f));
		model = glm::scale(model, glm::vec3(0.2f, 0.4f, 0.2f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) {
			if (i % 2 == 0) glVertexAttrib3f(1, 0.9f, 0.9f, 0.9f); // 흰색
			else glVertexAttrib3f(1, 0.9f, 0.1f, 0.1f); // 빨간색
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 신호등 박스 (검은색)
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(signalX, 2.0f, (float)z - 0.8f));
		model = glm::scale(model, glm::vec3(0.8f, 0.4f, 0.3f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) glVertexAttrib3f(1, 0.1f, 0.1f, 0.1f);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 깜빡이는 불빛 (빨간색)
	glm::vec3 offColor = glm::vec3(0.3f, 0.0f, 0.0f);
	glm::vec3 onColor = glm::vec3(1.0f, 0.0f, 0.0f);

	// 왼쪽 불빛
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(signalX - 0.25f, 2.0f, (float)z - 0.65f));
		model = glm::scale(model, glm::vec3(0.25f, 0.25f, 0.1f));
		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) {
			if (isWarning && isLightOn) glVertexAttrib3f(1, onColor.r, onColor.g, onColor.b);
			else glVertexAttrib3f(1, offColor.r, offColor.g, offColor.b);
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 오른쪽 불빛 (교차 점멸)
	{
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(signalX + 0.25f, 2.0f, (float)z - 0.65f));
		model = glm::scale(model, glm::vec3(0.25f, 0.25f, 0.1f));
		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) {
			if (isWarning && !isLightOn) glVertexAttrib3f(1, onColor.r, onColor.g, onColor.b);
			else glVertexAttrib3f(1, offColor.r, offColor.g, offColor.b);
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

// 기차 그리기
void drawTrain(const Train& train, GLuint shader) {
	// 기차 몸체 (매우 긴 빨간 박스)
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(train.x, 0.5f, train.z));
	model = glm::scale(model, glm::vec3(15.0f, 1.8f, 0.9f)); // 길이 15, 높이 1.8

	glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
	if (shader == shaderProgramID) glVertexAttrib3f(1, 0.8f, 0.1f, 0.1f); // 빨간색
	glDrawArrays(GL_TRIANGLES, 0, 36);

	// 기차 창문 (장식)
	for (float i = -6.0f; i <= 6.0f; i += 2.0f) {
		glm::mat4 winModel = glm::translate(glm::mat4(1.0f), glm::vec3(train.x + i, 0.8f, train.z));
		winModel = glm::scale(winModel, glm::vec3(1.0f, 0.6f, 1.0f)); // 약간 튀어나오게
		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(winModel));
		if (shader == shaderProgramID) glVertexAttrib3f(1, 0.2f, 0.2f, 0.3f); // 파란 창문
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

// 통나무 그리기
void drawLog(const Log& logObj, GLuint shader) {
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(logObj.x, 0.45f, logObj.z)); // 물 위에 살짝 떠있음

	// 통나무는 가로로 김
	glm::vec3 scale = glm::vec3(logObj.width, 0.3f, 0.8f);

	glm::vec3 whiteColor = glm::vec3(1.0f, 1.0f, 1.0f);

	drawPart(shader, model, glm::vec3(0.0f), scale, whiteColor);
}

// 연잎 그리기
void drawLilyPad(const LilyPad& pad, GLuint shader) {
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(pad.x, 0.41f, pad.z)); // 물 표면 바로 위

	// 납작한 사각형
	glm::vec3 scale = glm::vec3(0.8f, 0.05f, 0.8f);

	// [수정] 텍스처가 초록색이므로, 틴트(Tint) 색상을 흰색으로 변경하여 텍스처 원본 색 유지
	glm::vec3 whiteColor = glm::vec3(1.0f, 1.0f, 1.0f);

	drawPart(shader, model, glm::vec3(0.0f), scale, whiteColor);
}

void renderObjects(GLuint shader, const glm::mat4& pvMatrix)
{
	// [핵심 수정 1] 텍스트 렌더링에서 해제된 VAO를 다시 연결 (검은 화면 해결)
	glBindVertexArray(vao);

	// 쉐이더 유니폼 위치 가져오기
	GLint useTexLoc = glGetUniformLocation(shader, "useTexture");
	GLint texLoc = glGetUniformLocation(shader, "targetTexture");
	GLint uvScaleLoc = glGetUniformLocation(shader, "uvScale"); // 타일링 스케일 조절 변수

	int currentZ = (int)std::round(playerPos.z);
	int drawRangeFront = 30;
	int drawRangeBack = 10;

	//// [수정 2] 구름 그리기 전에 텍스처 끄기 설정 추가
	//if (isFlying || isLanding) {
	//	if (shader == shaderProgramID) {
	//		glUniform1i(useTexLoc, 0);           // 텍스처 사용 OFF (구름은 단순 색상)
	//		glUniform2f(uvScaleLoc, 1.0f, 1.0f); // 스케일 초기화
	//	}
	//	drawClouds(shader);
	//}

	for (int z = currentZ - drawRangeFront; z <= currentZ + drawRangeBack; ++z) {
		generateLane(z);
		const SeasonColors& colors = seasonThemes[getSeasonByZ(z)];

		// 바닥(Lane) 모델 행렬 설정
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, -0.5f, (float)z));
		model = glm::scale(model, glm::vec3(31.0f, 1.0f, 1.0f)); // 가로로 31배 확대

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

		if (shader == shaderProgramID) {
			// 텍스처 매핑 분기 처리 (강 vs 잔디 vs 기타)

			// CASE 1: 강 (River)
			if (mapType[z] == 2) {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, riverTexture); // 강 텍스처 바인딩
				glUniform1i(texLoc, 1); // 샘플러에 1번 유닛 지정
				glUniform1i(useTexLoc, 1); // 텍스처 사용 ON

				glUniform2f(uvScaleLoc, 31.0f, 1.0f);

				glVertexAttrib3f(1, 1.0f, 1.0f, 1.0f);
			}
			// CASE 2: 잔디 (Grass)
			else if (mapType[z] == 0) {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, grassTexture); // 잔디 텍스처 바인딩
				glUniform1i(texLoc, 1);
				glUniform1i(useTexLoc, 1); // 텍스처 사용 ON

				glUniform2f(uvScaleLoc, 31.0f, 1.0f);

				glVertexAttrib3f(1, colors.grass.r, colors.grass.g, colors.grass.b);
			}
			// CASE 3: 도로 또는 철길 (Road / Rail)
			else {
				// 텍스처 끄기 & 스케일 초기화
				glUniform1i(useTexLoc, 0);
				glUniform2f(uvScaleLoc, 1.0f, 1.0f);

				// 기존 색상 로직
				if (mapType[z] == 1) { // 도로
					glVertexAttrib3f(1, 0.2f, 0.2f, 0.2f);
				}
				else if (mapType[z] == 3) { // 철길 바닥
					glVertexAttrib3f(1, 0.5f, 0.5f, 0.55f);
				}
			}
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);

		if (shader == shaderProgramID) {
			glUniform1i(useTexLoc, 0); // 텍스처 끄기
			glUniform2f(uvScaleLoc, 1.0f, 1.0f); // UV 스케일 원상복구
		}

		// 차선 그리기
		if (mapType[z] == 1) {
			glm::vec3 lineScale = glm::vec3(1.0f, 0.02f, 0.15f);

			for (float x = -15.0f; x <= 15.0f; x += 5.0f) {
				glm::mat4 lineModel = glm::mat4(1.0f);
				lineModel = glm::translate(lineModel, glm::vec3(x, 0.01f, (float)z));
				lineModel = glm::scale(lineModel, lineScale);

				glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(lineModel));

				if (shader == shaderProgramID) {
					glVertexAttrib3f(1, 0.3f, 0.3f, 0.3f);
				}
				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}

		// 철길 구조물 그리기
		if (mapType[z] == 3) {
			bool isWarning = false;
			bool isLightOn = false;
			for (const auto& t : trains) {
				if ((int)t.z == z) {
					isWarning = (t.state == TRAIN_WARNING || t.state == TRAIN_PASSING);
					isLightOn = t.isLightOn;
					break;
				}
			}
			drawRail(z, isWarning, isLightOn, shader);
		}

		// 나무 그리기
		if (treeMap.count(z)) {
			for (int treeX : treeMap[z]) drawTree(treeX, z, shader);
		}
	}

	// 2. 동적 오브젝트 그리기
	if (!isFlying && !isLanding) {
		// 자동차
		for (const auto& car : cars) {
			if (car.z < currentZ - drawRangeFront || car.z > currentZ + drawRangeBack) continue;
			drawCar(car, shader);
		}

		// 기차
		for (const auto& train : trains) {
			if (train.z < currentZ - drawRangeFront || train.z > currentZ + drawRangeBack) continue;
			if (train.state == TRAIN_PASSING) {
				drawTrain(train, shader);
			}
		}

		// 통나무
		for (const auto& logObj : logs) {
			if (logObj.z < currentZ - drawRangeFront || logObj.z > currentZ + drawRangeBack) continue;

			// 쉐이더가 메인 쉐이더일 때만 텍스처 적용
			if (shader == shaderProgramID) {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, logTexture); // 통나무 텍스처 바인딩
				glUniform1i(texLoc, 1); // 샘플러 1번 사용 설정
				glUniform1i(useTexLoc, 1); // 텍스처 사용 ON

				glUniform2f(uvScaleLoc, logObj.width, 1.0f);
			}

			drawLog(logObj, shader);

			if (shader == shaderProgramID) {
				glUniform1i(useTexLoc, 0);
				glUniform2f(uvScaleLoc, 1.0f, 1.0f); // 스케일 초기화
			}
		}

		// 연잎
		for (const auto& pad : lilyPads) {
			if (pad.z < currentZ - drawRangeFront || pad.z > currentZ + drawRangeBack) continue;

			// [추가] 쉐이더가 메인 쉐이더일 때만 텍스처 적용
			if (shader == shaderProgramID) {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, lilyPadTexture); // 연잎 텍스처 바인딩
				glUniform1i(texLoc, 1); // 샘플러 1번 사용 설정
				glUniform1i(useTexLoc, 1); // 텍스처 사용 ON

				glUniform2f(uvScaleLoc, 1.0f, 1.0f); // UV 스케일 1:1
			}

			drawLilyPad(pad, shader);

			// [추가] 텍스처 사용 해제 및 초기화
			if (shader == shaderProgramID) {
				glUniform1i(useTexLoc, 0);
				glUniform2f(uvScaleLoc, 1.0f, 1.0f);
			}
		}

		// 코인
		for (const auto& coin : coins) {
			if (coin.z < currentZ - drawRangeFront || coin.z > currentZ + drawRangeBack) continue;
			drawCoin(coin, shader);
		}
	}

	// 플레이어 (닭)
	glm::mat4 pModel = glm::translate(glm::mat4(1.0f), playerPos);
	pModel = glm::rotate(pModel, glm::radians(playerRotation), glm::vec3(0.0f, 1.0f, 0.0f));
	pModel = glm::scale(pModel, glm::vec3(0.7f));
	if (isFlying) {
		glm::mat4 birdModel = pModel;
		birdModel = glm::translate(birdModel, glm::vec3(0.0f, 0.5f, 0.0f));
		birdModel = glm::scale(birdModel, glm::vec3(1.5f));
		drawBird(shader, birdModel);
	}
	else if (isBirdLeaving) {
		glm::mat4 birdModel = glm::mat4(1.0f);
		float t = glm::clamp(birdLeaveTime / BIRD_LEAVE_DURATION, 0.0f, 1.0f);
		glm::vec3 currentBirdPos = glm::mix(birdStartPos, birdTargetPos, t);
		birdModel = glm::translate(birdModel, currentBirdPos);
		birdModel = glm::scale(birdModel, glm::vec3(1.5f));
		drawBird(shader, birdModel);
	}
	else {
		drawChicken(shader, pModel);
	}

	glDrawArrays(GL_TRIANGLES, 0, 36);
}

// 파티클 렌더링 함수
void drawParticles(GLuint shader) {
	for (const auto& p : particles) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, p.position);
		model = glm::scale(model, glm::vec3(p.scale)); // 파티클은 정육면체

		// 쉐이더에 모델 행렬 전달
		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

		if (shader == shaderProgramID) {
			glVertexAttrib3f(1, p.color.r, p.color.g, p.color.b);
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

// 테두리 있는 텍스트 그리기 함수
void renderTextWithOutline(float x, float y, const char* text) {
	float offset = 5.0f; // 테두리 두께

	// 1. 검은색 테두리 그리기 (상하좌우 + 대각선 4방향 = 총 8방향)
	renderTextTTF(x - offset, y, text, 0.0f, 0.0f, 0.0f); // 좌
	renderTextTTF(x + offset, y, text, 0.0f, 0.0f, 0.0f); // 우
	renderTextTTF(x, y - offset, text, 0.0f, 0.0f, 0.0f); // 상
	renderTextTTF(x, y + offset, text, 0.0f, 0.0f, 0.0f); // 하

	renderTextTTF(x - offset, y - offset, text, 0.0f, 0.0f, 0.0f);
	renderTextTTF(x + offset, y - offset, text, 0.0f, 0.0f, 0.0f);
	renderTextTTF(x - offset, y + offset, text, 0.0f, 0.0f, 0.0f);
	renderTextTTF(x + offset, y + offset, text, 0.0f, 0.0f, 0.0f);

	// 2. 흰색 알맹이 그리기 (가장 위에 덮어쓰기)
	renderTextTTF(x, y, text, 1.0f, 1.0f, 1.0f);
}

// 텍스트 그리기 함수
void renderTextTTF(float x, float y, const char* text, float r, float g, float b) {
	glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDisable(GL_LIGHTING);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, 1280, 0, 960);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);

	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D); // 유닛 1 끄기

	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D); // 유닛 0 켜기 (폰트 텍스처용)

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// 이미 위에서 활성화했으므로 glActiveTexture(GL_TEXTURE0)는 생략 가능하지만 명시적으로 둡니다.
	glBindTexture(GL_TEXTURE_2D, fontTexture);

	glBegin(GL_QUADS);
	glColor3f(r, g, b);

	while (*text) {
		if (*text >= 32 && *text < 128) {
			stbtt_aligned_quad q;
			stbtt_GetBakedQuad(cdata, 512, 512, *text - 32, &x, &y, &q, 1);

			glTexCoord2f(q.s0, q.t0); glVertex2f(q.x0, 960 - q.y0);
			glTexCoord2f(q.s1, q.t0); glVertex2f(q.x1, 960 - q.y0);
			glTexCoord2f(q.s1, q.t1); glVertex2f(q.x1, 960 - q.y1);
			glTexCoord2f(q.s0, q.t1); glVertex2f(q.x0, 960 - q.y1);
		}
		++text;
	}
	glEnd();

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);

	glEnable(GL_LIGHTING);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}
// 구름 큐브 하나를 그리는 헬퍼 함수
void drawCloud(const Cloud& cloud, GLuint shader) {
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 0; dy++) {
			for (int dz = -1; dz <= 1; dz++) {
				// 구름 중심에서 약간 랜덤하게 퍼지도록
				if (rand() % 100 > 60) continue;

				glm::mat4 model = glm::mat4(1.0f);
				glm::vec3 offset = glm::vec3(
					dx * cloud.scale * 0.4f,
					dy * cloud.scale * 0.3f,
					dz * cloud.scale * 0.4f
				);
				model = glm::translate(model, cloud.position);
				model = glm::scale(model, glm::vec3(cloud.scale * 1.5f, cloud.scale * 0.8f, cloud.scale * 1.5f)); // 약간 납작하게

				glm::vec3 cloudColor = glm::vec3(0.95f, 0.95f, 0.95f); // 밝은 회색/흰색

				glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
				if (shader == shaderProgramID) {
					glVertexAttrib3f(1, cloudColor.r, cloudColor.g, cloudColor.b);
				}
				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}
	}
}

void drawClouds(GLuint shader) {
	for (const auto& cloud : clouds) {
		drawCloud(cloud, shader);
	}
}

GLvoid drawScene()
{
	// 1. 카메라 흔들림 오프셋 계산
	glm::vec3 shakeOffset(0.0f, 0.0f, 0.0f);

	// 흔들림 타이머가 작동 중이면 랜덤 좌표 생성
	if (shakeTimer > 0.0f) {
		float rx = ((rand() % 100) / 50.0f - 1.0f) * shakeMagnitude; // -mag ~ +mag
		float ry = ((rand() % 100) / 50.0f - 1.0f) * shakeMagnitude;
		float rz = ((rand() % 100) / 50.0f - 1.0f) * shakeMagnitude;
		shakeOffset = glm::vec3(rx, ry, rz);
	}

	// 2. 뷰 행렬 생성 
	glm::vec3 cameraTarget = playerPos;
	cameraTarget.y = 0.5f;

	glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.f / 960.f, 0.1f, 100.f);

	glm::mat4 view;
	if (isFlying || isLanding || isBirdLeaving) {
		// 비행 시점 (흔들림 추가)
		view = glm::lookAt(cameraTarget + glm::vec3(2, 16, 10) + shakeOffset,
			cameraTarget + shakeOffset,
			glm::vec3(0, 1, 0));
	}
	else {
		// 평소 시점 (흔들림 추가)
		view = glm::lookAt(cameraTarget + glm::vec3(2, 12, 10) + shakeOffset,
			cameraTarget + shakeOffset,
			glm::vec3(0, 1, 0));
	}

	glm::mat4 lightProjection = glm::ortho(-50.f, 50.f, -50.f, 50.f, 1.f, 100.f);
	glm::mat4 lightView = glm::lookAt(lightPos, cameraTarget, glm::vec3(0, 1, 0)); // playerPos -> cameraTarget
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	// [패스 1] 그림자 맵 생성 (Depth Map Generation)
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUseProgram(depthShader);

	glUniformMatrix4fv(glGetUniformLocation(depthShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

	renderObjects(depthShader, lightSpaceMatrix);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// [패스 2] 실제 장면 렌더링 (Main Render Pass)
	glViewport(0, 0, 1280, 960);
	if (isFlying || isLanding || isBirdLeaving) {
		// 하늘색 
		glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
	}
	else {
		// 평소 배경색 (검은색)
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);

	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
	glUniform3fv(glGetUniformLocation(shaderProgramID, "lightPos"), 1, &lightPos[0]);

	// 핀 조명 유니폼 값 전달
	int spotlightVal = isNightMode ? 1 : 0;
	glUniform1i(glGetUniformLocation(shaderProgramID, "enableSpotlight"), spotlightVal);
	glUniform3fv(glGetUniformLocation(shaderProgramID, "spotlightPos"), 1, glm::value_ptr(playerPos));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glUniform1i(glGetUniformLocation(shaderProgramID, "shadowMap"), 0);

	// 맵 오브젝트 렌더링 (비행 중에는 바닥 그리기 생략 가능하지만, 여기선 그림자 때문에 그림)
	renderObjects(shaderProgramID, proj * view);

	int currentZ = (int)std::round(playerPos.z);
	int drawRangeFront = 30;
	int drawRangeBack = 10;

	for (auto& item : items) {
		if (item.z < currentZ - drawRangeFront || item.z > currentZ + drawRangeBack) continue;
		// 회전 애니메이션
		item.rotation += 2.0f;
		drawItem(item, shaderProgramID);
	}

	// 파티클 그리기 호출
	drawParticles(shaderProgramID);

	if (isFlying || isLanding) {
		drawClouds(shaderProgramID);
	}

	// 날씨 파티클 렌더링 (봄/가을/겨울 통합)
	glUseProgram(shaderProgramID);
	for (const auto& wp : weatherParticles) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, wp.pos);

		// 계절마다 설정된 크기(모양) 적용
		model = glm::scale(model, wp.scaleVec);
		model = glm::rotate(model, wp.swayPhase, glm::vec3(0.5f, 1.0f, 0.2f));

		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(model));

		// 파티클마다 고유의 색상 적용
		glVertexAttrib3f(1, wp.color.r, wp.color.g, wp.color.b);

		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// [UI 렌더링] 
	// 1. 점수 표시
	std::string scoreStr = "SCORE: " + std::to_string(score);
	renderTextWithOutline(20, 60, scoreStr.c_str());

	// 2. 코인 개수 표시 
	std::string coinStr = "COINS: " + std::to_string(coinCount);
	renderTextWithOutline(1050, 60, coinStr.c_str());

	// 3. 아이템 상태 UI 표시 
	float uiY = 880.0f;
	uiY = 120.0f;

	if (hasShield) {
		renderTextWithOutline(950, uiY, "[SHIELD ON]");
		uiY += 40.0f;
	}
	if (isMagnetActive) {
		std::string msg = "[MAGNET] " + std::to_string((int)magnetTimer) + "s";
		renderTextWithOutline(900, uiY, msg.c_str());
		uiY += 40.0f;
	}
	if (isSlowActive) {
		std::string msg = "[SLOW] " + std::to_string((int)slowTimer) + "s";
		renderTextWithOutline(1000, uiY, msg.c_str());
	}


	// 4. 경고 메시지 (암전)
	if (isNightWarning) {
		// 0.2초 간격으로 깜빡거리게 만들기
		if ((int)(nightWarningTimer * 5) % 2 == 0) {
			float centerX = 1280.0f / 2.0f;
			float centerY = 960.0f / 2.0f;

			// 그림자 + 본문
			renderTextTTF(centerX - 150.0f + 3.0f, centerY + 3.0f, "WARNING: BLACKOUT!", 0.0f, 0.0f, 0.0f);
			renderTextTTF(centerX - 150.0f, centerY, "WARNING: BLACKOUT!", 1.0f, 0.0f, 0.0f);
		}
	}

	/* 5. 대쉬 상태 및 쿨타임 표시(화면 중앙 하단)
	std::string dashStr;
	float dashR = 1.0f, dashG = 1.0f, dashB = 0.0f; // 기본 색상 (노랑)

	if (isDashing) {
		dashStr = "DASH! (" + std::to_string((int)(dashTimer * 10) / 10.0f) + "s)";
		dashR = 1.0f; dashG = 0.0f; dashB = 0.0f; // 빨강
	}
	else if (dashCooldownTimer > 0.0f) {
		dashStr = "CD: " + std::to_string((int)(dashCooldownTimer * 10) / 10.0f) + "s";
		dashR = 0.5f; dashG = 0.5f; dashB = 0.5f; // 회색
	}
	else if (coinCount >= DASH_COST) {
		dashStr = "DASH READY";
		dashR = 0.0f; dashG = 1.0f; dashB = 0.0f; // 초록
	}
	else {
		dashStr = "NEED " + std::to_string(DASH_COST - coinCount) + " COINS";
		dashR = 1.0f; dashG = 0.5f; dashB = 0.0f; // 주황
	}
	// 대쉬 UI 위치
	renderTextTTF(1280 / 2 - 100, 100, dashStr.c_str(), dashR, dashG, dashB);
	*/


	// 6. 스페이스바 연타 이벤트 UI
	if (isEventActive) {
		float barWidth = 600.0f;
		float barHeight = 40.0f;
		float centerX = 1280.0f / 2.0f;
		float centerY = 960.0f - 150.0f; // 화면 위쪽 중앙

		// 1. 배경 (테두리)
		glUseProgram(0);
		glDisable(GL_DEPTH_TEST);
		glBegin(GL_QUADS);
		glColor3f(0.1f, 0.1f, 0.1f); // 검은색 배경
		glVertex2f(centerX - barWidth / 2.0f - 5.0f, centerY - barHeight / 2.0f - 5.0f);
		glVertex2f(centerX + barWidth / 2.0f + 5.0f, centerY - barHeight / 2.0f - 5.0f);
		glVertex2f(centerX + barWidth / 2.0f + 5.0f, centerY + barHeight / 2.0f + 5.0f);
		glVertex2f(centerX - barWidth / 2.0f - 5.0f, centerY + barHeight / 2.0f + 5.0f);
		glEnd();

		// 2. 게이지 채우기
		float fillWidth = barWidth * glm::clamp(eventProgress, 0.0f, 1.0f);
		glBegin(GL_QUADS);
		glColor3f(0.0f, 1.0f, 0.0f); // 초록색 게이지
		glVertex2f(centerX - barWidth / 2.0f, centerY - barHeight / 2.0f);
		glVertex2f(centerX - barWidth / 2.0f + fillWidth, centerY - barHeight / 2.0f);
		glVertex2f(centerX - barWidth / 2.0f + fillWidth, centerY + barHeight / 2.0f);
		glVertex2f(centerX - barWidth / 2.0f, centerY + barHeight / 2.0f);
		glEnd();

		// 3. 텍스트 표시
		renderTextTTF(centerX - 100.0f, 960.0f - centerY - 50.0f, "PRESS SPACE!", 1.0f, 1.0f, 1.0f);
		glEnable(GL_DEPTH_TEST);
		glUseProgram(shaderProgramID);
	}

	// [추가] 7. 바람 경고 UI
	if (isWindActive) {
		std::string windStr;
		if (windForce > 0) windStr = ">>> STRONG WIND >>>";
		else windStr = "<<< STRONG WIND <<<";

		// 하늘색 텍스트로 중앙 상단에 표시
		renderTextWithOutline(1280 / 2 - 150, 800, windStr.c_str());
	}

	glutSwapBuffers();
}

// 파티클 생성 함수
// pos: 발생 위치, color: 색상, count: 개수, speedScale: 퍼지는 속도 계수
void spawnParticles(glm::vec3 pos, glm::vec3 color, int count, float speedScale) {
	for (int i = 0; i < count; i++) {
		Particle p;
		p.position = pos;

		float rX = ((rand() % 100) / 100.0f - 0.5f) * speedScale;
		float rY = ((rand() % 100) / 100.0f) * (speedScale * 0.5f);
		float rZ = ((rand() % 100) / 100.0f - 0.5f) * speedScale;

		p.velocity = glm::vec3(rX, rY, rZ);
		p.color = color;
		p.scale = (rand() % 5) / 20.0f + 0.1f; // 0.1 ~ 0.35 크기 랜덤
		p.life = 1.0f; // 수명 초기화

		particles.push_back(p);
	}
}

void timer(int value)
{
	// 카메라 흔들림 시간 감소 로직
	if (shakeTimer > 0.0f) {
		shakeTimer -= 0.016f;
		if (shakeTimer <= 0.0f) {
			shakeTimer = 0.0f;
			shakeMagnitude = 0.0f; // 시간이 다 되면 흔들림 멈춤
		}
	}

	// 핀 조명 로직
	// 1. 경고 단계
	if (isNightWarning) {
		nightWarningTimer -= 0.016f;

		// 경고 시간이 끝나면? -> 진짜 밤 모드 시작!
		if (nightWarningTimer <= 0.0f) {
			isNightWarning = false;
			isNightMode = true; // 암전 시작
			nightModeTimer = NIGHT_DURATION;
			printf("경고 종료! 암전 시작!\n");
		}
	}
	// 2. 밤(핀 조명) 단계
	else if (isNightMode) {
		nightModeTimer -= 0.016f;
		if (nightModeTimer <= 0.0f) {
			isNightMode = false;
			nightEventCooldown = (rand() % 10) + 10.0f; // 쿨타임 시작
			printf("핀 조명 이벤트 종료! 다시 밝아집니다.\n");
		}
	}
	// 3. 평상시 (대기 단계)
	else {
		if (nightEventCooldown > 0.0f) {
			nightEventCooldown -= 0.016f;
		}
		else {
			// 확률 체크
			if (rand() % 1500 < 1) {
				isNightWarning = true;
				nightWarningTimer = WARNING_DURATION;
				printf("!!! 정전 경고 발령 !!!\n");
			}
		}
	}

	float playerX = playerPos.x;
	float playerZ = playerPos.z;
	float playerSize = 0.5f;
	bool scoreTargetReached = (score > 0) && (score % 50 == 0);

	// 주인공이 서 있을 높이
	float restingY = 0.5f;

	// 파티클 물리 업데이트
	for (auto it = particles.begin(); it != particles.end(); ) {
		it->position += it->velocity;
		it->life -= 0.05f;
		it->scale -= 0.005f;
		if (it->life <= 0.0f || it->scale <= 0.0f) it = particles.erase(it);
		else ++it;
	}

	// 대쉬 쿨타임 감소
	if (dashCooldownTimer > 0.0f) {
		dashCooldownTimer -= 0.016f;
		if (dashCooldownTimer < 0.0f) dashCooldownTimer = 0.0f;
	}

	// 5. [아이템] 슬로우 모션 속도 배율 설정
	float globalSpeedRate = 1.0f;
	if (isSlowActive) {
		globalSpeedRate = 0.5f; // 모든 물체 속도 절반
		slowTimer -= 0.016f;
		if (slowTimer <= 0.0f) {
			isSlowActive = false;
			printf("슬로우 효과 종료.\n");
		}
	}

	// 6. [아이템] 자석 타이머 및 효과
	if (isMagnetActive) {
		magnetTimer -= 0.016f;
		if (magnetTimer <= 0.0f) {
			isMagnetActive = false;
			printf("자석 효과 종료.\n");
		}
		// 코인 끌어당기기 로직
		for (auto& coin : coins) {
			if (coin.isCollected) continue;
			float dist = glm::distance(playerPos, glm::vec3(coin.x, playerPos.y, coin.z));
			if (dist < 6.0f) { // 사거리 6칸
				glm::vec3 dir = glm::normalize(playerPos - glm::vec3(coin.x, playerPos.y, coin.z));
				coin.x += dir.x * 0.2f;
				coin.z += dir.z * 0.2f;
			}
		}
	}

	// 7. [아이템] 획득 로직
	for (auto& item : items) {
		if (item.isCollected) continue;
		if (std::abs(item.z - playerPos.z) < 0.5f && std::abs(item.x - playerPos.x) < 0.5f) {
			item.isCollected = true;

			if (item.type == ITEM_SHIELD) {
				hasShield = true;
				printf("아이템 획득: 보호막!\n");
				spawnParticles(playerPos, glm::vec3(0.0f, 0.5f, 1.0f), 20, 1.0f);
			}
			else if (item.type == ITEM_MAGNET) {
				isMagnetActive = true;
				magnetTimer = 10.0f;
				printf("아이템 획득: 자석!\n");
				spawnParticles(playerPos, glm::vec3(1.0f, 0.0f, 0.0f), 20, 1.0f);
			}
			else if (item.type == ITEM_CLOCK) {
				isSlowActive = true;
				slowTimer = 5.0f;
				printf("아이템 획득: 슬로우!\n");
				spawnParticles(playerPos, glm::vec3(0.0f, 1.0f, 1.0f), 20, 1.0f);
			}
		}
	}

	// 바람 이벤트 로직
	if (isWindActive) {
		windTimer -= 0.016f;

		// 플레이어 밀림 로직
		if (!isMoving && !isFlying && !isLanding) {

			float nextX = playerPos.x + windForce;
			int currentZ = (int)std::round(playerPos.z);
			float bodyOffset = (windForce > 0) ? 0.4f : -0.4f;

			int checkGridX = (int)std::round(nextX + bodyOffset);

			bool isOnRiver = (mapType.count(currentZ) && mapType[currentZ] == 2);

			bool isTreeBlocking = isTreeAt(checkGridX, currentZ);

			if (!isOnRiver && !isTreeBlocking) {
				playerPos.x += windForce;
				playerStartPos.x += windForce;
				playerTargetPos.x += windForce;
			}
		}

		// 파티클 효과 
		if (rand() % 10 < 3) {
			float spawnX = (windForce > 0) ? -20.0f : 20.0f;
			float spawnZ = playerPos.z + (rand() % 40 - 20);
			float spawnY = (rand() % 10) + 2.0f;

			glm::vec3 pPos(spawnX, spawnY, spawnZ);
			glm::vec3 pColor(0.9f, 0.95f, 1.0f);

			Particle p;
			p.position = pPos;
			p.velocity = glm::vec3(windForce * 50.0f, -0.1f, 0.0f);
			p.color = pColor;
			p.scale = 0.1f;
			p.life = 0.8f;
			particles.push_back(p);
		}

		// 바람 종료 체크
		if (windTimer <= 0.0f) {
			isWindActive = false;
			printf("바람이 멈췄습니다.\n");
		}
	}
	else {
		// 바람이 불지 않을 때 -> 랜덤 확률 발생 
		if (!isFlying && !isEventActive && !isNightMode && rand() % 800 < 1) {
			isWindActive = true;
			windTimer = WIND_DURATION;
			windForce = (rand() % 2 == 0 ? 0.04f : -0.04f);
			printf("!!! 강풍 경보 !!! 방향: %s\n", (windForce > 0 ? "오른쪽 >>>" : "<<< 왼쪽"));
		}
	}

	if (isDashing) {
		const float DASH_SPEED = 0.5f;
		playerPos.z -= DASH_SPEED;
		dashTimer -= 0.016f;
		int currentZ = (int)std::round(playerPos.z);
		if (currentZ < minZ) {
			minZ = currentZ;
			score++;
			printf("Score: %d (DASH)\n", score);
		}
		if (dashTimer <= 0.0f) {
			isDashing = false;
			dashCooldownTimer = DASH_COOLDOWN;
			playerPos.x = (float)std::round(playerPos.x);
			playerPos.z = (float)std::round(playerPos.z);
			// 다음 움직임을 위해 playerTargetPos도 현재 위치로 업데이트
			playerTargetPos = playerPos;
			printf("대쉬 종료. 쿨다운 시작: %.1f초\n", DASH_COOLDOWN);

			//if (scoreTargetReached && score > lastEventScore && !isEventActive && !isFlying) {
			//	isEventActive = true;
			//	eventProgress = 0.0f;
			//	requiredTaps = 0;
			//	lastEventScore = score;
			//	printf("!!! 대쉬 종료 직후 스페이스 연타 이벤트 발생! 4초 안에 성공하세요!\n");
			//}
		}

	}

	//else {
	//	if (!isMoving && dashCooldownTimer <= 0.0f && coinCount >= DASH_COST) {
	//		// 코인 사용
	//		coinCount -= DASH_COST;

	//		// 대쉬 상태 시작
	//		isDashing = true;
	//		dashTimer = DASH_DURATION;

	//		// 시각적 피드백: 무적 대쉬 발동 파티클
	//		spawnParticles(playerPos, glm::vec3(1.0f, 0.5f, 0.0f), 20, 0.8f);
	//		printf("코인 7개 달성! 무적 대쉬 자동 발동! 남은 시간: %.1f초\n", DASH_DURATION);
	//	}
	//}
	else if (isFlying) {
		playerPos.z -= FLY_SPEED; // Z축 마이너스 방향 (앞으로) 이동
		playerPos.y = 3.0f; // 공중 높이 고정 (3.0f)

		flyTimer -= 0.016f;

		int currentZ = (int)std::round(playerPos.z);
		if (currentZ < minZ) {
			minZ = currentZ;
			score++;
			// 점수 획득 시 파티클 효과 추가 가능
		}

		// 비행 종료
		if (flyTimer <= 0.0f) {
			isFlying = false;
			// 땅으로 착지, 현재 칸의 정중앙에 고정
			isLanding = true;
			landingTime = 0.0f;
			/*playerPos.x = (float)std::round(playerPos.x);
			playerPos.z = (float)std::round(playerPos.z);*/

			birdStartPos = playerPos + glm::vec3(0.0f, 0.5f * 0.7f + 0.5f * 0.7f, 0.0f); // 닭 위에 새의 대략적 위치
			birdTargetPos = playerPos + glm::vec3(30.0f, FLY_HEIGHT + 10.0f, -50.0f); // 멀리 날아갈 목표 위치

			playerStartPos = playerPos;
			playerTargetPos = glm::vec3((float)std::round(playerPos.x), 0.5f, (float)std::round(playerPos.z));
			printf("로켓 라이드 종료! 착륙을 시작합니다.\n");
		}
	}
	else if (isLanding) {
		landingTime += 0.016f;
		float t = glm::clamp(landingTime / LANDING_DURATION, 0.0f, 1.0f);

		// X, Z는 목표 지점(TargetPos)으로 보간
		playerPos.x = glm::mix(playerStartPos.x, playerTargetPos.x, t);
		playerPos.z = glm::mix(playerStartPos.z, playerTargetPos.z, t);

		// Y는 하강 애니메이션
		playerPos.y = glm::mix(playerStartPos.y, playerTargetPos.y, t);

		// 착륙 완료
		if (t >= 1.0f) {
			isLanding = false;
			playerPos = playerTargetPos; // 최종 위치 확정 (원점 아님)
			isBirdLeaving = true;
			birdLeaveTime = 0.0f;
			printf("착륙 완료!\n");
		}
	}
	else if (isEventActive) {
		eventDuration -= 0.016f;

		//// 목표 게이지: 1초에 TAP_PER_SECOND 횟수를 누르면 게이지가 1.0이 됩니다.
		//float targetProgress = requiredTaps / (TAP_PER_SECOND * (4.0f - eventDuration));
		//eventProgress = glm::clamp(eventProgress, 0.0f, targetProgress);

		// 성공 조건:
		if (eventProgress >= 1.0f) {
			isEventActive = false;
			isFlying = true;
			flyTimer = FLY_DURATION;
			playerPos.y = FLY_HEIGHT;
			printf("로켓 라이드 성공! 새를 타고 비행합니다!\n");
			// 새와 함께 날아갈 때 효과 파티클
			spawnParticles(playerPos + glm::vec3(0, 1.0f, 0), glm::vec3(1.0f, 0.5f, 0.0f), 50, 1.5f);
		}
		// 실패 조건
		else if (eventDuration <= 0.0f) {
			isEventActive = false;
			eventProgress = 0.0f;
			printf("로켓 라이드 실패! 다음 기회를 노리세요.\n");
		}

		// 이벤트 중에는 일반 이동 로직을 건너뜁니다.
	}
	// [상태 E] 일반 게임 플레이 (이동, 충돌, 아이템)
	else {
		// 1. 대쉬 자동 발동 조건 체크
		if (!isMoving && dashCooldownTimer <= 0.0f && coinCount >= DASH_COST) {
			coinCount -= DASH_COST;
			isDashing = true;
			dashTimer = DASH_DURATION;
			spawnParticles(playerPos, glm::vec3(1.0f, 0.5f, 0.0f), 20, 0.8f);
			printf("코인 7개 달성! 무적 대쉬 자동 발동! 남은 시간: %.1f초\n", DASH_DURATION);
		}

		// 2. 랜덤 이벤트 발생 조건 체크
		if (scoreTargetReached && score > lastEventScore && !isMoving) {
			isEventActive = true;
			eventProgress = 0.0f;
			requiredTaps = 0;
			lastEventScore = score;
			printf("!!! 스페이스 연타 이벤트 발생! 4초 안에 성공하세요!\n");
		}

		// 3. 새 퇴장 애니메이션
		if (isBirdLeaving) {
			birdLeaveTime += 0.016f;
			float t = glm::clamp(birdLeaveTime / BIRD_LEAVE_DURATION, 0.0f, 1.0f);
			if (t >= 1.0f) {
				isBirdLeaving = false;
			}
		}

		// 통나무 이동 및 탑승
		for (auto& logObj : logs) {
			// [수정] 슬로우 효과 적용 (globalSpeedRate)
			logObj.x += logObj.speed * globalSpeedRate;
			if (logObj.speed > 0 && logObj.x > 20.0f) logObj.x = -20.0f;
			else if (logObj.speed < 0 && logObj.x < -20.0f) logObj.x = 20.0f;

			bool onLog = (std::abs(playerPos.z - logObj.z) < 0.1f) &&
				(playerPos.x >= logObj.x - logObj.width / 2.0f - 0.3f) &&
				(playerPos.x <= logObj.x + logObj.width / 2.0f + 0.3f);

			if (!isMoving && onLog) {
				// 탑승 중일 때도 슬로우 적용된 속도로 이동
				playerPos.x += logObj.speed * globalSpeedRate;
				playerTargetPos.x += logObj.speed * globalSpeedRate;
				playerStartPos.x += logObj.speed * globalSpeedRate;
				restingY = 1.1f;
			}
		}

		// [충돌 검사 시작]
		bool isDead = false;

		// 자동차 충돌
		for (auto& car : cars) {
			// [수정] 슬로우 효과 적용
			car.x += car.speed * globalSpeedRate;
			if (car.x > 18.0f && car.speed > 0) car.x = -18.0f;
			if (car.x < -18.0f && car.speed < 0) car.x = 18.0f;

			// 충돌 체크
			bool hit = (abs(car.z - playerPos.z) < 0.2f && abs(car.x - playerPos.x) < 0.8f) ||
				(abs(car.z - playerPos.z) < 0.08f && abs(car.x - playerPos.x) < 1.2f);

			if (hit) {
				if (hasShield) {
					hasShield = false; // 보호막 소모
					spawnParticles(playerPos, glm::vec3(0.5f, 0.5f, 1.0f), 30, 2.0f); // 파란 파티클
					printf("보호막이 충돌을 막았습니다!\n");
					// 차를 멀리 치워서 연속 충돌 방지
					car.x = (car.speed > 0) ? 20.0f : -20.0f;
				}
				else {
					isDead = true;
				}
			}
		}

		// 익사(물) 판정
		int pZ = (int)std::round(playerPos.z);
		if (!isMoving && mapType.count(pZ) && mapType[pZ] == 2) {
			bool safe = false;
			// 통나무 체크
			for (const auto& logObj : logs) {
				if (logObj.z == (float)pZ &&
					playerPos.x >= logObj.x - logObj.width / 2.0f - 0.4f &&
					playerPos.x <= logObj.x + logObj.width / 2.0f + 0.4f) {
					safe = true;
					restingY = 1.1f;
					break;
				}
			}
			// 연잎 체크
			if (!safe) {
				for (const auto& pad : lilyPads) {
					if (pad.z == (float)pZ && std::abs(playerPos.x - pad.x) < 0.6f) {
						safe = true;
						restingY = 0.93f;
						break;
					}
				}
			}
			if (!safe) isDead = true;
			if (playerPos.x < -16.0f || playerPos.x > 16.0f) isDead = true;
		}

		// 기차 충돌 검사
		for (auto& t : trains) {
			switch (t.state) {
			case TRAIN_IDLE:
				t.timer--;
				if (t.timer <= 0) {
					t.state = TRAIN_WARNING;
					t.timer = 120;
				}
				break;
			case TRAIN_WARNING:
				t.timer--;
				if ((t.timer / 10) % 2 == 0) t.isLightOn = true;
				else t.isLightOn = false;

				if (t.timer <= 0) {
					t.state = TRAIN_PASSING;
					t.x = -60.0f;
					t.speed = 3.0f;
				}
				break;
			case TRAIN_PASSING:
				t.x += t.speed;

				if (t.x > -20.0f && t.x < 25.0f &&
					!isFlying && !isLanding &&
					std::abs(t.z - playerPos.z) < 10.0f) {

					shakeTimer = 0.1f;
					shakeMagnitude = 0.2f;
				}

				// 충돌 체크
				if (t.state == TRAIN_PASSING)
				{
					int playerGridZ = (int)std::round(playerPos.z);
					int trainGridZ = (int)std::round(t.z);

					if (playerGridZ == trainGridZ) {
						if (playerPos.x > t.x - 9.0f && playerPos.x < t.x + 9.0f) {
							isDead = true;
							printf("기차에 치임!\n");
						}
					}
				}

				if (t.x > 60.0f) {
					t.state = TRAIN_IDLE;
					t.timer = rand() % 300 + 200;
				}
				break;
			}
		}

		if (isDead) {
			playerPos = glm::vec3(0.0f, 0.5f, 0.0f);
			playerTargetPos = playerPos;
			playerStartPos = playerPos;
			isMoving = false;
			isDashing = false; // 대쉬 상태 초기화
			dashTimer = 0.0f;
			dashCooldownTimer = 0.0f;

			// 바람 이벤트 초기화
			isWindActive = false;
			windTimer = 0.0f;
			windForce = 0.0f;

			// 핀 조명(암전) 이벤트 초기화
			isNightMode = false;
			isNightWarning = false;
			nightModeTimer = 0.0f;
			nightWarningTimer = 0.0f;
			nightEventCooldown = 10.0f;

			// 로켓/연타 이벤트 초기화
			isEventActive = false;
			eventProgress = 0.0f;
			requiredTaps = 0;
			lastEventScore = 0;

			// 4. 아이템
			items.clear();
			hasShield = false;
			isMagnetActive = false;
			isSlowActive = false;
			magnetTimer = 0.0f;
			slowTimer = 0.0f;

			// 비행 상태 초기화 (혹시 날다가 죽었을 경우 대비)
			isFlying = false;
			isLanding = false;
			isBirdLeaving = false;

			score = 0;
			minZ = 0;
			coinCount = 0;

			cars.clear();
			logs.clear();
			lilyPads.clear();
			treeMap.clear();
			mapType.clear();
			trains.clear();
			particles.clear();
			weatherParticles.clear();

			for (int z = -10; z < 10; ++z) generateLane(z);

			glutPostRedisplay();
			glutTimerFunc(16, timer, 0);
			return;

		}

		// 코인 로직
		for (auto& coin : coins) {
			if (coin.isCollected) continue;
			if (std::abs(coin.z - playerZ) < playerSize && std::abs(coin.x - playerX) < playerSize) {
				coin.isCollected = true;
				coinCount++;
				printf("Coin collected! Total: %d\n", coinCount);
				spawnParticles(glm::vec3(coin.x, 0.5f, coin.z), glm::vec3(1.0f, 0.9f, 0.0f), 10, 0.3f);
			}
		}

		// 점프 애니메이션
		if (isMoving) {
			moveTime += 0.016f;
			float t = glm::clamp(moveTime / MOVE_DURATION, 0.0f, 1.0f);

			playerPos.x = glm::mix(playerStartPos.x, playerTargetPos.x, t);
			playerPos.z = glm::mix(playerStartPos.z, playerTargetPos.z, t);

			float jumpY = JUMP_HEIGHT * 4.0f * t * (1.0f - t);
			playerPos.y = 0.5f + jumpY;

			if (t >= 1.0f) {
				playerPos = playerTargetPos;
				isMoving = false;

				int landZ = (int)std::round(playerPos.z);
				if (mapType.count(landZ) && mapType[landZ] == 2) {
					glm::vec3 particleColor = glm::vec3(0.0f, 0.5f, 1.0f);
					spawnParticles(playerPos, particleColor, 8, 0.2f);
				}
			}
		}

		// 점프 애니메이션 (대쉬 중에는 일반 점프를 막아야 함. isDashing은 isMoving과 동시에 실행되지 않도록 보장)
		if (isMoving && !isDashing) {
			moveTime += 0.016f;
			float t = glm::clamp(moveTime / MOVE_DURATION, 0.0f, 1.0f);

			playerPos.x = glm::mix(playerStartPos.x, playerTargetPos.x, t);
			playerPos.z = glm::mix(playerStartPos.z, playerTargetPos.z, t);

			float jumpY = JUMP_HEIGHT * 4.0f * t * (1.0f - t);
			playerPos.y = 0.5f + jumpY;

			if (t >= 1.0f) {
				playerPos = playerTargetPos;
				isMoving = false;

				int landZ = (int)std::round(playerPos.z);
				if (mapType.count(landZ) && mapType[landZ] == 2) {
					glm::vec3 particleColor = glm::vec3(0.0f, 0.5f, 1.0f);
					spawnParticles(playerPos, particleColor, 8, 0.2f);
				}
			}
		}
		// 점프 중이 아닐 때 높이 적용
		if (!isMoving && !isDashing) {
			playerPos.y = restingY;
			playerStartPos.y = restingY;
		}
	}


	// 계절별 날씨 파티클 생성 로직
	// 1. 현재 플레이어 위치의 계절 확인
	int currentPZ = (int)std::round(playerPos.z);
	GameSeason nowSeason = getSeasonByZ(currentPZ);

	if (nowSeason != SUMMER) {
		for (int i = 0; i < 3; ++i) {
			WeatherParticle wp;

			// 위치: 플레이어 주변 랜덤 
			float rangeX = ((rand() % 500) / 10.0f) - 25.0f;
			float rangeZ = ((rand() % 500) / 10.0f) - 25.0f;
			wp.pos = glm::vec3(playerPos.x + rangeX, 15.0f, playerPos.z + rangeZ);

			wp.swayPhase = (rand() % 100) / 10.0f; // 랜덤 시작 위상

			// 계절별 속성 설정
			if (nowSeason == SPRING) {
				// 봄: 벚꽃
				wp.color = glm::vec3(0.9f, 0.4f, 0.6f); // 연분홍
				wp.scaleVec = glm::vec3(0.15f, 0.02f, 0.15f);
				wp.speed = 0.03f + (rand() % 5) / 100.0f;
			}
			else if (nowSeason == AUTUMN) {
				// 가을: 낙엽
				float rVar = (rand() % 3) / 10.0f; // 색상 약간 다르게
				wp.color = glm::vec3(0.7f + rVar, 0.35f, 0.05f);
				wp.scaleVec = glm::vec3(0.2f, 0.02f, 0.2f);
				wp.speed = 0.06f + (rand() % 5) / 100.0f;
			}
			else if (nowSeason == WINTER) {
				// 겨울: 눈 
				wp.color = glm::vec3(1.0f, 1.0f, 1.0f); // 순백색
				wp.scaleVec = glm::vec3(0.12f, 0.12f, 0.12f);
				wp.speed = 0.1f + (rand() % 10) / 100.0f;
			}

			weatherParticles.push_back(wp);
		}
	}

	// 3. 파티클 업데이트 
	for (auto it = weatherParticles.begin(); it != weatherParticles.end(); ) {
		it->pos.y -= it->speed; // 하강

		// 흔들리는 효과
		it->swayPhase += 0.05f;
		it->pos.x += sin(it->swayPhase) * 0.03f;

		// 바닥 근처까지 오면 삭제
		if (it->pos.y < -1.0f) {
			it = weatherParticles.erase(it);
		}
		else {
			++it;
		}
	}

	glutPostRedisplay();
	glutTimerFunc(16, timer, 0);

}

// 폰트 초기화 함수
void initFont(const char* filename, float pixelHeight) {
	unsigned char temp_bitmap[512 * 512];

	FILE* f = fopen(filename, "rb");
	if (!f) {
		printf("폰트 파일을 찾을 수 없습니다: %s\n", filename);
		return;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	unsigned char* ttf_buffer = (unsigned char*)malloc(size);
	fread(ttf_buffer, 1, size, f);
	fclose(f);

	stbtt_BakeFontBitmap(ttf_buffer, 0, pixelHeight, temp_bitmap, 512, 512, 32, 96, cdata);
	free(ttf_buffer);

	glGenTextures(1, &fontTexture);
	glBindTexture(GL_TEXTURE_2D, fontTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY, 512, 512, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, temp_bitmap);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void initGame()
{
	transLoc = glGetUniformLocation(shaderProgramID, "trans");
	//for (int z = -10; z < 10; ++z) generateLane(z);

	float vertices[] = {
		// 위치(X,Y,Z) // 텍스처 좌표(U,V)
		-0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
		0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		-0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
		0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
		-0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

		-0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
		-0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
		-0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

		0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
		0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
		0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

		-0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
		-0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
		-0.5f, 0.5f, -0.5f, 0.0f, 1.0f
	};

	// 바퀴창문
	glm::vec3 wheelColor = glm::vec3(0.1f, 0.1f, 0.1f); // 검은색
	glm::vec3 windowColor = glm::vec3(0.2f, 0.2f, 0.3f); // 짙은 파란색/회색

	//바퀴4개
	float wheelXOffset = 0.6f;
	float wheelZOffset = 0.45f;
	float wheelYOffset = -0.2f; // 바퀴는 바닥에 가깝게
	float wheelScale = 0.3f;

	//세단스타일
	CarDesign sedan;
	sedan.baseScale = 1.0f;
	//바디길고납작
	sedan.parts.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.7f, 0.5f, 0.8f), glm::vec3(0.0f) }); // color는 Car.color 사용
	// 캐빈/지붕앞쪽위
	sedan.parts.push_back({ glm::vec3(0.1f, 0.35f, 0.0f), glm::vec3(0.8f, 0.4f, 0.7f), glm::vec3(1.0f, 1.0f, 1.0f) }); // 흰색 지붕

	//앞바퀴
	sedan.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	sedan.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });
	//뒷바퀴
	sedan.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	sedan.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });

	//창문 추가
	float windowXOffset = 0.1f; //캐빈 중앙으로부터의 X 오프셋
	float windowYOffset = 0.35f;
	float windowScaleX = 0.7f;
	float windowScaleY = 0.3f;
	// 옆 창문
	sedan.parts.push_back({ glm::vec3(windowXOffset, windowYOffset, 0.45f), glm::vec3(windowScaleX, windowScaleY, 0.1f), windowColor });
	sedan.parts.push_back({ glm::vec3(windowXOffset, windowYOffset, -0.45f), glm::vec3(windowScaleX, windowScaleY, 0.1f), windowColor });
	carDesigns.push_back(sedan);

	//2.suv
	CarDesign suv_pickup;
	suv_pickup.baseScale = 1.0f;
	//바디
	suv_pickup.parts.push_back({ glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(1.5f, 0.5f, 0.9f), glm::vec3(0.0f) }); // Car.color 사용
	//앞부분
	suv_pickup.parts.push_back({ glm::vec3(0.65f, 0.2f, 0.0f), glm::vec3(0.6f, 0.2f, 0.8f), glm::vec3(0.0f) }); // Car.color 사용
	//캐빈/지붕
	suv_pickup.parts.push_back({ glm::vec3(0.2f, 0.5f, 0.0f), glm::vec3(0.8f, 0.4f, 0.8f), glm::vec3(1.0f, 1.0f, 1.0f) }); // 흰색 지붕
	suv_pickup.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	suv_pickup.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });
	// 뒷바퀴
	suv_pickup.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	suv_pickup.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });

	//창문추가
	// 창문 (측면)
	suv_pickup.parts.push_back({ glm::vec3(0.2f, 0.5f, 0.45f), glm::vec3(0.6f, 0.3f, 0.1f), windowColor });
	suv_pickup.parts.push_back({ glm::vec3(0.2f, 0.5f, -0.45f), glm::vec3(0.6f, 0.3f, 0.1f), windowColor });
	carDesigns.push_back(suv_pickup);

	//3. 트럭
	CarDesign truck;
	truck.baseScale = 1.5f;
	//앞 캐빈
	truck.parts.push_back({ glm::vec3(0.5f, 0.1f, 0.0f), glm::vec3(0.7f, 0.7f, 0.9f), glm::vec3(0.0f) }); // Car.color 사용
	//뒷부분
	truck.parts.push_back({ glm::vec3(-0.7f, 0.2f, 0.0f), glm::vec3(2.0f, 1.2f, 0.95f), glm::vec3(0.9f, 0.9f, 0.9f) }); // 회색 화물칸
	//캐빈/지붕 (흰색)
	truck.parts.push_back({ glm::vec3(0.5f, 0.6f, 0.0f), glm::vec3(0.7f, 0.4f, 0.8f), glm::vec3(1.0f, 1.0f, 1.0f) }); // 흰색 지붕
	truck.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	truck.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });
	//뒷바퀴
	truck.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	truck.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });

	// 창문 추가
	// 창문 (측면)
	truck.parts.push_back({ glm::vec3(0.2f, 0.5f, 0.45f), glm::vec3(0.6f, 0.3f, 0.1f), windowColor });
	truck.parts.push_back({ glm::vec3(0.2f, 0.5f, -0.45f), glm::vec3(0.6f, 0.3f, 0.1f), windowColor });
	carDesigns.push_back(truck);

	//4. 택시
	CarDesign taxi = suv_pickup;
	taxi.baseScale = 1.0f;
	//램프 추가
	taxi.parts.push_back({ glm::vec3(0.2f, 0.8f, 0.0f), glm::vec3(0.2f, 0.1f, 0.5f), glm::vec3(1.0f, 1.0f, 0.0f) }); // 노란색 램프
	taxi.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	taxi.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });
	// 뒷바퀴
	taxi.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	taxi.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });

	//창문
	// 창문 (측면)
	taxi.parts.push_back({ glm::vec3(0.2f, 0.5f, 0.45f), glm::vec3(0.6f, 0.3f, 0.1f), windowColor });
	taxi.parts.push_back({ glm::vec3(0.2f, 0.5f, -0.45f), glm::vec3(0.6f, 0.3f, 0.1f), windowColor });
	carDesigns.push_back(taxi);
	glGenFramebuffers(1, &depthFBO);

	for (int z = -10; z < 10; ++z) generateLane(z);

	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
		SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0,1.0,1.0,1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// 1. 위치 속성 (Location 0)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// 2. 텍스처 좌표 속성 (Location 2) - stride는 5, offset은 3
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);

	loadTexture("river.jpg", &riverTexture);
	loadTexture("grass.jpg", &grassTexture);
	loadTexture("wood.jpg", &logTexture);
	loadTexture("lilypad.jpg", &lilyPadTexture);

}

bool isTreeAt(int x, int z) {
	if (treeMap.count(z)) {
		for (int treeX : treeMap[z]) if (treeX == x) return true;
	}
	return false;
}

void specialKeyboard(int key, int x, int y)
{
	if (isMoving) return;
	if (isMoving || isDashing) return;

	int nextX = (int)std::round(playerPos.x);
	int nextZ = (int)std::round(playerPos.z);

	switch (key) {
	case GLUT_KEY_UP:
		playerRotation = 0.0f;
		nextZ -= 1;
		break;
	case GLUT_KEY_DOWN:
		playerRotation = 180.0f;
		nextZ += 1;
		break;
	case GLUT_KEY_LEFT:
		playerRotation = 90.0f;
		nextX -= 1;
		break;
	case GLUT_KEY_RIGHT:
		playerRotation = -90.0f;
		nextX += 1;
		break;
	default: return;
	}

	if (!isTreeAt(nextX, nextZ)) {
		if (key == GLUT_KEY_UP && nextZ < minZ) {
			minZ = nextZ;
			score++;
			printf("Score: %d\n", score);

			int currentTotalLines = -minZ;

			//if (currentTotalLines > 0 && (currentTotalLines % LINES_PER_SEASON) == 0) {
			// //linesPassedSinceSeasonChange = 0;

			// // 다음 계절로 전환
			// switch (currentSeason) {
			// case SPRING: currentSeason = SUMMER; break;
			// case SUMMER: currentSeason = AUTUMN; break;
			// case AUTUMN: currentSeason = WINTER; break;
			// case WINTER: currentSeason = SPRING; break;
			// }
			//
			// printf("Season changed to %d (Lines passed: %d)\n", currentSeason, LINES_PER_SEASON);
			//}

		}

		playerStartPos = playerPos;

		playerTargetPos = glm::vec3((float)nextX, 0.5f, (float)nextZ);

		isMoving = true;
		moveTime = 0.0f;
	}
	glutPostRedisplay();
}

// 일반 키보드 입력 처리
void keyboard(unsigned char key, int x, int y)
{
	if (key == 'q' || key == 'Q') {
		exit(0);
	}

	if (key == ' ') {
		if (isEventActive) {
			requiredTaps++;

			// 게이지 상승 로직 (TAP_PER_SECOND가 10이고 이벤트가 4초라면 총 40번 누르면 게이지가 1.0이 됨)
			// eventProgress는 0.0 ~ 1.0 사이 값으로 계산되어야 합니다.
			// 4초 * TAP_PER_SECOND = 총 목표 횟수
			const float TOTAL_TAPS_REQUIRED = TAP_PER_SECOND * 4.0f;
			eventProgress = requiredTaps / TOTAL_TAPS_REQUIRED;
			eventProgress = glm::clamp(eventProgress, 0.0f, 1.1f);

			printf("Tap! Current Progress: %.2f\n", eventProgress);
			return;
		}

		// 연타 이벤트 중이 아닐 때 스페이스바를 누른 경우
		if (!isMoving && !isFlying && !isLanding) {
			printf("현재 스페이스바는 로켓 라이드 이벤트 중에만 사용 가능합니다.\n");
		}
	}

	if (key == '2') {
		if (!isNightMode && !isNightWarning) {
			isNightWarning = true;
			nightWarningTimer = 3.0f; // 3초간 경고 후 암전
			printf(">>> 테스트: 핀 조명(암전) 이벤트 강제 활성화! >>>\n");
		}
		else {
			isNightWarning = false;
			isNightMode = false;
			nightModeTimer = 0.0f;
			printf("테스트: 핀 조명 이벤트 강제 종료.\n");
		}
	}

	if (key == '3') {
		if (!isWindActive) {
			isWindActive = true;
			windTimer = 6.0f; // 6초 지속
			windForce = 0.04f; // 오른쪽으로 밀기
			printf(">>> 테스트: 강풍 강제 활성화! >>>\n");
		}
		else {
			isWindActive = false;
			printf("테스트: 강풍 강제 종료.\n");
		}
	}

	if (key == '4' || key == '5' || key == '6') {
		Item newItem;
		newItem.x = (float)std::round(playerPos.x);       // 현재 내 라인
		newItem.z = (float)std::round(playerPos.z) - 2.0f; // 2칸 앞 (진행방향)
		newItem.isCollected = false;
		newItem.rotation = 0.0f;

		if (key == '4') {
			newItem.type = ITEM_SHIELD;
			printf(">>> 치트: 보호막(Shield) 아이템 생성!\n");
		}
		else if (key == '5') {
			newItem.type = ITEM_MAGNET;
			printf(">>> 치트: 자석(Magnet) 아이템 생성!\n");
		}
		else if (key == '6') {
			newItem.type = ITEM_CLOCK;
			printf(">>> 치트: 시계(Slow) 아이템 생성!\n");
		}

		items.push_back(newItem);
	}
}

GLvoid Reshape(int w, int h)
{
	glViewport(0, 0, w, h);
}

// 기존 filetobuf 함수를 지우고 이 코드로 덮어씌우세요.
char* filetobuf(const char* file)
{
	FILE* fptr;
	long length;
	char* buf;

	fptr = fopen(file, "rb");
	if (!fptr) {
		// [중요] 파일을 못 찾으면 에러 메시지를 빨간 글씨로 출력하고 멈춥니다.
		fprintf(stderr, "--------------------------------------------------------\n");
		fprintf(stderr, "[치명적 오류] 파일을 찾을 수 없습니다: %s\n", file);
		fprintf(stderr, "프로젝트 폴더에 해당 파일이 있는지 확인해주세요.\n");
		fprintf(stderr, "--------------------------------------------------------\n");
		system("pause"); // 에러를 볼 수 있게 콘솔창 정지
		exit(EXIT_FAILURE);
		return NULL;
	}

	fseek(fptr, 0, SEEK_END);
	length = ftell(fptr);
	buf = (char*)malloc(length + 1);
	fseek(fptr, 0, SEEK_SET);
	fread(buf, length, 1, fptr);
	fclose(fptr);
	buf[length] = 0;

	return buf;
}

void make_vertexShaders()
{
	GLchar* vertexSource;

	vertexSource = filetobuf("vertex.glsl");
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);

	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, errorLog);
		std::cerr << "ERROR: vertex shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

void make_fragmentShaders()
{
	GLchar* fragmentSource;

	fragmentSource = filetobuf("fragment.glsl");
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);

	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, errorLog);
		std::cerr << "ERROR: frag_shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

GLuint make_shaderProgram()
{
	GLint result;
	GLchar* errorLog = NULL;
	GLuint shaderID;
	shaderID = glCreateProgram();

	glAttachShader(shaderID, vertexShader);
	glAttachShader(shaderID, fragmentShader);

	glLinkProgram(shaderID);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glGetProgramiv(shaderID, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(shaderID, 512, NULL, errorLog);
		std::cerr << "ERROR: shader program 연결 실패\n" << errorLog << std::endl;
		return false;
	}

	glUseProgram(shaderID);

	return shaderID;
}

void loadDepthShader()
{
	GLint result;
	GLchar errorLog[512];

	depthShader = glCreateProgram();

	//쉐이더로드
	GLuint depthVertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLchar* dvSource = filetobuf("depthvertex.glsl");
	glShaderSource(depthVertexShader, 1, &dvSource, NULL);
	glCompileShader(depthVertexShader);
	glAttachShader(depthShader, depthVertexShader);

	glGetShaderiv(depthVertexShader, GL_COMPILE_STATUS, &result);
	if (!result) {
		glGetShaderInfoLog(depthVertexShader, 512, NULL, errorLog);
		std::cerr << "ERROR: depth vertex shader 컴파일 실패\n" << errorLog << std::endl;

	}
	GLuint depthFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	GLchar* dfSource = filetobuf("depthfragment.glsl");
	glShaderSource(depthFragmentShader, 1, &dfSource, NULL);
	glCompileShader(depthFragmentShader);
	glAttachShader(depthShader, depthFragmentShader);

	glGetShaderiv(depthFragmentShader, GL_COMPILE_STATUS, &result);
	if (!result) {
		glGetShaderInfoLog(depthFragmentShader, 512, NULL, errorLog);
		std::cerr << "ERROR: depth fragment shader 컴파일 실패\n" << errorLog << std::endl;
	}

	glLinkProgram(depthShader);

	glGetProgramiv(depthShader, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(depthShader, 512, NULL, errorLog);
		std::cerr << "ERROR: depth shader program 연결 실패\n" << errorLog << std::endl;
	}

	glDeleteShader(depthVertexShader);
	glDeleteShader(depthFragmentShader);
}