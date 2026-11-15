#include "ClientNetwork.h"
#include "Input.h"
#include "Global.h"
#include "Logic.h"
#include "Protocol.h"
#include <iostream>

void specialKeyboard(int key, int x, int y)
{
	if (clientScreen != ClientScreen::Playing) {
		return;
	}

	//  인트로 상태 처리
	if (introState != 0) {
		if (introState == 2) {
			introState = 3;
		}
		return; 
	}

#ifdef _DEBUG
	// 실제로 누른 방향키를 MOVE 방향 값으로 변환
	Direction physicalDirection;

	switch (key) {
	case GLUT_KEY_UP:    physicalDirection = Direction::UP; break;
	case GLUT_KEY_DOWN:  physicalDirection = Direction::DOWN; break;
	case GLUT_KEY_LEFT:  physicalDirection = Direction::LEFT; break;
	case GLUT_KEY_RIGHT: physicalDirection = Direction::RIGHT; break;
	default: return;
	}

	//std::cout << "MOVE direction: "
	//	<< static_cast<int>(physicalDirection) << '\n';
#endif

	if (isMoving || isGameOver) return;
	if (activeClientNetwork && (!activeClientNetwork->Ready() || activeClientNetwork->MovePending())) return;

	// 글리치 모드일 때 키 입력 뒤집기
	const auto* effects = localItems.GetPlayerEffects(1);
    if (effects && effects->reverseControlsRemainingMs > 0) {
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
        if (activeClientNetwork) {
            Direction direction = key == GLUT_KEY_UP ? Direction::UP :
                key == GLUT_KEY_DOWN ? Direction::DOWN :
                key == GLUT_KEY_LEFT ? Direction::LEFT : Direction::RIGHT;
            activeClientNetwork->RequestMove(direction, playerPos.x, playerPos.z);
            return;
        }
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
	if (clientScreen != ClientScreen::Playing) {
		if (key == 'q' || key == 'Q') {
			glutLeaveMainLoop();
		}

		return;
	}

	// 게임 오버 시 아무 키나 누르면 재시작
	if (isGameOver) {
		resetGame();
		return;
	}

	if (key == 'q' || key == 'Q') {
		glutLeaveMainLoop();
		return;
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

    if (key == 'z' || key == 'Z') {
        const auto* inventory = localItems.GetInventory(1);
        if (inventory) localItems.SwitchSlot(1, 1 - inventory->selectedSlot);
        glutPostRedisplay();
        return;
    }
    if (key == 'x' || key == 'X') {
        const auto* inventory = localItems.GetInventory(1);
        const bool emptySlot = !inventory ||
            inventory->slots[inventory->selectedSlot] == ItemKind::Empty;
        const bool used = localItems.UseSelectedItem(
            1, (int)std::round(playerPos.x), (int)std::round(playerPos.z));
        showItemFeedback(used ? "ITEM USED"
                              : emptySlot ? "EMPTY SLOT" : "CANNOT USE ITEM");
        glutPostRedisplay();
        return;
    }

    if (key == '4' || key == '9') {
        const int itemX = (int)std::round(playerPos.x);
        const int itemZ = (int)std::round(playerPos.z) - 2;
        if (key == '4') {
            spawnItem(itemX, itemZ, ITEM_SHIELD);
        } else {
            static unsigned nextKind = 0;
            spawnItem(itemX, itemZ, static_cast<ItemType>(ITEM_REVERSE + nextKind));
            nextKind = (nextKind + 1) % 3;
        }
    }

}

void mouse(int button, int state, int x, int y)
{
	if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) {
		return;
	}

	const int width = glutGet(GLUT_WINDOW_WIDTH);
	const int height = glutGet(GLUT_WINDOW_HEIGHT);

	if (width <= 0 || height <= 0) {
		return;
	}

	// 실제 창의 마우스 좌표를 UI 기준 좌표로 변환
	const float uiX = x * 1280.0f / width;
	const float uiY = y * 960.0f / height;

	// 타이틀의 GAME START
	if (clientScreen == ClientScreen::Title &&
		introState == 2)
	{
		const bool clicked =
			uiX >= 460.0f && uiX <= 820.0f &&
			uiY >= 680.0f && uiY <= 770.0f;

		if (clicked) {
			// 화면 확인용 이동.
			// 통신 연결 시 여기에서 JOIN을 요청하고,
			// 서버 응답에 따라 화면을 전환하도록 변경.
			clientScreen = ClientScreen::Lobby;
			introState = 0;

			glutPostRedisplay();
		}

		return;
	}

	// 대기방의 BACK
	if (clientScreen == ClientScreen::Lobby) {
		const bool clicked =
			uiX >= 460.0f && uiX <= 820.0f &&
			uiY >= 780.0f && uiY <= 870.0f;

		if (clicked) {
			clientScreen = ClientScreen::Title;

			// 돌아갈 때는 로고를 중앙에 바로 표시
			introState = 2;
			logoX = logoTargetX;

			glutPostRedisplay();
		}
	}
}
