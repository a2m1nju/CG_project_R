#include "Renderer.h"
#include "Global.h"
#include "Logic.h" 
#include "ClientNetwork.h"
#include "Input.h"
#include "CharacterModels.h"
#include "GameViewport.h"
#include "Texture.h"
#include "BananaMesh.h"

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


// 기존 매 모델에 날갯짓을 더해 화면 추격 사망 연출에만 사용한다.
static void drawBird(GLuint shader, const glm::mat4& baseModel, float seconds) {
    const glm::vec3 brown(0.4f, 0.2f, 0.0f);
    const glm::vec3 white(0.9f, 0.9f, 0.9f);
    const glm::vec3 yellow(1.0f, 0.8f, 0.0f);
    drawPart(shader, baseModel, glm::vec3(0.0f), glm::vec3(1.0f, 0.5f, 0.6f), brown);
    drawPart(shader, baseModel, glm::vec3(0.5f, 0.15f, 0.0f), glm::vec3(0.4f), white);
    drawPart(shader, baseModel, glm::vec3(0.7f, 0.15f, 0.0f), glm::vec3(0.2f, 0.1f, 0.1f), yellow);
    drawPart(shader, baseModel, glm::vec3(-0.55f, 0.0f, 0.0f), glm::vec3(0.3f, 0.2f, 0.6f), brown);

    const float flap = std::sin(seconds * 19.0f) * 28.0f;
    for (int side : { -1, 1 }) {
        glm::mat4 wing = glm::translate(baseModel, glm::vec3(0.0f, 0.12f, side * 0.35f));
        wing = glm::rotate(wing, glm::radians(side * flap), glm::vec3(1.0f, 0.0f, 0.0f));
        drawPart(shader, wing, glm::vec3(0.0f, 0.0f, side * 0.62f),
                 glm::vec3(0.15f, 0.1f, 1.2f), brown);
        drawPart(shader, baseModel, glm::vec3(0.15f, -0.32f, side * 0.28f),
                 glm::vec3(0.08f, 0.36f, 0.08f), yellow);
    }
}

static float birdFlightProgress(const BirdCarryAnimation& animation) {
    return glm::clamp((animation.elapsedSeconds - 0.85f) /
                      (BirdCarryDurationSeconds - 0.85f), 0.0f, 1.0f);
}

static void drawBirdCarry(GLuint shader, const BirdCarryAnimation& animation, CharacterId characterId, AccessoryLoadout accessories) {
    if (!animation.active) return;
    const float seconds = glm::clamp(animation.elapsedSeconds, 0.0f,
                                     BirdCarryDurationSeconds);
    const float approach = glm::clamp(seconds / 0.55f, 0.0f, 1.0f);
    const float pickup = glm::clamp((seconds - 0.55f) / 0.30f, 0.0f, 1.0f);
    const float lift = birdFlightProgress(animation);
    // 이 게임의 진행 방향은 Z축 음의 방향이다.
    const glm::vec3 birdPosition = animation.origin + glm::vec3(
        0.0f, 4.8f - 3.8f * approach + 5.5f * lift,
        3.0f * (1.0f - approach) - 12.0f * lift);
    const glm::vec3 chickenPosition = animation.origin + glm::vec3(
        0.0f, 0.6f * pickup + 5.5f * lift, -12.0f * lift);

    glm::mat4 birdModel = glm::translate(glm::mat4(1.0f), birdPosition);
    birdModel = glm::rotate(birdModel, glm::radians(90.0f),
                            glm::vec3(0.0f, 1.0f, 0.0f));
    birdModel = glm::scale(birdModel, glm::vec3(1.35f));
    drawBird(shader, birdModel, seconds);
    glm::mat4 chickenModel = glm::translate(glm::mat4(1.0f), chickenPosition);
    chickenModel = glm::scale(chickenModel, glm::vec3(0.7f));
    drawCharacter(shader, chickenModel, characterId, accessories.head, accessories.back, accessories.face);
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

	// 1. 나무 기둥
	for (int i = 0; i < 1; ++i) {
		model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3((float)x, 0.5f + (float)i, (float)z));
		model = glm::scale(model, glm::vec3(0.4f, 1.0f, 0.4f)); 

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

	// 침목 
	glm::vec3 woodColor = glm::vec3(0.35f, 0.2f, 0.1f);

	for (float x = -50.0f; x <= 50.0f; x += 1.2f) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(x, 0.01f, (float)z));
		model = glm::scale(model, glm::vec3(0.3f, 0.05f, 1.4f));

		glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));
		if (shader == shaderProgramID) glVertexAttrib3f(1, woodColor.r, woodColor.g, woodColor.b);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	// 철로 - 은색
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

	// 신호등 - 오른쪽 배치
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
	model = glm::translate(model, glm::vec3(logObj.x, 0.05f, logObj.z));

	glm::vec3 scale = glm::vec3(logObj.width, 0.3f, 0.8f);

	glm::vec3 whiteColor = glm::vec3(1.0f, 1.0f, 1.0f);

	drawPart(shader, model, glm::vec3(0.0f), scale, whiteColor);
}

// 연잎 그리기
void drawLilyPad(const LilyPad& pad, GLuint shader)
{
	glm::mat4 model = glm::mat4(1.0f);

	model = glm::translate(model,glm::vec3(pad.x, 0.05f, pad.z));

	glm::vec3 scale(0.8f, 0.05f, 0.8f);
	glm::vec3 whiteColor(1.0f, 1.0f, 1.0f);

	drawPart(
		shader,
		model,
		glm::vec3(0.0f),
		scale,
		whiteColor);
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
        BananaMesh::Draw(shader, model);
    }
    else if (item.type == ITEM_SHIELD) {
        model = glm::scale(model, glm::vec3(1.2f));
        drawShieldModel(shader, model);
    }
    else if (item.type == ITEM_DASH_MUSHROOM) {
        drawPart(shader, model, glm::vec3(0.0f, -0.18f, 0.0f),
            glm::vec3(0.22f, 0.36f, 0.22f), glm::vec3(0.94f, 0.88f, 0.68f));
        drawPart(shader, model, glm::vec3(0.0f, 0.12f, 0.0f),
            glm::vec3(0.72f, 0.34f, 0.72f), glm::vec3(0.92f, 0.15f, 0.12f));
        drawPart(shader, model, glm::vec3(0.0f, 0.30f, 0.0f),
            glm::vec3(0.24f, 0.03f, 0.24f), glm::vec3(1.0f));
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

static glm::vec3 RemotePlayerPosition(const RemotePlayerView& player) {
    const float moveSeconds = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - player.moveStarted).count();
    const float visibleMoveDuration = MOVE_DURATION * 0.5f;
    if (moveSeconds >= 0.0f && moveSeconds < visibleMoveDuration) {
        const float t = glm::clamp(moveSeconds / visibleMoveDuration, 0.0f, 1.0f);
        return glm::vec3(
            glm::mix(player.moveStartX, player.x, t),
            0.5f + JUMP_HEIGHT * 4.0f * t * (1.0f - t),
            glm::mix(player.moveStartZ, player.z, t));
    }
    return glm::vec3(player.x, 0.5f, player.z);
}


struct OffscreenPlayerMarker {
    CharacterId character;
    AccessoryLoadout accessories;
    glm::vec2 circle;
    glm::vec2 direction;
};

static void beginIndicator2D(GLint& previousMatrixMode) {
    glGetIntegerv(GL_MATRIX_MODE, &previousMatrixMode);
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 1280, 0, 960);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
}

static void endIndicator2D(GLint previousMatrixMode) {
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(previousMatrixMode);
}

static void drawOffscreenPlayerIndicators(const glm::mat4& projection,
                                          const glm::mat4& view,
                                          const GameViewport& viewport) {
    if (!activeClientNetwork || isGameOver || localBirdCarry.active) return;

    constexpr float markerSize = 180.0f;
    constexpr float edgeInset = 105.0f;
    std::vector<OffscreenPlayerMarker> markers;
    for (const auto& player : activeClientNetwork->GetPlayers()) {
        if (player.id == activeClientNetwork->GetMyPlayerId() || !player.alive)
            continue;

        const glm::vec3 position = RemotePlayerPosition(player);
        const glm::vec4 clip = projection * view *
            glm::vec4(position + glm::vec3(0.0f, 0.20f, 0.0f), 1.0f);
        if (clip.w > 0.0f &&
            std::abs(clip.x) <= clip.w * 1.05f &&
            std::abs(clip.y) <= clip.w * 1.05f)
            continue;

        glm::vec2 direction(clip.x, clip.y);
        if (glm::length(direction) < 0.0001f)
            direction = glm::vec2(0.0f, -1.0f);
        else
            direction = glm::normalize(direction);
        const float distanceX = std::abs(direction.x) > 0.0001f
            ? (640.0f - edgeInset) / std::abs(direction.x) : 1000000.0f;
        const float distanceY = std::abs(direction.y) > 0.0001f
            ? (480.0f - edgeInset) / std::abs(direction.y) : 1000000.0f;
        const float distance = (std::min)(distanceX, distanceY);
        markers.push_back({player.characterId, player.accessories,
            glm::vec2(640.0f, 480.0f) + direction * distance, direction});
    }
    if (markers.empty()) return;

    if (markers.size() == 2 &&
        glm::length(markers[0].circle - markers[1].circle) < 120.0f) {
        const glm::vec2 tangent(-markers[0].direction.y,
                                 markers[0].direction.x);
        markers[0].circle = glm::clamp(markers[0].circle - tangent * 55.0f,
            glm::vec2(edgeInset), glm::vec2(1280.0f - edgeInset, 960.0f - edgeInset));
        markers[1].circle = glm::clamp(markers[1].circle + tangent * 55.0f,
            glm::vec2(edgeInset), glm::vec2(1280.0f - edgeInset, 960.0f - edgeInset));
    }

    glPushAttrib(GL_CURRENT_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT |
        GL_DEPTH_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT);
    static GLuint indicatorTexture = 0;
    if (indicatorTexture == 0) {
        loadTexture("assets/offscreen_player_indicator.png", &indicatorTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // 캐릭터를 불투명한 초상화로 그린 뒤 원형으로 잘라 표시한다.
    static GLuint portraitFramebuffer = 0;
    static GLuint portraitTexture = 0;
    static GLuint portraitDepth = 0;
    static bool portraitReady = false;
    GLint previousFramebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glActiveTexture(GL_TEXTURE0);

    if (portraitFramebuffer == 0) {
        glGenFramebuffers(1, &portraitFramebuffer);
        glGenTextures(1, &portraitTexture);
        glBindTexture(GL_TEXTURE_2D, portraitTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenRenderbuffers(1, &portraitDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, portraitDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 128, 128);
        glBindFramebuffer(GL_FRAMEBUFFER, portraitFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, portraitTexture, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, portraitDepth);
        portraitReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
    }
    if (!portraitReady) {
        glPopAttrib();
        return;
    }

    const glm::mat4 portraitProjection =
        glm::perspective(glm::radians(38.0f), 1.0f, 0.1f, 10.0f);
    const glm::mat4 portraitView = glm::lookAt(
        glm::vec3(0.55f, 0.70f, -2.5f), glm::vec3(0.0f, 0.10f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 noShadow = glm::translate(glm::mat4(1.0f),
        glm::vec3(0.0f, 0.0f, 4.0f));
    const float portraitRadius = markerSize * 74.0f / 512.0f;
    GLint previousMatrixMode = GL_MODELVIEW;
    glUseProgram(shaderProgramID);
    glBindVertexArray(vao);
    glDisableVertexAttribArray(1);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projection"),
        1, GL_FALSE, glm::value_ptr(portraitProjection));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "view"),
        1, GL_FALSE, glm::value_ptr(portraitView));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "lightSpaceMatrix"),
        1, GL_FALSE, glm::value_ptr(noShadow));
    glUniform3f(glGetUniformLocation(shaderProgramID, "lightPos"),
        1.0f, 3.0f, -3.0f);
    glUniform1i(glGetUniformLocation(shaderProgramID, "useTexture"), 0);
    glUniform1i(glGetUniformLocation(shaderProgramID, "enableSpotlight"), 0);
    glUniform1i(glGetUniformLocation(shaderProgramID, "isGlitch"), 0);
    for (const auto& marker : markers) {
        glBindFramebuffer(GL_FRAMEBUFFER, portraitFramebuffer);
        glViewport(0, 0, 128, 128);
        glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_TRUE);
        glClearColor(0.04f, 0.08f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgramID);
        glBindVertexArray(vao);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        drawCharacter(shaderProgramID, glm::mat4(1.0f), marker.character,
            marker.accessories.head, marker.accessories.back,
            marker.accessories.face);

        glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
        beginIndicator2D(previousMatrixMode);
        glBindTexture(GL_TEXTURE_2D, portraitTexture);
        glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0.5f, 0.5f);
        glVertex2f(marker.circle.x, marker.circle.y);
        for (int i = 0; i <= 48; ++i) {
            const float angle = glm::radians(i * 360.0f / 48.0f);
            const float x = std::cos(angle);
            const float y = std::sin(angle);
            glTexCoord2f(0.5f + 0.5f * x, 0.5f + 0.5f * y);
            glVertex2f(marker.circle.x + portraitRadius * x,
                       marker.circle.y + portraitRadius * y);
        }
        glEnd();
        endIndicator2D(previousMatrixMode);
    }

    beginIndicator2D(previousMatrixMode);
    glBindTexture(GL_TEXTURE_2D, indicatorTexture);
    const float halfSize = markerSize * 0.5f;
    for (const auto& marker : markers) {
        const glm::vec2 imageCenter = marker.circle +
            marker.direction * (markerSize * 74.0f / 512.0f);
        const float angle = glm::degrees(
            std::atan2(-marker.direction.x, marker.direction.y));
        glPushMatrix();
        glTranslatef(imageCenter.x, imageCenter.y, 0.0f);
        glRotatef(angle, 0.0f, 0.0f, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-halfSize, -halfSize);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( halfSize, -halfSize);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( halfSize,  halfSize);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-halfSize,  halfSize);
        glEnd();
        glPopMatrix();
    }
    endIndicator2D(previousMatrixMode);
    glPopAttrib();
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
			if (!activeClientNetwork) {
				generateLane(z);
			}
			else if (mapType.find(z) == mapType.end()) {
				continue;
			}
			const SeasonColors& colors = seasonThemes[getSeasonByZ(z)];

			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, glm::vec3(0.0f, -0.5f, (float)z));

			model = glm::scale(model, glm::vec3(groundWidth, 1.0f, 1.0f));
			glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, glm::value_ptr(model));

			if (shader == shaderProgramID) {

				// 경우 1: 강
				if (mapType[z] == 2) {
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, riverTexture); 
					glUniform1i(texLoc, 1); 
					glUniform1i(useTexLoc, 1); 

					glUniform2f(uvScaleLoc, groundWidth / 2.0f, 1.0f);

					glVertexAttrib3f(1, 1.0f, 1.0f, 1.0f);
				}
				// 경우 2: 잔디
				else if (mapType[z] == 0) {
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, grassTexture); 
					glUniform1i(texLoc, 1);
					glUniform1i(useTexLoc, 1); 

					glUniform2f(uvScaleLoc, groundWidth / 2.0f, 1.0f);

					glVertexAttrib3f(1, colors.grass.r, colors.grass.g, colors.grass.b);
				}
				// 경우 3: 도로 또는 철길
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

    if (shader == shaderProgramID) glUniform1i(useTexLoc, 0);

    const CharacterId myCharacter = activeClientNetwork
        ? activeClientNetwork->GetMyCharacterId() : CharacterId::Chicken;
    const AccessoryLoadout myAccessories = activeClientNetwork
        ? activeClientNetwork->GetMyAccessories() : AccessoryLoadout{};
    if (localBirdCarry.active) {
        drawBirdCarry(shader, localBirdCarry, myCharacter, myAccessories);
    } else if (!isGameOver) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), playerPos);
        model = glm::rotate(model, glm::radians(playerRotation),
                            glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(0.7f));
        drawCharacter(shader, model, myCharacter, myAccessories.head, myAccessories.back, myAccessories.face);
    }

    if (activeClientNetwork) {
        for (const auto& player : activeClientNetwork->GetPlayers()) {
            if (player.id == activeClientNetwork->GetMyPlayerId() || !player.alive)
                continue;
            glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                             RemotePlayerPosition(player));
            model = glm::rotate(model, glm::radians(player.rotation),
                                glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.7f));
            drawCharacter(shader, model, player.characterId, player.accessories.head, player.accessories.back, player.accessories.face);
        }
    }
}

static void drawShieldAuraAt(const glm::mat4& projection, const glm::mat4& view, const glm::vec3& position) {

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
    glTranslatef(position.x, position.y + 0.05f, position.z);
    const float spinDegrees = std::fmod(
        static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) * 0.045f, 360.0f);
    glRotatef(20.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(spinDegrees, 0.0f, 1.0f, 0.0f);

    constexpr int kSegments = 48;
    constexpr int kLatitudeBands = 16;
    constexpr float kRadius = 0.8f;
    constexpr float kPi = 3.14159265f;
    constexpr float kTau = 2.0f * kPi;

    // 안쪽 캐릭터가 보이도록 투명한 보호막을 그린다.
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

    const GameViewport viewport = FitGameViewport(
        glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
    if (viewport.width == 0 || viewport.height == 0) return;
    constexpr int kWindowWidth = 1280;
    constexpr int kWindowHeight = 960;
    constexpr int kSlotSize = 88;
    constexpr int kSlotGap = 12;
    constexpr int kSlotLeft = 20;
    constexpr int kSlotTop = 105;
    constexpr int kBorder = 6;

    // 화면 좌표를 기준으로 두 개의 아이템 칸을 그린다.
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

    // 차 있는 칸에는 월드 아이템과 같은 3D 모델을 그린다.
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
        const int iconX = viewport.pixelX(static_cast<float>(left));
        const int iconY = viewport.pixelY(static_cast<float>(bottom));
        const int iconPixels = (std::max)(1, viewport.pixels(static_cast<float>(iconSize)));
        glViewport(iconX, iconY, iconPixels, iconPixels);
        glScissor(iconX, iconY, iconPixels, iconPixels);
        glClear(GL_DEPTH_BUFFER_BIT);
        Item icon{};
        icon.type = inventory->slots[slot] == ItemKind::Shield ? ITEM_SHIELD
            : inventory->slots[slot] == ItemKind::DashMushroom ? ITEM_DASH_MUSHROOM
            : static_cast<ItemType>(ITEM_REVERSE + static_cast<int>(inventory->slots[slot]) - 1);
        drawItem(icon, shaderProgramID);
    }

    glDisable(GL_SCISSOR_TEST);
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

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

static void drawMenuText(float centerX, float y, const char* text, float scale = 1.0f)
{
	float width = 0.0f;

	for (const char* p = text; *p; ++p) {
		const unsigned char ch =
			static_cast<unsigned char>(*p);

		if (ch >= 32 && ch < 128) {
			width += cdata[ch - 32].xadvance;
		}
	}

	renderTextTTF(
		centerX - width * scale * 0.5f,
		y, text,
		1.0f, 1.0f, 1.0f, scale);
}

static void drawMenuButton(float x, float y, float width, float height, const char* text)
{
	glUseProgram(0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_TEXTURE_2D);

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, 1280, 0, 960);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	// OpenGL 사각형 좌표는 아래쪽이 원점이므로 변환
	const float top = 960.0f - y;
	const float bottom = 960.0f - y - height;

	glColor3f(0.15f, 0.42f, 0.65f);

	glBegin(GL_QUADS);
	glVertex2f(x, bottom);
	glVertex2f(x + width, bottom);
	glVertex2f(x + width, top);
	glVertex2f(x, top);
	glEnd();

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	drawMenuText(
		x + width * 0.5f,
		y + height * 0.5f + 15.0f,
		text, 0.7f);
}

static void drawMenuScreen()
{
	const int windowWidth = glutGet(GLUT_WINDOW_WIDTH);
	const int windowHeight = glutGet(GLUT_WINDOW_HEIGHT);

	const LobbyView* lobby = activeClientNetwork ? &activeClientNetwork->GetLobby() : nullptr;

	if (windowWidth <= 0 || windowHeight <= 0) {
		return;
	}
	const GameViewport viewport = FitGameViewport(windowWidth, windowHeight);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_SCISSOR_TEST);
	glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

	glDepthMask(GL_TRUE);
	glClearColor(0.08f, 0.12f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// 타이틀 화면
	if (clientScreen == ClientScreen::Title) {
		glUseProgram(0);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glActiveTexture(GL_TEXTURE0);

		drawLogo();

		if (introState == 2) {
			drawMenuButton(
				460.0f, 680.0f,
				360.0f, 90.0f,
				"GAME START");
		}
		if (lobby) {
			drawMenuText(
				640.0f, 920.0f,
				lobby->message.c_str(), 0.35f);
		}
		else {
			drawMenuText(
				640.0f, 920.0f,
				"START SERVER AND RUN WITH SERVER IP",
				0.35f);
		}
		return;
	}
	if (!lobby) {
		drawMenuText(
			640.0f, 480.0f,
			"NOT CONNECTED", 0.7f);
		return;
	}

	if (clientScreen == ClientScreen::Matching) {
		drawMenuText(640.0f, 300.0f, "MATCHING");

		const std::string countText =
			std::to_string(lobby->count) + " / 3";

		drawMenuText(
			640.0f, 450.0f,
			countText.c_str());

		drawMenuText(
			640.0f, 550.0f,
			lobby->message.c_str(), 0.45f);

		drawMenuButton(
			460.0f, 780.0f,
			360.0f, 90.0f,
			lobby->leaving ? "LEAVING..." : "BACK");

		return;
	}

	// 대기방의 닭 모델을 그릴 준비
	glUseProgram(shaderProgramID);
	glBindVertexArray(vao);

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	// 모델 부위별 색상을 drawCharacter()에서 지정
	glDisableVertexAttribArray(1);

	const glm::mat4 projection = glm::perspective(
		glm::radians(35.0f),
		260.0f / 300.0f,
		0.1f, 20.0f);

	// 닭 부리는 -Z 방향이므로 앞쪽에서 바라봄
	const glm::mat4 view = glm::lookAt(
		glm::vec3(1.2f, 0.9f, -3.0f),
		glm::vec3(0.0f, -0.05f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f));

	// 기존 인벤토리 모델과 같은 그림자 제외 방식
	const glm::mat4 noShadow = glm::translate(
		glm::mat4(1.0f),
		glm::vec3(0.0f, 0.0f, 4.0f));

	glUniformMatrix4fv(
		glGetUniformLocation(shaderProgramID, "projection"),
		1, GL_FALSE, glm::value_ptr(projection));

	glUniformMatrix4fv(
		glGetUniformLocation(shaderProgramID, "view"),
		1, GL_FALSE, glm::value_ptr(view));

	glUniformMatrix4fv(
		glGetUniformLocation(shaderProgramID, "lightSpaceMatrix"),
		1, GL_FALSE, glm::value_ptr(noShadow));

	glUniform1i(
		glGetUniformLocation(shaderProgramID, "useTexture"), 0);

	glUniform1i(
		glGetUniformLocation(shaderProgramID, "enableSpotlight"), 0);

	glUniform1i(
		glGetUniformLocation(shaderProgramID, "isGlitch"), 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthMap);

	glUniform1i(
		glGetUniformLocation(shaderProgramID, "shadowMap"), 0);

	// 화면의 세 구역에 같은 닭 모델을 각각 그리기
	for (int i = 0;
		i < static_cast<int>(lobby->members.size()) && i < 3;
		++i){
		const int centerX = 320 + i * 320;

		glViewport(
			viewport.pixelX(static_cast<float>(centerX - 130)),
			viewport.pixelY(360.0f),
			(std::max)(1, viewport.pixels(260.0f)),
			(std::max)(1, viewport.pixels(300.0f)));

		glm::mat4 previewModel(1.0f);
		if (lobby->members[i].id == activeClientNetwork->GetMyPlayerId())
			previewModel = glm::rotate(previewModel, glm::radians(GetLobbyPreviewAngle()),
				glm::vec3(0.0f, 1.0f, 0.0f));
		drawCharacter(shaderProgramID, previewModel, lobby->members[i].characterId,
            lobby->members[i].accessories.head, lobby->members[i].accessories.back,
            lobby->members[i].accessories.face);
	}

	// 글자는 창 전체를 기준으로 표시
	glViewport(viewport.x, viewport.y, viewport.width, viewport.height);

	drawMenuText(640.0f, 180.0f, "LOBBY");

	drawMenuText(
		640.0f, 250.0f,
		lobby->message.c_str(), 0.4f);

	for (int i = 0;
		i < static_cast<int>(lobby->members.size()) && i < 3;
		++i)
	{
		const auto& member = lobby->members[i];
		std::string label = std::to_string(i + 1) + "P";
		if (member.id == activeClientNetwork->GetMyPlayerId()) {
			label += " (YOU)";
		}
		drawMenuText(
			320.0f + i * 320.0f,
			660.0f,
			label.c_str(), 0.75f);

		drawMenuText(
			320.0f + i * 320.0f,
			700.0f,
			member.ready ? "READY" : "NOT READY",
			0.45f);
		const char* names[] = { "CHICKEN", "RABBIT", "CAT", "DOG", "BEAR", "PANDA", "TINO", "MONOKUMA", "KORILAKKUMA", "KUROMI", "SANS" };
		const unsigned id = static_cast<unsigned>(member.characterId);
		drawMenuText(320.0f + i * 320.0f, 730.0f,
			names[id < CharacterCount ? id : 0], 0.45f);
		if (member.id == activeClientNetwork->GetMyPlayerId()) {
			drawMenuText(320.0f + i * 320.0f, 595.0f, "DRAG TO ROTATE", 0.32f);
			const char* headNames[] = { "HEAD: NONE", "HEAD: RIBBON", "HEAD: CROWN",
				"HEAD: HAT", "HEAD: HALO", "HEAD: HORNS", "HEAD: FLOWER" };
			const char* backNames[] = { "BACK: NONE", "BACK: BACKPACK",
				"BACK: ANGEL WINGS", "BACK: DEVIL WINGS" };
			const char* faceNames[] = { "FACE: NONE", "FACE: GLASSES", "FACE: SUNGLASSES" };
			const auto head = static_cast<unsigned>(member.accessories.head);
			const auto back = static_cast<unsigned>(member.accessories.back);
			const auto face = static_cast<unsigned>(member.accessories.face);
			const float centerX = 320.0f + i * 320.0f;
			drawMenuText(centerX, 760.0f, headNames[head < HeadAccessoryCount ? head : 0], 0.38f);
			drawMenuText(centerX, 795.0f, backNames[back < BackAccessoryCount ? back : 0], 0.38f);
			drawMenuText(centerX, 830.0f, faceNames[face < FaceAccessoryCount ? face : 0], 0.38f);
			if (lobby->stage == LobbyStage::Ready && !member.ready &&
				!lobby->readyPending && !lobby->leaving &&
				!activeClientNetwork->IsCharacterSelectionPending()) {
				drawMenuButton(centerX - 125.0f, 740.0f, 34.0f, 38.0f, "<");
				drawMenuButton(centerX + 91.0f, 740.0f, 34.0f, 38.0f, ">");
				drawMenuButton(centerX - 125.0f, 775.0f, 34.0f, 38.0f, "<");
				drawMenuButton(centerX + 91.0f, 775.0f, 34.0f, 38.0f, ">");
				if (member.characterId != CharacterId::Chicken) {
					drawMenuButton(centerX - 125.0f, 810.0f, 34.0f, 38.0f, "<");
					drawMenuButton(centerX + 91.0f, 810.0f, 34.0f, 38.0f, ">");
				}
			}
		}
		if (member.id == activeClientNetwork->GetMyPlayerId() &&
			lobby->stage == LobbyStage::Ready && !member.ready &&
			!lobby->readyPending && !lobby->leaving &&
			!activeClientNetwork->IsCharacterSelectionPending()) {
			const float centerX = 320.0f + i * 320.0f;
			drawMenuButton(centerX - 195.0f, 485.0f, 55.0f, 70.0f, "<");
			drawMenuButton(centerX + 140.0f, 485.0f, 55.0f, 70.0f, ">");
		}
	}
	if (lobby->stage == LobbyStage::Loading) {
		return;
	}
	const char* readyLabel = activeClientNetwork->IsMyLobbyReady() ? "CANCEL READY" : "READY";

	if (lobby->readyPending || activeClientNetwork->IsCharacterSelectionPending()) {
		readyLabel = "WAITING...";
	}

	if (lobby->leaving) {
		readyLabel = "LEAVING...";
	}
	drawMenuButton(460.0f, 865.0f, 360.0f, 80.0f, readyLabel);

	drawMenuButton(1000.0f, 875.0f, 200.0f, 60.0f, "BACK");
}




GLvoid drawScene()
{
	const GameViewport viewport = FitGameViewport(
		glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
	if (viewport.width == 0 || viewport.height == 0) return;
	// 타이틀과 대기방에서는 게임 월드를 그리지 않음
	if (clientScreen != ClientScreen::Playing) {
		drawMenuScreen();
		glutSwapBuffers();
		return;
	}
	glm::vec3 shakeOffset(0.0f, 0.0f, 0.0f);

	if (shakeTimer > 0.0f) {
		float rx = ((rand() % 100) / 50.0f - 1.0f) * shakeMagnitude;
		float ry = ((rand() % 100) / 50.0f - 1.0f) * shakeMagnitude;
		float rz = ((rand() % 100) / 50.0f - 1.0f) * shakeMagnitude;
		shakeOffset = glm::vec3(rx, ry, rz);
	}

	glm::vec3 cameraTarget = playerPos;
	if (localBirdCarry.active) {
		const float flight = birdFlightProgress(localBirdCarry);
		cameraTarget = localBirdCarry.origin +
			glm::vec3(0.0f, 2.0f * flight, -7.0f * flight);
		if (activeClientNetwork && activeClientNetwork->ChaseActive()) {
			const float blend = glm::smoothstep(
				0.0f, 0.45f, localBirdCarry.elapsedSeconds);
			cameraTarget.z = glm::mix(
				activeClientNetwork->ChaseFrontZ(), cameraTarget.z, blend);
		}
	}
	if (!localBirdCarry.active && activeClientNetwork && activeClientNetwork->ChaseActive()) {
		cameraTarget.z = (std::min)(cameraTarget.z, activeClientNetwork->ChaseFrontZ());
	}
	if (!localBirdCarry.active) cameraTarget.y = 0.5f;

	glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.f / 960.f, 0.1f, 100.f);

	glm::mat4 view = glm::lookAt(cameraTarget + glm::vec3(2, 12, 10) + shakeOffset,
		cameraTarget + shakeOffset, glm::vec3(0, 1, 0));


	
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

	glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
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
    const auto* localShield = localItems.GetPlayerEffects(1);
    if (!isGameOver && localShield && localShield->shieldRemainingMs > 0) {
        drawShieldAuraAt(proj, view, playerPos);
    }
    if (activeClientNetwork) {
        for (const auto& player : activeClientNetwork->GetPlayers()) {
            if (player.id != activeClientNetwork->GetMyPlayerId() &&
                player.alive && player.shieldRemainingMs > 0) {
                drawShieldAuraAt(proj, view, RemotePlayerPosition(player));
            }
        }
    }
	// 주변 바닥 아이템을 그린다.
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

	// 1. 점수 표시
	std::string scoreStr = "SCORE: " + std::to_string(score);
	renderTextWithOutline(20, 60, scoreStr.c_str());

	// 아이템 효과 상태를 표시한다.
	float uiY = 60.0f;
    constexpr float statusTextScale = 0.58f;
    constexpr float statusLineHeight = 44.0f;

    if (effects && effects->shieldRemainingMs > 0) {
        std::string msg = "[SHIELD] " + std::to_string((effects->shieldRemainingMs + 999) / 1000) + "s";
        renderTextWithOutline(1040, uiY, msg.c_str(), statusTextScale);
        uiY += statusLineHeight;
    }
    if (effects && effects->boostRemainingMs > 0) {
        std::string msg = "[DASH] " + std::to_string((effects->boostRemainingMs + 999) / 1000) + "s";
        renderTextWithOutline(1040, uiY, msg.c_str(), statusTextScale);
        uiY += statusLineHeight;
    }

	if (isGodMode) {
		renderTextWithOutline(1040, uiY, "[GOD MODE]", statusTextScale);
		uiY += statusLineHeight;
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

	drawOffscreenPlayerIndicators(proj, view, viewport);
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

void renderTextWithOutline(float x, float y, const char* text, float scale) {
	float offset = 5.0f * scale;

	renderTextTTF(x - offset, y, text, 0.0f, 0.0f, 0.0f, scale); // 좌
	renderTextTTF(x + offset, y, text, 0.0f, 0.0f, 0.0f, scale); // 우
	renderTextTTF(x, y - offset, text, 0.0f, 0.0f, 0.0f, scale); // 상
	renderTextTTF(x, y + offset, text, 0.0f, 0.0f, 0.0f, scale); // 하

	renderTextTTF(x - offset, y - offset, text, 0.0f, 0.0f, 0.0f, scale);
	renderTextTTF(x + offset, y - offset, text, 0.0f, 0.0f, 0.0f, scale);
	renderTextTTF(x - offset, y + offset, text, 0.0f, 0.0f, 0.0f, scale);
	renderTextTTF(x + offset, y + offset, text, 0.0f, 0.0f, 0.0f, scale);

	renderTextTTF(x, y, text, 1.0f, 1.0f, 1.0f, scale);
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