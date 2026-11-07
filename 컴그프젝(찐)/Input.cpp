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