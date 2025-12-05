#define _CRT_SECURE_NO_WARNINGS 
#define STB_TRUETYPE_IMPLEMENTATION 
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

//자동차 부품
struct CarPart {
	glm::vec3 offset; // 차량 중앙을 기준으로 한 상대적 위치
	glm::vec3 scale;  // 크기
	glm::vec3 color;  // 부품의 색상
};

// 자동차 모델 정의
struct CarDesign {
	std::vector<CarPart> parts;
	float baseScale; //전체크기조절
};

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

glm::vec3 lightPos(-15.0f, 20.0f, 0.0f);

std::map<int, int> mapType; // 0=잔디 1=도로
std::map<int, std::vector<int>> treeMap;
std::vector<Car> cars;
std::vector<CarDesign> carDesigns; 

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
	initFont("Cafe24PROUP.ttf", 60.0f);

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

			// 디자인 ID 랜덤 할당
			if (!carDesigns.empty()) {
				newCar.designID = rand() % carDesigns.size();
			}
			else {
				newCar.designID = 0; // 디자인이 없으면 기본값
			}

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
				glVertexAttrib3f(1, 0.2f, 0.2f, 0.2f);
			}
			else { // 잔디 (연한 초록)
				glVertexAttrib3f(1, 0.47f, 0.9f, 0.42f);
			}
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);

		// 차선
        if (mapType[z] == 1) { 
            glm::vec3 lineScale = glm::vec3(1.0f, 0.02f, 0.15f); // 길이, 높이, 폭

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

		if (treeMap.count(z)) {
			for (int treeX : treeMap[z]) drawTree(treeX, z,shader);
		}
	}

	// 자동차들
	for (const auto& car : cars) {
		if (car.z < currentZ - drawRangeFront || car.z > currentZ + drawRangeBack) continue;
		/*glm::mat4 carModel = glm::translate(glm::mat4(1.0f), glm::vec3(car.x, 0.5f, car.z));
		carModel = glm::scale(carModel, glm::vec3(1.5f, 0.8f, 0.8f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(carModel));
		if (shader == shaderProgramID) {
			glVertexAttrib3f(1, car.color.r, car.color.g, car.color.b);
		}*/
		//glDrawArrays(GL_TRIANGLES, 0, 36);
		drawCar(car, shader);
	}

	// 플레이어
	glm::mat4 pModel = glm::translate(glm::mat4(1.0f), playerPos);

	// [추가] 저장된 각도만큼 Y축을 기준으로 회전
	pModel = glm::rotate(pModel, glm::radians(playerRotation), glm::vec3(0.0f, 1.0f, 0.0f));

	// 크기 조절 (기존 0.7f)
	pModel = glm::scale(pModel, glm::vec3(0.7f));
	drawChicken(shader, pModel);


	glDrawArrays(GL_TRIANGLES, 0, 36);
}

//  테두리 있는 텍스트 그리기 함수
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
	glUseProgram(0); // 쉐이더 끄기

	// 2D 투영 설정
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, 1280, 0, 960); // 윈도우 해상도에 맞춤 (좌하단 0,0)

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, fontTexture);

	glBegin(GL_QUADS);
	glColor3f(r, g, b);

	while (*text) {
		if (*text >= 32 && *text < 128) {
			stbtt_aligned_quad q;
			// 문자의 위치(Quad)와 텍스처 좌표(UV)를 계산해줌
			stbtt_GetBakedQuad(cdata, 512, 512, *text - 32, &x, &y, &q, 1);

			glTexCoord2f(q.s0, q.t0); glVertex2f(q.x0, 960 - q.y0); // Y좌표 반전 주의
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

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glUseProgram(shaderProgramID); // 쉐이더 복구
}

GLvoid drawScene()
{
	
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.f / 960.f, 0.1f, 100.f);
	glm::mat4 view = glm::lookAt(playerPos + glm::vec3(2, 12, 10), playerPos, glm::vec3(0, 1, 0));

	glm::mat4 lightProjection = glm::ortho(-50.f, 50.f, -50.f, 50.f, 1.f, 100.f);
	glm::mat4 lightView = glm::lookAt(lightPos, playerPos, glm::vec3(0, 1, 0));
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	// 패스2
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUseProgram(depthShader);

	glUniformMatrix4fv(glGetUniformLocation(depthShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
	renderObjects(depthShader, lightSpaceMatrix);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	// 패스2
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

	// 점수 표시 (좌측 상단)
	std::string scoreStr = "SCORE: " + std::to_string(score);
	renderTextWithOutline(20, 60, scoreStr.c_str());

	glutSwapBuffers();
}

void timer(int value)
{
	for (auto& car : cars) {
		car.x += car.speed;
		if (car.x > 15.0f && car.speed > 0) car.x = -15.0f;
		if (car.x < -15.0f && car.speed < 0) car.x = 15.0f;

		if (abs(car.z - playerPos.z) < 0.08f && abs(car.x - playerPos.x) < 1.2f) {
			playerPos = glm::vec3(0.0f, 0.5f, 0.0f);
			playerTargetPos = playerPos;
			playerStartPos = playerPos;
			isMoving = false;
			score = 0;
			minZ = 0;
		}
	}
	if (isMoving) {
		
		moveTime += 0.016f;

		// 보간 비율 t 계산 
		float t = glm::clamp(moveTime / MOVE_DURATION, 0.0f, 1.0f);

		//점프할려고
		playerPos.x = glm::mix(playerStartPos.x, playerTargetPos.x, t);
		playerPos.z = glm::mix(playerStartPos.z, playerTargetPos.z, t);

		//포물선
		//t * (1.0f - t)
		float jumpY = JUMP_HEIGHT * 4.0f * t * (1.0f - t); 
		playerPos.y = 0.5f + jumpY; 

		if (t >= 1.0f) {
			
			playerPos = playerTargetPos;
			playerPos.y = 0.5f;
			isMoving = false;
		}
	}
	glutPostRedisplay();
	glutTimerFunc(16, timer, 0);
}

// 폰트 초기화 함수
void initFont(const char* filename, float pixelHeight) {
	unsigned char temp_bitmap[512 * 512]; // 폰트를 구울 비트맵 버퍼 (크기는 조절 가능)

	// 1. TTF 파일 읽기
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

	// 2. 비트맵에 글자 굽기 (ASCII 32번부터 96개 글자)
	stbtt_BakeFontBitmap(ttf_buffer, 0, pixelHeight, temp_bitmap, 512, 512, 32, 96, cdata);
	free(ttf_buffer);

	// 3. OpenGL 텍스처 생성
	glGenTextures(1, &fontTexture);
	glBindTexture(GL_TEXTURE_2D, fontTexture);

	// 텍스처 설정 (글자가 흐릿하면 GL_NEAREST 사용)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 512, 512, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void initGame()
{
	transLoc = glGetUniformLocation(shaderProgramID, "trans");
	//for (int z = -10; z < 10; ++z) generateLane(z);

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

	//  창문 추가
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
	if (isMoving) return;
	

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
		}

		playerStartPos = playerPos;

		playerTargetPos = glm::vec3((float)nextX, 0.5f, (float)nextZ);

		
		isMoving = true;
		moveTime = 0.0f;
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