#include "ClientNetwork.h"
#include "Input.h"
#include "Global.h"
#include "Logic.h"
#include "Protocol.h"
#include "GameViewport.h"
#include <iostream>
#include <cmath>

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

#endif

	if (isMoving || isGameOver || localBirdCarry.active) return;
	if (activeClientNetwork && (!activeClientNetwork->Ready() || activeClientNetwork->MovePending())) return;

	// 글리치 모드일 때 키 입력 뒤집기
	const auto* effects = localItems.GetPlayerEffects(1);
	if (!activeClientNetwork && effects && effects->reverseControlsRemainingMs > 0) {
		switch (key) {
		case GLUT_KEY_UP:    key = GLUT_KEY_DOWN; break;
		case GLUT_KEY_DOWN:  key = GLUT_KEY_UP; break;
		case GLUT_KEY_LEFT:  key = GLUT_KEY_RIGHT; break;
		case GLUT_KEY_RIGHT: key = GLUT_KEY_LEFT; break;
		}
	}
	if (activeClientNetwork) {
		Direction physical;

		switch (key) {
		case GLUT_KEY_UP:
			physical = Direction::UP;
			break;
		case GLUT_KEY_DOWN:
			physical = Direction::DOWN;
			break;
		case GLUT_KEY_LEFT:
			physical = Direction::LEFT;
			break;
		case GLUT_KEY_RIGHT:
			physical = Direction::RIGHT;
			break;
		default:
			return;
		}

		const auto* effect = localItems.GetPlayerEffects(1);

		const bool reversed = effect && effect->reverseControlsRemainingMs > 0;

		const int visualKey = reversed
			? (key == GLUT_KEY_UP ? GLUT_KEY_DOWN
				: key == GLUT_KEY_DOWN ? GLUT_KEY_UP
				: key == GLUT_KEY_LEFT ? GLUT_KEY_RIGHT
				: GLUT_KEY_LEFT)
			: key;

		playerRotation =
			visualKey == GLUT_KEY_UP ? 0.0f
			: visualKey == GLUT_KEY_DOWN ? 180.0f
			: visualKey == GLUT_KEY_LEFT ? 90.0f
			: -90.0f;

		activeClientNetwork->RequestMove(physical, playerPos.x, playerPos.z);

		return;
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

	if ((!activeClientNetwork || mapType.find(nextZ) != mapType.end()) && !isTreeAt(nextX, nextZ))
	{
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
	if (clientScreen == ClientScreen::Lobby &&
		(key == 'r' || key == 'R'))
	{
		// 준비 버튼과 같은 요청으로 준비 / 준비 취소 전환
		if (activeClientNetwork) {
			activeClientNetwork->RequestReady(
				!activeClientNetwork->IsMyLobbyReady());
		}

		return;
	}
	if (clientScreen != ClientScreen::Playing) {
		if (key == 'q' || key == 'Q') {
			glutLeaveMainLoop();
		}

		return;
	}

	// 게임 오버 시 아무 키나 누르면 재시작
	if (localBirdCarry.active) return;
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

		if (inventory) {
			const auto next = static_cast<std::uint8_t>(
				1 - inventory->selectedSlot);

			if (activeClientNetwork) {
				activeClientNetwork->RequestSwitch(next);
			}
			else {
				localItems.SwitchSlot(1, next);
			}
		}

		glutPostRedisplay();
		return;
	}
	if (key == 'x' || key == 'X') {
		if (activeClientNetwork) {
			activeClientNetwork->RequestUse();
			return;
		}

		const auto* inventory = localItems.GetInventory(1);

		const bool emptySlot = !inventory || inventory->slots[inventory->selectedSlot] == ItemKind::Empty;

		const bool used = localItems.UseSelectedItem(1,
			static_cast<int>(std::round(playerPos.x)),
			static_cast<int>(std::round(playerPos.z)));

		showItemFeedback(
			used ? "ITEM USED"
			: emptySlot ? "EMPTY SLOT"
			: "CANNOT USE ITEM");

		glutPostRedisplay();
		return;
	}
	if (key >= '1' && key <= '5') {
		const auto kind =
			static_cast<std::uint8_t>(key - '0');

		if (activeClientNetwork) {
			activeClientNetwork->RequestSpawn(kind);
		}
		else {
			spawnItem(
				static_cast<int>(std::round(playerPos.x)),
				static_cast<int>(std::round(playerPos.z)) - 2,
				kind == 4
				? ITEM_SHIELD
				: kind == 5 ? ITEM_DASH_MUSHROOM
				: static_cast<ItemType>(
					ITEM_REVERSE + kind - 1));
		}

		glutPostRedisplay();
		return;
	}
	if (!activeClientNetwork && (key == '4' || key == '9'))
	{
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

static float lobbyPreviewAngle = 0.0f;
static bool previewDragging = false;
static int previewLastMouseX = 0;

float GetLobbyPreviewAngle() { return lobbyPreviewAngle; }

void mouse(int button, int state, int x, int y)
{
	if (button != GLUT_LEFT_BUTTON) return;
	if (state == GLUT_UP) { previewDragging = false; return; }
	if (state != GLUT_DOWN) return;
	previewDragging = false;
	const int width = glutGet(GLUT_WINDOW_WIDTH);
	const int height = glutGet(GLUT_WINDOW_HEIGHT);

	if (width <= 0 || height <= 0) {
		return;
	}
	const GameViewport viewport = FitGameViewport(width, height);
	const int top = height - viewport.y - viewport.height;
	if (x < viewport.x || x >= viewport.x + viewport.width ||
		y < top || y >= top + viewport.height) {
		return;
	}
	const float uiX = (x - viewport.x) * 1280.0f / viewport.width;
	const float uiY = (y - top) * 960.0f / viewport.height;

	const auto inside = [&](float left, float top,
		float w, float h)
		{
			return uiX >= left && uiX <= left + w &&
				uiY >= top && uiY <= top + h;
		};

	if (clientScreen == ClientScreen::Title &&
		introState == 2)
	{
		if (inside(460, 680, 360, 90)) {
			if (!activeClientNetwork) {
				std::cout
					<< "Start the server and run with 127.0.0.1.\n";
				return;
			}

			activeClientNetwork->RequestJoin("Player");
		}

		return;
	}
	if (!activeClientNetwork) {
		return;
	}
	if (clientScreen == ClientScreen::Matching) {
		if (inside(460, 780, 360, 90)) {
			activeClientNetwork->RequestLeave();
		}

		return;
	}
	if (clientScreen == ClientScreen::Lobby) {
		const auto& lobby = activeClientNetwork->GetLobby();
		if (lobby.stage == LobbyStage::Ready) {
			for (int i = 0; i < static_cast<int>(lobby.members.size()) && i < 3; ++i) {
				if (lobby.members[i].id != activeClientNetwork->GetMyPlayerId()) continue;
				const float centerX = 320.0f + i * 320.0f;
				if (inside(centerX - 130.0f, 300.0f, 260.0f, 300.0f)) {
					previewDragging = true;
					previewLastMouseX = x;
					return;
				}
				break;
			}
		}
		if (lobby.stage == LobbyStage::Ready && !lobby.readyPending &&
			!lobby.leaving && !activeClientNetwork->IsMyLobbyReady() &&
			!activeClientNetwork->IsCharacterSelectionPending()) {
			for (int i = 0; i < static_cast<int>(lobby.members.size()) && i < 3; ++i) {
				if (lobby.members[i].id != activeClientNetwork->GetMyPlayerId()) continue;
				const float centerX = 320.0f + i * 320.0f;
				const int id = static_cast<int>(lobby.members[i].characterId);
				if (inside(centerX - 195.0f, 485.0f, 55.0f, 70.0f)) {
					activeClientNetwork->RequestCharacterSelection(
						static_cast<CharacterId>((id + CharacterCount - 1) % CharacterCount));
					return;
				}
				if (inside(centerX + 140.0f, 485.0f, 55.0f, 70.0f)) {
					activeClientNetwork->RequestCharacterSelection(
					static_cast<CharacterId>((id + 1) % CharacterCount));
				return;
				}
				if (inside(centerX - 125.0f, 740.0f, 34.0f, 38.0f) ||
					inside(centerX + 91.0f, 740.0f, 34.0f, 38.0f)) {
					const int direction = inside(centerX - 125.0f, 740.0f, 34.0f, 38.0f) ? -1 : 1;
					for (int step = 1; step < HeadAccessoryCount; ++step) {
						const auto head = static_cast<HeadAccessory>(
							(static_cast<int>(lobby.members[i].accessories.head) + direction * step + HeadAccessoryCount) % HeadAccessoryCount);
						if (CanEquipHead(lobby.members[i].characterId, head)) {
							auto next = lobby.members[i].accessories;
							next.head = head;
							activeClientNetwork->RequestAppearanceSelection(lobby.members[i].characterId, next);
							break;
						}
					}
					return;
				}
				if (inside(centerX - 125.0f, 775.0f, 34.0f, 38.0f) ||
					inside(centerX + 91.0f, 775.0f, 34.0f, 38.0f)) {
					const int direction = inside(centerX - 125.0f, 775.0f, 34.0f, 38.0f) ? -1 : 1;
					auto next = lobby.members[i].accessories;
					next.back = static_cast<BackAccessory>(
						(static_cast<int>(next.back) + direction + BackAccessoryCount) % BackAccessoryCount);
					activeClientNetwork->RequestAppearanceSelection(lobby.members[i].characterId, next);
					return;
				}
				if (inside(centerX - 125.0f, 810.0f, 34.0f, 38.0f) ||
					inside(centerX + 91.0f, 810.0f, 34.0f, 38.0f)) {
					const int direction = inside(centerX - 125.0f, 810.0f, 34.0f, 38.0f) ? -1 : 1;
					auto next = lobby.members[i].accessories;
					next.face = static_cast<FaceAccessory>(
						(static_cast<int>(next.face) + direction + FaceAccessoryCount) % FaceAccessoryCount);
					activeClientNetwork->RequestAppearanceSelection(lobby.members[i].characterId, next);
					return;
				}
			break;
			}
		}
		// 준비 / 준비 취소
		if (inside(460, 865, 360, 80)) {
			activeClientNetwork->RequestReady(
				!activeClientNetwork->IsMyLobbyReady());
		}
		// 나가기
		else if (inside(1000, 875, 200, 60)) {
			activeClientNetwork->RequestLeave();
		}
	}
}
void mouseMotion(int x, int)
{
	if (!previewDragging) return;
	if (clientScreen != ClientScreen::Lobby || !activeClientNetwork ||
		activeClientNetwork->GetLobby().stage != LobbyStage::Ready) {
		previewDragging = false;
		return;
	}
	const int width = FitGameViewport(
		glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT)).width;
	if (width <= 0) return;
	lobbyPreviewAngle = std::fmod(lobbyPreviewAngle +
		(x - previewLastMouseX) * 1280.0f / width, 360.0f);
	previewLastMouseX = x;
	glutPostRedisplay();
}
