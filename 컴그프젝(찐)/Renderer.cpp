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
//  물약 그리기 
//  시계 그리기
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

	for (float x = -50.0f; x <= 50.0f; x += 1.2f) {
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
		model = glm::scale(model, glm::vec3(100.0f, 0.08f, 0.1f));

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
	if (item.type == ITEM_REVERSE) {
        const glm::vec3 cyan(0.1f, 0.9f, 0.9f);
        drawPart(shader, model, glm::vec3(0.0f, 0.15f, 0.0f), glm::vec3(0.75f, 0.15f, 0.2f), cyan);
        drawPart(shader, model, glm::vec3(-0.3f, 0.15f, -0.2f), glm::vec3(0.2f, 0.15f, 0.4f), cyan);
        drawPart(shader, model, glm::vec3(0.0f, -0.15f, 0.0f), glm::vec3(0.75f, 0.15f, 0.2f), cyan);
        drawPart(shader, model, glm::vec3(0.3f, -0.15f, 0.2f), glm::vec3(0.2f, 0.15f, 0.4f), cyan);
    }
    else if (item.type == ITEM_BLACKOUT) {
        drawPart(shader, model, glm::vec3(0.0f), glm::vec3(0.75f, 0.75f, 0.75f), glm::vec3(0.4f, 0.1f, 0.7f));
        drawPart(shader, model, glm::vec3(0.0f, 0.0f, 0.4f), glm::vec3(0.45f, 0.45f, 0.08f), glm::vec3(0.05f, 0.02f, 0.1f));
    }
    else if (item.type == ITEM_BANANA) {
        const glm::vec3 yellow(1.0f, 0.85f, 0.05f);
        drawPart(shader, model, glm::vec3(-0.25f, 0.0f, 0.0f), glm::vec3(0.2f, 0.25f, 0.3f), yellow);
        drawPart(shader, model, glm::vec3(0.0f, -0.1f, 0.0f), glm::vec3(0.35f, 0.2f, 0.3f), yellow);
        drawPart(shader, model, glm::vec3(0.3f, 0.0f, 0.0f), glm::vec3(0.2f, 0.25f, 0.3f), yellow);
    }
    else if (item.type == ITEM_SHIELD) {
        model = glm::scale(model, glm::vec3(1.2f));
        drawShieldModel(shader, model);
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

	if (!isGameOver) {
		// 플레이어 (닭)
		glm::mat4 pModel = glm::translate(glm::mat4(1.0f), playerPos);
		pModel = glm::rotate(pModel, glm::radians(playerRotation), glm::vec3(0.0f, 1.0f, 0.0f));
		float scale = 0.7f;
		pModel = glm::scale(pModel, glm::vec3(scale));

		drawChicken(shader, pModel);
	}
}

static void drawShieldAura(const glm::mat4& projection, const glm::mat4& view) {
    const auto* effects = localItems.GetPlayerEffects(1);
    if (isGameOver || !effects || effects->shieldRemainingMs == 0) return;

    GLint previousMatrixMode = GL_MODELVIEW;
    glGetIntegerv(GL_MATRIX_MODE, &previousMatrixMode);
    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_LINE_BIT);
    glUseProgram(0);
    glBindVertexArray(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(glm::value_ptr(projection));
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(glm::value_ptr(view));
    glTranslatef(playerPos.x, playerPos.y + 0.05f, playerPos.z);
    const float spinDegrees = std::fmod(
        static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) * 0.045f, 360.0f);
    glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(spinDegrees, 0.0f, 1.0f, 0.0f);

    constexpr int kSegments = 48;
    constexpr int kLatitudeBands = 16;
    constexpr float kRadius = 0.8f;
    constexpr float kPi = 3.14159265f;
    constexpr float kTau = 2.0f * kPi;

    // Draw a transparent shell without hiding the character inside it.
    for (int band = 0; band < kLatitudeBands; ++band) {
        const float lower = -0.5f * kPi + kPi * band / kLatitudeBands;
        const float upper = -0.5f * kPi + kPi * (band + 1) / kLatitudeBands;
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = kTau * i / kSegments;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const float highlight = std::pow((c + 1.0f) * 0.5f, 8.0f);
            glColor4f(0.18f, 0.7f, 1.0f, 0.11f + 0.12f * highlight);
            glVertex3f(kRadius * std::cos(lower) * c,
                       kRadius * std::sin(lower),
                       kRadius * std::cos(lower) * s);
            glVertex3f(kRadius * std::cos(upper) * c,
                       kRadius * std::sin(upper),
                       kRadius * std::cos(upper) * s);
        }
        glEnd();
    }

    glLineWidth(1.5f);
    glColor4f(0.55f, 0.95f, 1.0f, 0.55f);
    for (int ring = 0; ring < 3; ++ring) {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < kSegments; ++i) {
            const float angle = kTau * static_cast<float>(i) / kSegments;
            const float c = std::cos(angle) * kRadius;
            const float s = std::sin(angle) * kRadius;
            if (ring == 0) glVertex3f(c, 0.0f, s);
            else if (ring == 1) glVertex3f(c, s, 0.0f);
            else glVertex3f(0.0f, s, c);
        }
        glEnd();
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(previousMatrixMode);
    glPopAttrib();
    glBindVertexArray(vao);
    glUseProgram(shaderProgramID);
}

static void drawInventorySlots() {
    const PlayerInventory* inventory = localItems.GetInventory(1);
    if (!inventory) return;

    constexpr int kWindowWidth = 1280;
    constexpr int kWindowHeight = 960;
    constexpr int kSlotSize = 88;
    constexpr int kSlotGap = 12;
    constexpr int kSlotLeft = 20;
    constexpr int kSlotTop = 105;
    constexpr int kBorder = 6;

    // Draw the two HUD boxes in screen coordinates.
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    const GLboolean lightingWasEnabled = glIsEnabled(GL_LIGHTING);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, kWindowWidth, 0, kWindowHeight);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    auto drawQuad = [](float left, float bottom, float right, float top) {
        glBegin(GL_QUADS);
        glVertex2f(left, bottom);
        glVertex2f(right, bottom);
        glVertex2f(right, top);
        glVertex2f(left, top);
        glEnd();
    };

    for (int slot = 0; slot < 2; ++slot) {
        const float left = static_cast<float>(kSlotLeft + slot * (kSlotSize + kSlotGap));
        const float bottom = static_cast<float>(kWindowHeight - kSlotTop - kSlotSize);
        const float right = left + kSlotSize;
        const float top = bottom + kSlotSize;
        const bool selected = inventory->selectedSlot == slot;
        if (selected)
            glColor4f(1.0f, 0.78f, 0.18f, 1.0f);
        else
            glColor4f(0.55f, 0.58f, 0.63f, 1.0f);
        drawQuad(left, bottom, right, top);
        glColor4f(0.05f, 0.07f, 0.10f, 0.82f);
        drawQuad(left + kBorder, bottom + kBorder, right - kBorder, top - kBorder);

        if (inventory->slots[slot] == ItemKind::Empty) {
            glColor4f(0.45f, 0.48f, 0.52f, 1.0f);
            drawQuad(left + 28.0f, bottom + 39.0f,
                     right - 28.0f, bottom + 45.0f);
        }

        if (selected) {
            glColor4f(1.0f, 0.78f, 0.18f, 1.0f);
            glBegin(GL_TRIANGLES);
            glVertex2f(left + 12.0f, top + 10.0f);
            glVertex2f(left + 26.0f, top + 10.0f);
            glVertex2f(left + 19.0f, top + 2.0f);
            glEnd();
        }
    }

    glDisable(GL_BLEND);
    if (lightingWasEnabled) glEnable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    // Render the same 3D models used by world items into each occupied box.
    glUseProgram(shaderProgramID);
    glBindVertexArray(vao);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    const glm::mat4 iconProjection = glm::perspective(glm::radians(35.0f), 1.0f, 0.1f, 10.0f);
    const glm::mat4 iconView = glm::lookAt(glm::vec3(0.0f, 1.5f, 3.1f),
        glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 noShadow = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 4.0f));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projection"), 1, GL_FALSE, glm::value_ptr(iconProjection));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "view"), 1, GL_FALSE, glm::value_ptr(iconView));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(noShadow));
    glUniform3f(glGetUniformLocation(shaderProgramID, "lightPos"), 1.0f, 3.0f, 3.0f);
    glUniform1i(glGetUniformLocation(shaderProgramID, "useTexture"), 0);
    glUniform1i(glGetUniformLocation(shaderProgramID, "enableSpotlight"), 0);
    glUniform1i(glGetUniformLocation(shaderProgramID, "isGlitch"), 0);

    for (int slot = 0; slot < 2; ++slot) {
        if (inventory->slots[slot] == ItemKind::Empty) continue;
        const int left = kSlotLeft + slot * (kSlotSize + kSlotGap) + kBorder;
        const int bottom = kWindowHeight - kSlotTop - kSlotSize + kBorder;
        const int iconSize = kSlotSize - 2 * kBorder;
        glViewport(left, bottom, iconSize, iconSize);
        glScissor(left, bottom, iconSize, iconSize);
        glClear(GL_DEPTH_BUFFER_BIT);
        Item icon{};
        icon.type = inventory->slots[slot] == ItemKind::Shield ? ITEM_SHIELD
            : static_cast<ItemType>(ITEM_REVERSE + static_cast<int>(inventory->slots[slot]) - 1);
        drawItem(icon, shaderProgramID);
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, kWindowWidth, kWindowHeight);

    for (int slot = 0; slot < 2; ++slot) {
        const float labelX = static_cast<float>(
            kSlotLeft + slot * (kSlotSize + kSlotGap) + 30);
        const float labelY = static_cast<float>(kSlotTop + kSlotSize + 8);
        const char* label = slot == 0 ? "1" : "2";

        if (inventory->selectedSlot == slot)
            renderTextTTF(labelX, labelY, label, 1.0f, 0.78f, 0.18f);
        else
            renderTextTTF(labelX, labelY, label, 0.7f, 0.7f, 0.7f);
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

	glm::mat4 view = glm::lookAt(cameraTarget + glm::vec3(2, 12, 10) + shakeOffset,
		cameraTarget + shakeOffset, glm::vec3(0, 1, 0));

	/*glm::mat4 lightProjection = glm::ortho(-50.f, 50.f, -50.f, 50.f, 1.f, 100.f);
	glm::mat4 lightView = glm::lookAt(lightPos, cameraTarget, glm::vec3(0, 1, 0)); 
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;*/

	
	// 그림자 오류 수정 
	const glm::vec3 worldLightPos =
		lightPos + glm::vec3(cameraTarget.x, 0.0f, cameraTarget.z);

	glm::mat4 lightProjection = glm::ortho(-50.f, 50.f, -50.f, 50.f, 1.f, 100.f);
	glm::mat4 lightView = glm::lookAt(worldLightPos, cameraTarget, glm::vec3(0, 1, 0));
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
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);
	const auto* effects = localItems.GetPlayerEffects(1);
    const bool localReverse = effects && effects->reverseControlsRemainingMs > 0;
    const bool localBlackout = effects && effects->blackoutRemainingMs > 0;
    glUniform1i(glGetUniformLocation(shaderProgramID, "isGlitch"), localReverse ? 1 : 0);

	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "view"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "lightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

	glUniform3fv(glGetUniformLocation(shaderProgramID, "lightPos"), 1, glm::value_ptr(worldLightPos));

	int spotlightVal = localBlackout ? 1 : 0;
	glUniform1i(glGetUniformLocation(shaderProgramID, "enableSpotlight"), spotlightVal);
	glUniform3fv(glGetUniformLocation(shaderProgramID, "spotlightPos"), 1, glm::value_ptr(playerPos));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glUniform1i(glGetUniformLocation(shaderProgramID, "shadowMap"), 0);

	renderObjects(shaderProgramID, proj * view);
    drawShieldAura(proj, view);
	// Draw nearby ground items.
	int currentZ = (int)std::round(playerPos.z);
	int drawRangeFront = 30;
	int drawRangeBack = 10;
	for (auto& item : items) {
		if (item.z < currentZ - drawRangeFront || item.z > currentZ + drawRangeBack) continue;
		item.rotation += 2.0f;
		drawItem(item, shaderProgramID);

	}

    for (const auto& trap : localItems.GetBananaTraps()) {
        Item banana{};
        banana.x = (float)trap.second.x;
        banana.z = (float)trap.second.z;
        banana.type = ITEM_BANANA;
        drawItem(banana, shaderProgramID);
    }
    drawParticles(shaderProgramID);

	// 날씨 파티클 렌더링 (봄/가을/겨울 통합)

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

	// UI
	// 1. 점수 표시
	std::string scoreStr = "SCORE: " + std::to_string(score);
	renderTextWithOutline(20, 60, scoreStr.c_str());

	// Item effect status.
	float uiY = 120.0f;

    if (effects && effects->shieldRemainingMs > 0) {
        std::string msg = "[SHIELD] " + std::to_string((effects->shieldRemainingMs + 999) / 1000) + "s";
        renderTextWithOutline(900, uiY, msg.c_str());
        uiY += 40.0f;
    }

	if (isGodMode) {
		renderTextWithOutline(950, uiY, "[GOD MODE]");
		uiY += 40.0f;
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
	}
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

	if (introState == 0 && !isGameOver) {
        drawInventorySlots();
        if (glutGet(GLUT_ELAPSED_TIME) < itemFeedbackUntilMs) {
            float textWidth = 0.0f;
            for (unsigned char ch : itemFeedbackMessage) {
                if (ch >= 32 && ch < 128) textWidth += cdata[ch - 32].xadvance;
            }
            const float scale = std::min(0.5f, 180.0f / std::max(textWidth, 1.0f));
            const char* text = itemFeedbackMessage.c_str();
            renderTextTTF(22, 245, text, 0.0f, 0.0f, 0.0f, scale);
            renderTextTTF(26, 245, text, 0.0f, 0.0f, 0.0f, scale);
            renderTextTTF(24, 243, text, 0.0f, 0.0f, 0.0f, scale);
            renderTextTTF(24, 247, text, 0.0f, 0.0f, 0.0f, scale);
            renderTextTTF(24, 245, text, 1.0f, 1.0f, 1.0f, scale);
        }
    }

	glutSwapBuffers();
}

void renderTextTTF(float x, float y, const char* text, float r, float g, float b, float scale) {
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
	float penX = 0.0f;
	float penY = 0.0f;

	while (*text) {
		if (*text >= 32 && *text < 128) {
			stbtt_aligned_quad q;
			stbtt_GetBakedQuad(cdata, 512, 512, *text - 32, &penX, &penY, &q, 1);

			const float left = x + q.x0 * scale;
			const float right = x + q.x1 * scale;
			const float top = 960.0f - (y + q.y0 * scale);
			const float bottom = 960.0f - (y + q.y1 * scale);
			glTexCoord2f(q.s0, q.t0); glVertex2f(left, top);
			glTexCoord2f(q.s1, q.t0); glVertex2f(right, top);
			glTexCoord2f(q.s1, q.t1); glVertex2f(right, bottom);
			glTexCoord2f(q.s0, q.t1); glVertex2f(left, bottom);
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