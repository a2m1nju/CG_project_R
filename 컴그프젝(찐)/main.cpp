#define _CRT_SECURE_NO_WARNINGS 

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>    // [추가] 맵을 무한대로 관리하기 위해 필요
#include <ctime>
#include <algorithm> // for remove_if
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

// --- 구조체 정의 ---
struct Car {
	float x, z;       // 위치
	float speed;      // 속도
	glm::vec3 color;  // 차 색상
};

// --- 전역 변수 ---
GLuint vao, vbo;
GLuint transLoc;
glm::vec3 playerPos(0.0f, 0.5f, 0.0f);

// [변경] 고정 배열 대신 std::map 사용 (Key: Z좌표, Value: 0=잔디, 1=도로)
// 맵이 무한히 늘어나도 관리할 수 있습니다.
std::map<int, int> mapType;
std::vector<Car> cars;

// --- 함수 선언 ---
GLvoid drawScene(GLvoid);
GLvoid Reshape(int w, int h);
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
char* filetobuf(const char* file);

void initGame();
void generateLane(int z); // [추가] 특정 Z 위치의 맵을 생성하는 함수
void specialKeyboard(int key, int x, int y);
void timer(int value);

GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;

int main(int argc, char** argv)
{
	srand((unsigned int)time(NULL));

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(1280, 960);
	glutCreateWindow("Infinite Crossy Road");

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		std::cerr << "Unable to initialize GLEW" << std::endl;
		exit(EXIT_FAILURE);
	}

	make_vertexShaders();
	make_fragmentShaders();
	shaderProgramID = make_shaderProgram();

	initGame();

	glEnable(GL_DEPTH_TEST);

	glutDisplayFunc(drawScene);
	glutReshapeFunc(Reshape);
	glutSpecialFunc(specialKeyboard);
	glutTimerFunc(16, timer, 0);

	glutMainLoop();
}

// [추가] 해당 Z좌표에 맵이 없으면 새로 생성하는 함수
void generateLane(int z)
{
	// 이미 해당 줄(z)에 데이터가 있다면 생략
	if (mapType.find(z) != mapType.end()) return;

	// 시작 지점 근처(z: -5 ~ 5)는 안전하게 잔디로 고정
	if (z > -5 && z < 5) {
		mapType[z] = 0;
		return;
	}

	// 30% 확률로 도로 생성
	if (rand() % 10 < 5) {
		mapType[z] = 1; // 도로

		// 차 생성 로직
		int numCars = 1 + rand() % 2; // 1~2대
		float speed = (0.05f + (rand() % 5) / 20.0f);
		if (rand() % 2 == 0) speed *= -1.0f; // 방향 랜덤

		for (int i = 0; i < numCars; ++i) {
			Car newCar;
			newCar.z = (float)z;
			newCar.x = (float)(rand() % 30 - 15);
			newCar.speed = speed;

			// 차 색상 랜덤
			newCar.color = glm::vec3(
				(rand() % 10) / 10.0f,
				(rand() % 10) / 10.0f,
				(rand() % 10) / 10.0f
			);
			if (newCar.color.r < 0.2f) newCar.color.r += 0.5f; // 너무 어두운 색 방지

			cars.push_back(newCar);
		}
	}
	else {
		mapType[z] = 0; // 잔디
	}
}

GLvoid drawScene()
{
	glClearColor(0.6f, 0.8f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shaderProgramID);
	glBindVertexArray(vao);

	// 1. 카메라 설정
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.0f / 960.0f, 0.1f, 100.0f);
	glm::vec3 cameraOffset(0.0f, 12.0f, 10.0f);
	glm::mat4 view = glm::lookAt(playerPos + cameraOffset, playerPos, glm::vec3(0, 1, 0));
	glm::mat4 PV = proj * view;

	// 2. 무한 맵 그리기 (플레이어 위치 기준 앞뒤로 그리기)
	// 플레이어가 이동함에 따라 그리는 범위(z)도 같이 이동합니다.
	int currentZ = (int)std::round(playerPos.z);
	int drawRangeFront = 30; // 플레이어 앞쪽으로 얼마나 보여줄지
	int drawRangeBack = 10;  // 플레이어 뒤쪽으로 얼마나 보여줄지

	// "앞"은 z값이 감소하는 방향(화면 안쪽)입니다.
	for (int z = currentZ - drawRangeFront; z <= currentZ + drawRangeBack; ++z) {

		generateLane(z); // [중요] 그리려는 곳에 맵이 없으면 즉석에서 생성!

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, -0.5f, (float)z));
		model = glm::scale(model, glm::vec3(30.0f, 1.0f, 1.0f));

		glm::mat4 MVP = PV * model;
		glUniformMatrix4fv(transLoc, 1, GL_FALSE, glm::value_ptr(MVP));

		if (mapType[z] == 0) // 잔디
			glVertexAttrib3f(1, 0.2f, 0.8f, 0.2f);
		else // 도로
			glVertexAttrib3f(1, 0.3f, 0.3f, 0.3f);

		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 3. 자동차 그리기
	for (const auto& car : cars) {
		// 플레이어 시야 범위 내의 차만 그리기 (최적화)
		if (car.z < currentZ - drawRangeFront || car.z > currentZ + drawRangeBack) continue;

		glm::mat4 carModel = glm::mat4(1.0f);
		carModel = glm::translate(carModel, glm::vec3(car.x, 0.5f, car.z));
		carModel = glm::scale(carModel, glm::vec3(1.5f, 0.8f, 0.8f));

		glm::mat4 carMVP = PV * carModel;
		glUniformMatrix4fv(transLoc, 1, GL_FALSE, glm::value_ptr(carMVP));

		glVertexAttrib3f(1, car.color.r, car.color.g, car.color.b);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 4. 플레이어 그리기
	glm::mat4 pModel = glm::mat4(1.0f);
	pModel = glm::translate(pModel, playerPos);
	pModel = glm::scale(pModel, glm::vec3(0.7f, 0.7f, 0.7f));

	glm::mat4 pMVP = PV * pModel;
	glUniformMatrix4fv(transLoc, 1, GL_FALSE, glm::value_ptr(pMVP));

	glVertexAttrib3f(1, 1.0f, 0.6f, 0.0f);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	glutSwapBuffers();
}

void timer(int value)
{
	int currentZ = (int)std::round(playerPos.z);

	// 1. 자동차 이동 및 관리
	// (플레이어보다 너무 뒤처진 차(z > currentZ + 20)는 삭제하는 것이 메모리에 좋지만, 
	//  간단한 구현을 위해 여기서는 이동만 처리합니다)
	for (auto& car : cars) {
		car.x += car.speed;

		if (car.x > 15.0f && car.speed > 0) car.x = -15.0f;
		if (car.x < -15.0f && car.speed < 0) car.x = 15.0f;

		// 2. 충돌 처리
		if (abs(car.z - playerPos.z) < 0.5f && abs(car.x - playerPos.x) < 1.0f) {
			std::cout << "Crash! Resetting Position." << std::endl;
			playerPos = glm::vec3(0.0f, 0.5f, 0.0f);
		}
	}

	glutPostRedisplay();
	glutTimerFunc(16, timer, 0);
}

void initGame()
{
	transLoc = glGetUniformLocation(shaderProgramID, "trans");

	// 초기에 플레이어 주변 맵 미리 생성
	for (int z = -10; z < 10; ++z) {
		generateLane(z);
	}

	// 큐브 데이터 (기존과 동일)
	float vertices[] = {
		-0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
		-0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
		-0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,
		 0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
		-0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
		 0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f
	};

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
}

void specialKeyboard(int key, int x, int y)
{
	float speed = 1.0f;
	switch (key) {
	case GLUT_KEY_UP:    playerPos.z -= speed; break; // 앞으로 (화면 안쪽)
	case GLUT_KEY_DOWN:  playerPos.z += speed; break; // 뒤로
	case GLUT_KEY_LEFT:  playerPos.x -= speed; break;
	case GLUT_KEY_RIGHT: playerPos.x += speed; break;
	}
	glutPostRedisplay();
}

GLvoid Reshape(int w, int h)
{
	glViewport(0, 0, w, h);
}

char* filetobuf(const char* file)
{
	FILE* fptr;
	long length;
	char* buf;

	fptr = fopen(file, "rb");
	if (!fptr)
		return NULL;
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