#include "Input.h"
#include "Global.h"
#include "Logic.h"

void specialKeyboard(int key, int x, int y)
{
	//  인트로 상태 처리
	if (introState != 0) {
		if (introState == 2) {
			introState = 3;
		}
		return; 
	}

	if (isMoving) return;
	if (isMoving || isDashing) return;

	// 글리치 모드일 때 키 입력 뒤집기
	if (isGlitchMode) {
		switch (key) {
		case GLUT_KEY_UP:    key = GLUT_KEY_DOWN; break;
		case GLUT_KEY_DOWN:  key = GLUT_KEY_UP; break;
		case GLUT_KEY_LEFT:  key = GLUT_KEY_RIGHT; break;
		case GLUT_KEY_RIGHT: key = GLUT_KEY_LEFT; break;
		}
	}

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

			int currentTotalLines = -minZ;
		}

		playerStartPos = playerPos;

		playerTargetPos = glm::vec3((float)nextX, 0.5f, (float)nextZ);

		isMoving = true;
		moveTime = 0.0f;

		PlaySound(TEXT("footstep.wav"), NULL, SND_ASYNC | SND_FILENAME);
	}
	glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y)
{
	// 게임 오버 시 아무 키나 누르면 재시작
	if (isGameOver) {
		resetGame();
		return;
	}

	if (key == 'q' || key == 'Q') {
		exit(0);
	}

	if (key == '0') {
		isGodMode = !isGodMode;
		printf("God Mode: %s\n", isGodMode ? "ON" : "OFF");
	}

	// 인트로 상태 처리
	if (introState != 0) {
		if (introState == 2) {
			introState = 3;
		}
		return; 
	}

	if (key == ' ') {
		if (isEventActive) {
			requiredTaps++;

			const float TOTAL_TAPS_REQUIRED = TAP_PER_SECOND * 4.0f;
			eventProgress = requiredTaps / TOTAL_TAPS_REQUIRED;
			eventProgress = glm::clamp(eventProgress, 0.0f, 1.1f);

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
			nightWarningTimer = 3.0f; 
		}
		else {
			isNightWarning = false;
			isNightMode = false;
			nightModeTimer = 0.0f;
		}
	}

	if (key == '3') {
		if (!isWindActive) {
			isWindActive = true;
			windTimer = 6.0f; 
			windForce = 0.04f; 
		}
		else {
			isWindActive = false;
		}
	}

	if (key == '4' || key == '5' || key == '6' || key == '8') {
		Item newItem;
		newItem.x = (float)std::round(playerPos.x);       
		newItem.z = (float)std::round(playerPos.z) - 2.0f;
		newItem.isCollected = false;
		newItem.rotation = 0.0f;

		// 보호막
		if (key == '4') {
			newItem.type = ITEM_SHIELD;
		}
		// 자석
		else if (key == '5') {
			newItem.type = ITEM_MAGNET;
		}
		// 시계
		else if (key == '6') {
			newItem.type = ITEM_CLOCK;
		}
		// 성장물약
		else if (key == '8') { 
			newItem.type = ITEM_POTION;
		}
		items.push_back(newItem);
	}
	// 글리치 모드
	if (key == '7') {
		isGlitchMode = !isGlitchMode;
		if (isGlitchMode) glitchTimer = 5.0f;
	}
	// 거인 모드
	if (key == 'g' || key == 'G') {
		if (!isGiantMode && !isFlying && introState == 0) {
			isGiantMode = true;
			giantTimer = 8.0f; 
		}
	}
}