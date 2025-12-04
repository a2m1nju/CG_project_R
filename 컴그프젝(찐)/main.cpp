#define _CRT_SECURE_NO_WARNINGS 

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>   
#include <ctime>
#include <algorithm> 
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
};

// 전역 변수
GLuint vao, vbo;
GLuint transLoc;
glm::vec3 playerPos(0.0f, 0.5f, 0.0f);
glm::mat4 PV; // 나무 그리기 함수와 공유할 전역 행렬
GLuint depthFBO, depthMap;
GLuint depthShader;
const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;

glm::vec3 lightPos(10.0f, 20.0f, 10.0f);

std::map<int, int> mapType; // 0=잔디 1=도로
std::map<int, std::vector<int>> treeMap;
std::vector<Car> cars;

// 함수 선언
GLvoid drawScene(GLvoid);
GLvoid Reshape(int w, int h);
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
char* filetobuf(const char* file);

void initGame();
void generateLane(int z);
void specialKeyboard(int key, int x, int y);
void timer(int value);
bool isTreeAt(int x, int z);
void drawTree(int x, int z); // 복셀 나무 그리기 함수
void loadDepthShader();

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

	glEnable(GL_DEPTH_TEST);

	glutDisplayFunc(drawScene);
	glutReshapeFunc(Reshape);
	glutSpecialFunc(specialKeyboard);
	glutTimerFunc(16, timer, 0);

	glutMainLoop();
}

// 퍼지는 모양 대신 위로 쌓아올린 박스형 나무
void drawTree(int x, int z,GLuint shader) {
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
			glVertexAttrib3f(1, 1.0f, 0.2f, 0.0f);
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
					if (yOffset == 0) glVertexAttrib3f(1, 0.0f, 0.5f, 0.0f); // 진한 초록
					else if (yOffset == 1) glVertexAttrib3f(1, 0.1f, 0.6f, 0.1f);
					else glVertexAttrib3f(1, 0.2f, 0.7f, 0.2f); // 밝은 초록
				}
				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}
	}
}

void generateLane(int z)
{
	if (mapType.find(z) != mapType.end()) return;

	if (z >= -2 && z <= 2) {
		mapType[z] = 0;
		return;
	}

	if (rand() % 10 < 5) { // 도로
		mapType[z] = 1;
		int numCars = 1 + rand() % 2;
		float speed = (0.1f + (rand() % 5) / 20.0f);
		if (rand() % 2 == 0) speed *= -1.0f;

		for (int i = 0; i < numCars; ++i) {
			Car newCar;
			newCar.z = (float)z;
			newCar.x = (float)(rand() % 30 - 15);
			newCar.speed = speed;
			newCar.color = glm::vec3((rand() % 10) / 10.f, (rand() % 10) / 10.f, (rand() % 10) / 10.f);
			if (newCar.color.r < 0.2f) newCar.color.r += 0.5f;
			cars.push_back(newCar);
		}
	}
	else { // 잔디
		mapType[z] = 0;
		for (int x = -15; x <= 15; ++x) {
			if (rand() % 10 < 1) {
				treeMap[z].push_back(x);
			}
		}
	}
}
void renderObjects(GLuint shader, const glm::mat4& pvMatrix)
{
	int currentZ = (int)std::round(playerPos.z);
	int drawRangeFront = 30;
	int drawRangeBack = 10;

	for (int z = currentZ - drawRangeFront; z <= currentZ + drawRangeBack; ++z) {
		generateLane(z);

		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, -0.5f, (float)z));
		model = glm::scale(model, glm::vec3(31.0f, 1.0f, 1.0f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) {
			if (mapType[z] == 1) { // 도로 (회색)
				glVertexAttrib3f(1, 0.3f, 0.3f, 0.3f);
			}
			else { // 잔디 (연한 초록)
				glVertexAttrib3f(1, 0.3f, 0.8f, 0.3f);
			}
		}

		glDrawArrays(GL_TRIANGLES, 0, 36);

		if (treeMap.count(z)) {
			for (int treeX : treeMap[z]) drawTree(treeX, z,shader);
		}
	}

	// 자동차들
	for (const auto& car : cars) {
		if (car.z < currentZ - drawRangeFront || car.z > currentZ + drawRangeBack) continue;
		glm::mat4 carModel = glm::translate(glm::mat4(1.0f), glm::vec3(car.x, 0.5f, car.z));
		carModel = glm::scale(carModel, glm::vec3(1.5f, 0.8f, 0.8f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(carModel));
		if (shader == shaderProgramID) {
			glVertexAttrib3f(1, car.color.r, car.color.g, car.color.b);
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 플레이어
	glm::mat4 pModel = glm::translate(glm::mat4(1.0f), playerPos);
	pModel = glm::scale(pModel, glm::vec3(0.7f));

	glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(pModel));
	if (shader == shaderProgramID) {
		glVertexAttrib3f(1, 1.0f, 0.6f, 0.0f);
	}
	glDrawArrays(GL_TRIANGLES, 0, 36);
}


GLvoid drawScene()
{
	

	glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.f / 960.f, 0.1f, 100.f);
	glm::mat4 view = glm::lookAt(playerPos + glm::vec3(2, 12, 10), playerPos, glm::vec3(0, 1, 0));

	glm::mat4 lightProjection = glm::ortho(-50.f, 50.f, -50.f, 50.f, 1.f, 100.f);
	glm::mat4 lightView = glm::lookAt(lightPos, playerPos, glm::vec3(0, 1, 0));
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	// PASS 1: Shadow depth map
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUseProgram(depthShader);

	glUniformMatrix4fv(glGetUniformLocation(depthShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
	renderObjects(depthShader, lightSpaceMatrix);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	// PASS 2: Final render
	glViewport(0, 0, 1280, 960);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);

	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
	glUniform3fv(glGetUniformLocation(shaderProgramID, "lightPos"), 1, &lightPos[0]);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glUniform1i(glGetUniformLocation(shaderProgramID, "shadowMap"), 0);

	renderObjects(shaderProgramID, proj * view);

	glutSwapBuffers();
}

void timer(int value)
{
	for (auto& car : cars) {
		car.x += car.speed;
		if (car.x > 15.0f && car.speed > 0) car.x = -15.0f;
		if (car.x < -15.0f && car.speed < 0) car.x = 15.0f;

		if (abs(car.z - playerPos.z) < 0.5f && abs(car.x - playerPos.x) < 1.0f) {
			playerPos = glm::vec3(0.0f, 0.5f, 0.0f);
		}
	}
	glutPostRedisplay();
	glutTimerFunc(16, timer, 0);
}

void initGame()
{
	transLoc = glGetUniformLocation(shaderProgramID, "trans");
	for (int z = -10; z < 10; ++z) generateLane(z);

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

	glGenFramebuffers(1, &depthFBO);

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
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
}

bool isTreeAt(int x, int z) {
	if (treeMap.count(z)) {
		for (int treeX : treeMap[z]) if (treeX == x) return true;
	}
	return false;
}

void specialKeyboard(int key, int x, int y)
{
	int nextX = (int)std::round(playerPos.x);
	int nextZ = (int)std::round(playerPos.z);
	switch (key) {
	case GLUT_KEY_UP:    nextZ -= 1; break;
	case GLUT_KEY_DOWN:  nextZ += 1; break;
	case GLUT_KEY_LEFT:  nextX -= 1; break;
	case GLUT_KEY_RIGHT: nextX += 1; break;
	}
	if (!isTreeAt(nextX, nextZ)) {
		playerPos.x = (float)nextX;
		playerPos.z = (float)nextZ;
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
void loadDepthShader()
{
	GLint result;
	GLchar errorLog[512];

	// 깊이 쉐이더 프로그램 생성
	depthShader = glCreateProgram();

	// depthvertex.glsl 로드 및 컴파일
	GLuint depthVertexShader = glCreateShader(GL_VERTEX_SHADER);
	GLchar* dvSource = filetobuf("depthvertex.glsl");
	glShaderSource(depthVertexShader, 1, &dvSource, NULL);
	glCompileShader(depthVertexShader);
	glAttachShader(depthShader, depthVertexShader);

	// 컴파일 에러 체크 (추가 권장)
	glGetShaderiv(depthVertexShader, GL_COMPILE_STATUS, &result);
	if (!result) {
		glGetShaderInfoLog(depthVertexShader, 512, NULL, errorLog);
		std::cerr << "ERROR: depth vertex shader 컴파일 실패\n" << errorLog << std::endl;
		// exit(EXIT_FAILURE); // 필요하면 강제 종료
	}


	// depthfragment.glsl 로드 및 컴파일
	GLuint depthFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	GLchar* dfSource = filetobuf("depthfragment.glsl");
	glShaderSource(depthFragmentShader, 1, &dfSource, NULL);
	glCompileShader(depthFragmentShader);
	glAttachShader(depthShader, depthFragmentShader);

	// 컴파일 에러 체크 (추가 권장)
	glGetShaderiv(depthFragmentShader, GL_COMPILE_STATUS, &result);
	if (!result) {
		glGetShaderInfoLog(depthFragmentShader, 512, NULL, errorLog);
		std::cerr << "ERROR: depth fragment shader 컴파일 실패\n" << errorLog << std::endl;
	}


	// 프로그램 링크
	glLinkProgram(depthShader);

	// 링크 에러 체크 (추가 권장)
	glGetProgramiv(depthShader, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(depthShader, 512, NULL, errorLog);
		std::cerr << "ERROR: depth shader program 연결 실패\n" << errorLog << std::endl;
	}

	// 쉐이더 오브젝트 정리
	glDeleteShader(depthVertexShader);
	glDeleteShader(depthFragmentShader);
}