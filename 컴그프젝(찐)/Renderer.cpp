#include "Renderer.h"
#include "Global.h"
#include "Logic.h" 

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

void drawPart(GLuint shader, glm::mat4 parentModel, glm::vec3 offset, glm::vec3 scale, glm::vec3 color) {
    glm::mat4 model = glm::translate(parentModel, offset);
    model = glm::scale(model, scale);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
    if (shader == shaderProgramID) {
        glVertexAttrib3f(1, color.r, color.g, color.b);
    }
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void drawChicken(GLuint shader, glm::mat4 baseModel) {

	glm::vec3 white(1.0f, 1.0f, 1.0f);
	glm::vec3 red(1.0f, 0.2f, 0.2f);
	glm::vec3 orange(1.0, 0.270588f, 0.0f);
	glm::vec3 black(0.1f, 0.1f, 0.1f);

	// 1. 몸통 
	drawPart(shader, baseModel, glm::vec3(0.0f, -0.1f, 0.0f), glm::vec3(0.6f, 0.6f, 0.6f), white);

	// 2. 벼슬 
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.25f, 0.1f), glm::vec3(0.15f, 0.15f, 0.2f), red);

	// 3. 부리 
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, -0.35f), glm::vec3(0.15f, 0.15f, 0.15f), orange);

	// 4. 와틀 
	drawPart(shader, baseModel, glm::vec3(0.0f, -0.15f, -0.35f), glm::vec3(0.1f, 0.12f, 0.1f), red);

	// 5. 눈
	drawPart(shader, baseModel, glm::vec3(0.31f, 0.0f, -0.2f), glm::vec3(0.05f, 0.05f, 0.05f), black);
	drawPart(shader, baseModel, glm::vec3(-0.31f, 0.0f, -0.2f), glm::vec3(0.05f, 0.05f, 0.05f), black);

	// 6. 날개 
	drawPart(shader, baseModel, glm::vec3(0.32f, -0.2f, 0.05f), glm::vec3(0.1f, 0.3f, 0.4f), white);
	drawPart(shader, baseModel, glm::vec3(-0.32f, -0.2f, 0.05f), glm::vec3(0.1f, 0.3f, 0.4f), white);

	// 8. 다리 
	drawPart(shader, baseModel, glm::vec3(0.2f, -0.25f, 0.0f), glm::vec3(0.1f, 0.5f, 0.1f), orange);
	drawPart(shader, baseModel, glm::vec3(-0.2f, -0.25f, 0.0f), glm::vec3(0.1f, 0.5f, 0.1f), orange);

	// 9. 발 
	drawPart(shader, baseModel, glm::vec3(0.2f, -0.49f, -0.3f), glm::vec3(0.15f, 0.02f, 0.25f), orange);
	drawPart(shader, baseModel, glm::vec3(-0.2f, -0.49f, -0.3f), glm::vec3(0.15f, 0.02f, 0.25f), orange);

	// 9. 꼬리 
	drawPart(shader, baseModel, glm::vec3(0.0f, -0.1f, 0.35f), glm::vec3(0.3f, 0.3f, 0.1f), white);
}

void drawBird(GLuint shader, glm::mat4 baseModel) {
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
	if (coin.isCollected) return; 

	glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), glm::vec3(coin.x, 0.5f, coin.z));

	baseModel = glm::scale(baseModel, glm::vec3(0.5f)); 

	glm::vec3 yellowColor = glm::vec3(1.0f, 0.9f, 0.0f); 
	glm::vec3 redColor = glm::vec3(1.0f, 0.0f, 0.0f); 
	float C_height = 0.1f; 

	float base_Y_offset = 0.0f;
	float C_Y_offset = base_Y_offset + C_height;

	C_Y_offset = 0.1f;
	base_Y_offset = 0.0f;

	float C_Y_POS = 0.1f; 
	float C_Y_THICKNESS = 0.1f; 

	drawPart(shader, baseModel, glm::vec3(0.0f, base_Y_offset, 0.0f), glm::vec3(0.9f, C_Y_THICKNESS, 0.9f), yellowColor);

	glm::vec3 redColor_C = glm::vec3(1.0f, 0.0f, 0.0f);
	float C_Y_offset_final = C_Y_POS; 
	float C_thick = 0.15f;
	float C_length = 0.4f;
	float C_span = 0.25f; 

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

//  방패 그리기
void drawShieldModel(GLuint shader, glm::mat4 baseModel) {
	// 1. 방패 테두리/판 (은색/회색)
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.6f, 0.7f, 0.1f), glm::vec3(0.7f, 0.7f, 0.8f));

	// 2. 방패 내부 (파란색)
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, 0.06f), glm::vec3(0.45f, 0.55f, 0.1f), glm::vec3(0.0f, 0.0f, 1.0f));

	// 3. 십자가 문양 (흰색)
	// 세로 줄
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, 0.12f), glm::vec3(0.15f, 0.45f, 0.05f), glm::vec3(1.0f, 1.0f, 1.0f));
	// 가로 줄
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.05f, 0.12f), glm::vec3(0.35f, 0.15f, 0.05f), glm::vec3(1.0f, 1.0f, 1.0f));

	// 4. 손잡이 (뒤쪽)
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, -0.1f), glm::vec3(0.2f, 0.2f, 0.1f), glm::vec3(0.4f, 0.2f, 0.0f));
}

//  자석 그리기 
void drawMagnetModel(GLuint shader, glm::mat4 baseModel) {
	glm::vec3 red(1.0f, 0.0f, 0.0f);
	glm::vec3 silver(0.8f, 0.8f, 0.9f);

	// 1. 왼쪽 기둥 (위쪽 빨강)
	drawPart(shader, baseModel, glm::vec3(-0.2f, 0.1f, 0.0f), glm::vec3(0.15f, 0.3f, 0.15f), red);
	// 1-2. 왼쪽 팁 (아래쪽 은색)
	drawPart(shader, baseModel, glm::vec3(-0.2f, -0.15f, 0.0f), glm::vec3(0.15f, 0.15f, 0.15f), silver);

	// 2. 오른쪽 기둥 (위쪽 빨강)
	drawPart(shader, baseModel, glm::vec3(0.2f, 0.1f, 0.0f), glm::vec3(0.15f, 0.3f, 0.15f), red);
	// 2-2. 오른쪽 팁 (아래쪽 은색)
	drawPart(shader, baseModel, glm::vec3(0.2f, -0.15f, 0.0f), glm::vec3(0.15f, 0.15f, 0.15f), silver);

	// 3. 위쪽 연결부 (빨강) - U자의 굽은 부분
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.25f, 0.0f), glm::vec3(0.55f, 0.15f, 0.15f), red);
}

//  물약 그리기 
void drawPotionModel(GLuint shader, glm::mat4 baseModel) {
	glm::vec3 purple(0.6f, 0.0f, 0.8f);
	glm::vec3 glassWhite(0.9f, 0.9f, 1.0f); 
	glm::vec3 cork(0.6f, 0.4f, 0.2f); 

	// 1. 물약 액체 (아래쪽 뚱뚱한 부분)
	drawPart(shader, baseModel, glm::vec3(0.0f, -0.15f, 0.0f), glm::vec3(0.4f, 0.35f, 0.4f), purple);

	// 2. 병 목 (가운데 얇은 부분)
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.15f, 0.0f), glm::vec3(0.15f, 0.25f, 0.15f), glassWhite);

	// 3. 병 입구 (목 위쪽 테두리)
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.3f, 0.0f), glm::vec3(0.2f, 0.05f, 0.2f), glassWhite);

	// 4. 코르크 마개
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.35f, 0.0f), glm::vec3(0.12f, 0.1f, 0.12f), cork);
}
//  시계 그리기
void drawClockModel(GLuint shader, glm::mat4 baseModel) {
	glm::vec3 cyan(0.0f, 0.8f, 0.8f);
	glm::vec3 white(1.0f, 1.0f, 1.0f); 
	glm::vec3 black(0.1f, 0.1f, 0.1f); 
	glm::vec3 silver(0.7f, 0.7f, 0.8f); 

	// 1. 시계 몸통 (테두리) - 하늘색 두꺼운 네모
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f, 0.5f, 0.2f), cyan);

	// 2. 시계 판 (흰색 앞면) - 몸통 앞쪽에 얇게 붙임
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.0f, 0.11f), glm::vec3(0.4f, 0.4f, 0.05f), white);

	// 3. 시계 바늘 (검은색) - 시계 판 위에 얇게 붙임
	// 분침 (위로 긴 거, 12시 방향)
	drawPart(shader, baseModel, glm::vec3(0.0f, 0.1f, 0.15f), glm::vec3(0.05f, 0.25f, 0.05f), black);
	// 시침 (오른쪽으로 짧은 거, 3시 방향)
	drawPart(shader, baseModel, glm::vec3(0.1f, 0.0f, 0.15f), glm::vec3(0.2f, 0.05f, 0.05f), black);

	// 4. 자명종 벨 (위에 두 개)
	// 왼쪽 벨 몸통
	drawPart(shader, baseModel, glm::vec3(-0.2f, 0.3f, 0.0f), glm::vec3(0.15f, 0.1f, 0.15f), cyan);
	// 오른쪽 벨 몸통
	drawPart(shader, baseModel, glm::vec3(0.2f, 0.3f, 0.0f), glm::vec3(0.15f, 0.1f, 0.15f), cyan);
	// 왼쪽 벨 꼭다리 (은색 작은 점)
	drawPart(shader, baseModel, glm::vec3(-0.2f, 0.35f, 0.0f), glm::vec3(0.05f, 0.05f, 0.05f), silver);
	// 오른쪽 벨 꼭다리
	drawPart(shader, baseModel, glm::vec3(0.2f, 0.35f, 0.0f), glm::vec3(0.05f, 0.05f, 0.05f), silver);

	// 5. 다리 (아래 두 개) - 은색으로 받침대 표현
	drawPart(shader, baseModel, glm::vec3(-0.15f, -0.3f, 0.0f), glm::vec3(0.08f, 0.1f, 0.08f), silver);
	drawPart(shader, baseModel, glm::vec3(0.15f, -0.3f, 0.0f), glm::vec3(0.08f, 0.1f, 0.08f), silver);
}

// 나무	그리기
void drawTree(int x, int z, GLuint shader) {

	const SeasonColors& colors = seasonThemes[getSeasonByZ(z)];
	glm::mat4 model;
	//glm::mat4 MVP;

	// 1. 나무 기둥
	for (int i = 0; i < 1; ++i) {
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3((float)x, 0.5f + (float)i, (float)z));
		model = glm::scale(model, glm::vec3(0.4f, 1.0f, 0.4f)); 

		//glUniformMatrix4fv(transLoc, 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model)); 

		if (shader == shaderProgramID) {
			glVertexAttrib3f(1, colors.treeTrunk.r, colors.treeTrunk.g, colors.treeTrunk.b);
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 2. 나뭇잎 위로 반듯하게 쌓은 형태
	for (int yOffset = 0; yOffset < 4; ++yOffset) { 
		for (int dx = -1; dx <= 1; ++dx) { 
			for (int dz = -1; dz <= 1; ++dz) { 

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

// 자동차 그리기
void drawCar(const Car& car, GLuint shader) {
	if (car.designID < 0 || car.designID >= carDesigns.size()) return;

	const CarDesign& design = carDesigns[car.designID];
	glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), glm::vec3(car.x, 0.5f, car.z));

	baseModel = glm::scale(baseModel, glm::vec3(design.baseScale));

	for (const auto& part : design.parts) {
		glm::mat4 model = baseModel;
		model = glm::translate(model, part.offset);
		model = glm::scale(model, part.scale);

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

		if (shader == shaderProgramID) {
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

// 철로 그리기
void drawRail(int z, bool isWarning, bool isLightOn, GLuint shader) {

	// 침목 (Sleepers) 
	glm::vec3 woodColor = glm::vec3(0.35f, 0.2f, 0.1f);

	for (float x = -16.0f; x <= 16.0f; x += 1.2f) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(x, 0.01f, (float)z));
		model = glm::scale(model, glm::vec3(0.3f, 0.05f, 1.4f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) glVertexAttrib3f(1, woodColor.r, woodColor.g, woodColor.b);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 철로 (Rails) - 은색
	glm::vec3 railColor = glm::vec3(0.6f, 0.6f, 0.7f);
	float railZPositions[] = { -0.3f, 0.3f }; 

	for (float rz : railZPositions) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, 0.06f, (float)z + rz));
		model = glm::scale(model, glm::vec3(32.0f, 0.08f, 0.1f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) glVertexAttrib3f(1, railColor.r, railColor.g, railColor.b);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 신호등 (Signal) - 오른쪽 배치
	float signalX = 7.0f;

	// 줄무늬 기둥
	for (int i = 0; i < 5; ++i) {
		glm::mat4 model = glm::mat4(1.0f);
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

	// 오른쪽 불빛
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
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(train.x, 0.5f, train.z));
	model = glm::scale(model, glm::vec3(15.0f, 1.8f, 0.9f)); 

	glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
	if (shader == shaderProgramID) glVertexAttrib3f(1, 0.8f, 0.1f, 0.1f);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	// 기차 창문 
	for (float i = -6.0f; i <= 6.0f; i += 2.0f) {
		glm::mat4 winModel = glm::translate(glm::mat4(1.0f), glm::vec3(train.x + i, 0.8f, train.z));
		winModel = glm::scale(winModel, glm::vec3(1.0f, 0.6f, 1.0f)); 
		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(winModel));
		if (shader == shaderProgramID) glVertexAttrib3f(1, 0.2f, 0.2f, 0.3f); 
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

// 통나무 그리기
void drawLog(const Log& logObj, GLuint shader) {
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(logObj.x, 0.45f, logObj.z)); 

	glm::vec3 scale = glm::vec3(logObj.width, 0.3f, 0.8f);

	glm::vec3 whiteColor = glm::vec3(1.0f, 1.0f, 1.0f);

	drawPart(shader, model, glm::vec3(0.0f), scale, whiteColor);
}

// 연잎 그리기
void drawLilyPad(const LilyPad& pad, GLuint shader) {
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(pad.x, 0.41f, pad.z)); 

	glm::vec3 scale = glm::vec3(0.8f, 0.05f, 0.8f);
	glm::vec3 whiteColor = glm::vec3(1.0f, 1.0f, 1.0f);

	drawPart(shader, model, glm::vec3(0.0f), scale, whiteColor);
}

// 아이템 그리기
void drawItem(const Item& item, GLuint shader) {
	if (item.isCollected) return;
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(item.x, 0.5f, item.z));
	model = glm::rotate(model, glm::radians(item.rotation), glm::vec3(0.0f, 1.0f, 0.0f));

	// 아이템 종류에 따라 다른 모양 그리기
	if (item.type == ITEM_SHIELD) {
		model = glm::scale(model, glm::vec3(1.2f));
		drawShieldModel(shader, model);
	}
	else if (item.type == ITEM_MAGNET) {
		model = glm::scale(model, glm::vec3(1.3f));
		drawMagnetModel(shader, model);
	}
	else if (item.type == ITEM_POTION) {
		model = glm::scale(model, glm::vec3(1.2f));
		drawPotionModel(shader, model);
	}
	else if (item.type == ITEM_CLOCK) {
		model = glm::scale(model, glm::vec3(1.2f));
		drawClockModel(shader, model);
	}
}

// 파티클 그리기
void drawParticles(GLuint shader) {
	for (const auto& p : particles) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, p.position);
		model = glm::scale(model, glm::vec3(p.scale)); 

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

		if (shader == shaderProgramID) {
			glVertexAttrib3f(1, p.color.r, p.color.g, p.color.b);
		}
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}
}

void drawCloud(const Cloud& cloud, GLuint shader) {
	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 0; dy++) {
			for (int dz = -1; dz <= 1; dz++) {
				if (rand() % 100 > 60) continue;

				glm::mat4 model = glm::mat4(1.0f);
				glm::vec3 offset = glm::vec3(
					dx * cloud.scale * 0.4f,
					dy * cloud.scale * 0.3f,
					dz * cloud.scale * 0.4f
				);
				model = glm::translate(model, cloud.position);
				model = glm::scale(model, glm::vec3(cloud.scale * 1.5f, cloud.scale * 0.8f, cloud.scale * 1.5f)); 

				glm::vec3 cloudColor = glm::vec3(0.95f, 0.95f, 0.95f); 

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

void renderObjects(GLuint shader, const glm::mat4& pvMatrix)
{
	glBindVertexArray(vao);

	GLint useTexLoc = glGetUniformLocation(shader, "useTexture");
	GLint texLoc = glGetUniformLocation(shader, "targetTexture");
	GLint uvScaleLoc = glGetUniformLocation(shader, "uvScale"); 

	int currentZ = (int)std::round(playerPos.z);
	int drawRangeFront = 30;
	int drawRangeBack = 10;
	float groundWidth = 300.0f;

	if (!isFlying) {
		for (int z = currentZ - drawRangeFront; z <= currentZ + drawRangeBack; ++z) {
			generateLane(z);
			const SeasonColors& colors = seasonThemes[getSeasonByZ(z)];

			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(0.0f, -0.5f, (float)z));

			model = glm::scale(model, glm::vec3(groundWidth, 1.0f, 1.0f));
			glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

			if (shader == shaderProgramID) {

				// CASE 1: 강 
				if (mapType[z] == 2) {
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, riverTexture); 
					glUniform1i(texLoc, 1); 
					glUniform1i(useTexLoc, 1); 

					glUniform2f(uvScaleLoc, groundWidth / 2.0f, 1.0f);

					glVertexAttrib3f(1, 1.0f, 1.0f, 1.0f);
				}
				// CASE 2: 잔디 
				else if (mapType[z] == 0) {
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, grassTexture); 
					glUniform1i(texLoc, 1);
					glUniform1i(useTexLoc, 1); 

					glUniform2f(uvScaleLoc, groundWidth / 2.0f, 1.0f);

					glVertexAttrib3f(1, colors.grass.r, colors.grass.g, colors.grass.b);
				}
				// CASE 3: 도로 또는 철길
				else {
					glUniform1i(useTexLoc, 0);
					glUniform2f(uvScaleLoc, 1.0f, 1.0f);

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
				glUniform1i(useTexLoc, 0); 
				glUniform2f(uvScaleLoc, 1.0f, 1.0f); 
			}

			// 차선 그리기
			if (mapType[z] == 1) {
				glm::vec3 lineScale = glm::vec3(1.0f, 0.02f, 0.15f);

				for (float x = -groundWidth / 2; x <= groundWidth / 2; x += 5.0f) {
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

				if (shader == shaderProgramID) {
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, logTexture); 
					glUniform1i(texLoc, 1); 
					glUniform1i(useTexLoc, 1); 

					glUniform2f(uvScaleLoc, logObj.width, 1.0f);
				}

				drawLog(logObj, shader);

				if (shader == shaderProgramID) {
					glUniform1i(useTexLoc, 0);
					glUniform2f(uvScaleLoc, 1.0f, 1.0f);
				}
			}

			// 연잎
			for (const auto& pad : lilyPads) {
				if (pad.z < currentZ - drawRangeFront || pad.z > currentZ + drawRangeBack) continue;

				if (shader == shaderProgramID) {
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, lilyPadTexture); 
					glUniform1i(texLoc, 1); 
					glUniform1i(useTexLoc, 1); 

					glUniform2f(uvScaleLoc, 1.0f, 1.0f); 
				}

				drawLilyPad(pad, shader);

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
	}

	if (!isGameOver) {
		// 플레이어 (닭)
		glm::mat4 pModel = glm::translate(glm::mat4(1.0f), playerPos);
		pModel = glm::rotate(pModel, glm::radians(playerRotation), glm::vec3(0.0f, 1.0f, 0.0f));
		float scale = isGiantMode ? 3.5f : 0.7f;
		pModel = glm::scale(pModel, glm::vec3(scale));

		if (isFlying) {
			glm::mat4 birdModel = pModel;
			birdModel = glm::translate(birdModel, glm::vec3(0.0f, 0.5f, 0.0f));
			birdModel = glm::scale(birdModel, glm::vec3(1.5f));
			drawBird(shader, birdModel);

			// 닭 그리기 (새 위에 탑승)
			glm::mat4 chickenOnBird = pModel;
			chickenOnBird = glm::translate(chickenOnBird, glm::vec3(0.0f, 1.0f, 0.0f)); 
			drawChicken(shader, chickenOnBird);
		}
		else if (isBirdLeaving) {
			drawChicken(shader, pModel); // 플레이어 그리기
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
	}
}

GLvoid drawScene()
{
	glm::vec3 shakeOffset(0.0f, 0.0f, 0.0f);

	if (shakeTimer > 0.0f) {
		float rx = ((rand() % 100) / 50.0f - 1.0f) * shakeMagnitude;
		float ry = ((rand() % 100) / 50.0f - 1.0f) * shakeMagnitude;
		float rz = ((rand() % 100) / 50.0f - 1.0f) * shakeMagnitude;
		shakeOffset = glm::vec3(rx, ry, rz);
	}

	glm::vec3 cameraTarget = playerPos;
	cameraTarget.y = 0.5f;

	glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.f / 960.f, 0.1f, 100.f);

	glm::mat4 view;
	if (isGiantMode) {
		// 거인 모드
		view = glm::lookAt(cameraTarget + glm::vec3(0, 18, 16) + shakeOffset,
			cameraTarget + shakeOffset,
			glm::vec3(0, 1, 0));
	}
	else if (isFlying || isLanding || isBirdLeaving) {
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
	glm::mat4 lightView = glm::lookAt(lightPos, cameraTarget, glm::vec3(0, 1, 0)); 
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	// 그림자 맵 생성 
	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUseProgram(depthShader);

	glUniformMatrix4fv(glGetUniformLocation(depthShader, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));


	renderObjects(depthShader, lightSpaceMatrix);


	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, 1280, 960);
	if (isFlying) {
		glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
	}
	else {
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	}
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);
	glUniform1i(glGetUniformLocation(shaderProgramID, "isGlitch"), isGlitchMode ? 1 : 0);

	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
	glUniform3fv(glGetUniformLocation(shaderProgramID, "lightPos"), 1, &lightPos[0]);

	int spotlightVal = isNightMode ? 1 : 0;
	glUniform1i(glGetUniformLocation(shaderProgramID, "enableSpotlight"), spotlightVal);
	glUniform3fv(glGetUniformLocation(shaderProgramID, "spotlightPos"), 1, glm::value_ptr(playerPos));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glUniform1i(glGetUniformLocation(shaderProgramID, "shadowMap"), 0);

	renderObjects(shaderProgramID, proj * view);
	if (!isFlying) {
		renderObjects(shaderProgramID, proj * view);

		// 아이템 그리기
		int currentZ = (int)std::round(playerPos.z);
		int drawRangeFront = 30;
		int drawRangeBack = 10;
		for (auto& item : items) {
			if (item.z < currentZ - drawRangeFront || item.z > currentZ + drawRangeBack) continue;
			item.rotation += 2.0f;
			drawItem(item, shaderProgramID);

		}
	}

	int currentZ = (int)std::round(playerPos.z);
	int drawRangeFront = 30;
	int drawRangeBack = 10;

	for (auto& item : items) {
		if (item.z < currentZ - drawRangeFront || item.z > currentZ + drawRangeBack) continue;
		item.rotation += 2.0f;
		drawItem(item, shaderProgramID);
	}

	drawParticles(shaderProgramID);

	if (isFlying || isLanding) {
		drawClouds(shaderProgramID);
	}

	// 날씨 파티클 렌더링 (봄/가을/겨울 통합)
	if (!isFlying) {
		glUseProgram(shaderProgramID);
		for (const auto& wp : weatherParticles) {
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, wp.pos);

			model = glm::scale(model, wp.scaleVec);
			model = glm::rotate(model, wp.swayPhase, glm::vec3(0.5f, 1.0f, 0.2f));

			glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "model"), 1, GL_FALSE, glm::value_ptr(model));

			glVertexAttrib3f(1, wp.color.r, wp.color.g, wp.color.b);

			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
	}

	// UI
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
		if ((int)(nightWarningTimer * 5) % 2 == 0) {
			float centerX = 1280.0f / 2.0f;
			float centerY = 960.0f / 2.0f;

			renderTextTTF(centerX - 200.0f + 3.0f, centerY + 3.0f, "WARNING: BLACKOUT!", 0.0f, 0.0f, 0.0f);
			renderTextTTF(centerX - 200.0f, centerY, "WARNING: BLACKOUT!", 1.0f, 0.0f, 0.0f);
		}
	}
	// 비행 중 텍스트 UI
	if (isFlying) {
		renderTextWithOutline(1280 / 2 - 220, 200, "The bird will take you home...");
		renderTextWithOutline(1280 / 2 - 220, 270, "ENJOY THE SKY!");
	}

	// 무적 상태 표시 (착륙 직후)
	if (isLandingSuccess) {
		renderTextWithOutline(1280 / 2 - 100, 200, "SAFE LANDING!");
	}

	// 6. 스페이스바 연타 이벤트 UI
	if (isEventActive) {
		float barWidth = 600.0f;
		float barHeight = 40.0f;
		float centerX = 1280.0f / 2.0f;
		float centerY = 960.0f - 150.0f;

		// 1. 배경 (테두리)
		glUseProgram(0);
		glDisable(GL_DEPTH_TEST);
		glBegin(GL_QUADS);
		glColor3f(0.1f, 0.1f, 0.1f);
		glVertex2f(centerX - barWidth / 2.0f - 5.0f, centerY - barHeight / 2.0f - 5.0f);
		glVertex2f(centerX + barWidth / 2.0f + 5.0f, centerY - barHeight / 2.0f - 5.0f);
		glVertex2f(centerX + barWidth / 2.0f + 5.0f, centerY + barHeight / 2.0f + 5.0f);
		glVertex2f(centerX - barWidth / 2.0f - 5.0f, centerY + barHeight / 2.0f + 5.0f);
		glEnd();

		// 2. 게이지 채우기
		float fillWidth = barWidth * glm::clamp(eventProgress, 0.0f, 1.0f);
		glBegin(GL_QUADS);
		glColor3f(0.0f, 1.0f, 0.0f);
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

	// 7. 바람 경고 UI
	if (isWindActive) {
		std::string windStr;
		if (windForce > 0) windStr = ">>> STRONG WIND >>>";
		else windStr = "<<< STRONG WIND <<<";

		renderTextWithOutline(1280 / 2 - 150, 800, windStr.c_str());
	}
	if (introState != 0) {
		drawLogo();

		if (introState == 2) {
			static float blinkTimer = 0.0f;
			blinkTimer += 0.05f;
			if (sin(blinkTimer) > 0) {
				renderTextWithOutline(1280 / 2 - 150, 700, "PRESS ANY KEY!");
			}
		}
	}

	if (introState == 0) {
		std::string scoreStr = "SCORE: " + std::to_string(score);
		renderTextWithOutline(20, 60, scoreStr.c_str());
		// 2. 코인 표시
		std::string coinStr = "COINS: " + std::to_string(coinCount);
		renderTextWithOutline(1050, 60, coinStr.c_str());

		if (isLandingSuccess) {
			renderTextWithOutline(1280 / 2 - 100, 200, "SAFE LANDING!");
		}
	}
	if (isGlitchMode) {
		float centerX = 1280.0f / 2.0f;
		float centerY = 960.0f / 2.0f; 
		renderTextWithOutline(centerX - 470.0f, centerY, "SYSTEM ERROR: CONTROLS REVERSED!");
	}

	// 게임 오버 UI 렌더링
	if (isGameOver) {
		float centerX = 1280.0f / 2.0f;
		float centerY = 960.0f / 2.0f;

		renderTextWithOutline(centerX - 150.0f, centerY - 50.0f, "GAME OVER");

		static float blink = 0.0f;
		blink += 0.05f;
		if (sin(blink) > 0) {
			renderTextWithOutline(centerX - 350.0f, centerY + 50.0f, "PRESS ANY KEY TO RESTART");
		}
	}

	glutSwapBuffers();
}

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
	glDisable(GL_TEXTURE_2D); 

	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D); 

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

void renderTextWithOutline(float x, float y, const char* text) {
	float offset = 5.0f; 

	renderTextTTF(x - offset, y, text, 0.0f, 0.0f, 0.0f); // 좌
	renderTextTTF(x + offset, y, text, 0.0f, 0.0f, 0.0f); // 우
	renderTextTTF(x, y - offset, text, 0.0f, 0.0f, 0.0f); // 상
	renderTextTTF(x, y + offset, text, 0.0f, 0.0f, 0.0f); // 하

	renderTextTTF(x - offset, y - offset, text, 0.0f, 0.0f, 0.0f);
	renderTextTTF(x + offset, y - offset, text, 0.0f, 0.0f, 0.0f);
	renderTextTTF(x - offset, y + offset, text, 0.0f, 0.0f, 0.0f);
	renderTextTTF(x + offset, y + offset, text, 0.0f, 0.0f, 0.0f);

	renderTextTTF(x, y, text, 1.0f, 1.0f, 1.0f);
}

void drawLogo() {
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glUseProgram(0); 

	glMatrixMode(GL_PROJECTION);
	glPushMatrix(); 
	glLoadIdentity();
	gluOrtho2D(0, 1280, 0, 960); 

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, logoTexture);

	glColor3f(1.0f, 1.0f, 1.0f);
	float w = 700.0f; 
	float h = 500.0f; 

	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f); glVertex2f(logoX - w / 2, logoY - h / 2); // 좌하
	glTexCoord2f(1.0f, 0.0f); glVertex2f(logoX + w / 2, logoY - h / 2); // 우하
	glTexCoord2f(1.0f, 1.0f); glVertex2f(logoX + w / 2, logoY + h / 2); // 우상
	glTexCoord2f(0.0f, 1.0f); glVertex2f(logoX - w / 2, logoY + h / 2); // 좌상
	glEnd();

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST); 

	glMatrixMode(GL_PROJECTION);
	glPopMatrix(); 
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

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