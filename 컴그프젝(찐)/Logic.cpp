#include "Logic.h"
#include "Global.h"
#include "Renderer.h" 
#include "Texture.h" 

GameSeason getSeasonByZ(int z) {
    int linesPassed = -z;
    int seasonIndex = (linesPassed / LINES_PER_SEASON) % 4;
    if (seasonIndex == 0) return SUMMER;
    if (seasonIndex == 1) return AUTUMN;
    if (seasonIndex == 2) return WINTER;
    if (seasonIndex == 3) return SPRING;
    return SUMMER;
}

bool isTreeAt(int x, int z) {
    if (treeMap.count(z)) {
        for (int treeX : treeMap[z]) if (treeX == x) return true;
    }
    return false;
}

bool spawnItem(int x, int z, ItemType type) {
    Item item{};
    item.x = static_cast<float>(x);
    item.z = static_cast<float>(z);
    item.type = type;
    if (type >= ITEM_SHIELD && type <= ITEM_BANANA) {
        item.id = nextItemId;
        const ItemKind kind = type == ITEM_SHIELD ? ItemKind::Shield
            : static_cast<ItemKind>(type - ITEM_REVERSE + 1);
        if (!localItems.AddGroundItem({item.id, kind, x, z})) return false;
        ++nextItemId;
    }
    items.push_back(item);
    return true;
}

void showItemFeedback(const char* message) {
    itemFeedbackMessage = message;
    itemFeedbackUntilMs = glutGet(GLUT_ELAPSED_TIME) + 1500;
}

void generateLane(int z)
{
	if (mapType.find(z) != mapType.end()) return;

	if (z >= -2 && z <= 2) {
		mapType[z] = 0;
		return;
	}

	static int lastRiverZ = 0; 

	int growDir = (z < 0) ? -1 : 1;

	int prevZ = z - growDir;
	bool prevWasRiver = (mapType.count(prevZ) && mapType[prevZ] == 2);

	bool forceRiver = (abs(z - lastRiverZ) > 15);

	int randVal = rand() % 10;

	if (!forceRiver && randVal < 6) {
		mapType[z] = 1; // 도로

		int numCars = 1 + rand() % 2;
		float speed = (0.05f + (rand() % 3) / 50.0f);
		if (rand() % 2 == 0) speed *= -1.0f;

		const float laneWidth = 36.0f;
		const float spacing = laneWidth / numCars;
		const float startX = (float)(rand() % 30 - 15);


		for (int i = 0; i < numCars; ++i) {
			Car newCar;
			newCar.z = (float)z;
			
			// 자동차 겹침 해결 
			newCar.x = startX + i * spacing;

			if (newCar.x >= 18.0f) {
				newCar.x -= laneWidth;
			}

			newCar.speed = speed;
			newCar.color = glm::vec3((rand() % 10) / 10.f, (rand() % 10) / 10.f, (rand() % 10) / 10.f);
			if (newCar.color.r < 0.2f) newCar.color.r += 0.5f;
			newCar.designID = (!carDesigns.empty()) ? rand() % carDesigns.size() : 0;
			cars.push_back(newCar);
		}
	}

	// 강
	else if ((forceRiver || randVal < 7) && !prevWasRiver) {

		int riverWidth = 2 + rand() % 3; 

		bool canPlaceRiver = true;
		for (int k = 1; k < riverWidth; ++k) {

			if (mapType.count(z + (k * growDir))) {
				canPlaceRiver = false;
				break;
			}
		}

		if (canPlaceRiver) {
			lastRiverZ = z; 
			int pathX = (rand() % 9) - 4; // 정답 경로

			for (int k = 0; k < riverWidth; ++k) {
				int currentZ = z + (k * growDir);
				mapType[currentZ] = 2; 

				bool isLogLane = (rand() % 2 == 0);

				if (isLogLane) { // 통나무
					float speed = 0.03f + (rand() % 3) / 100.0f;
					if (rand() % 2 == 0) speed *= -1.0f;

					// 안전 통나무
					Log safeLog;
					safeLog.z = (float)currentZ;
					safeLog.width = 5.0f;
					safeLog.speed = speed;
					safeLog.x = (float)pathX;
					logs.push_back(safeLog);

					// 랜덤 통나무
					int extraLogs = 1 + rand() % 2;
					for (int i = 0; i < extraLogs; i++) {
						Log log;
						log.z = (float)currentZ;
						log.width = 2.0f + (rand() % 3) * 0.5f;
						log.speed = speed;
						int randX = (rand() % 30 - 15);
						if (abs(randX - pathX) > 5) {
							log.x = (float)randX;
							logs.push_back(log);
						}
					}
					pathX += (speed > 0) ? 1 : -1;
				}
				else { // 연잎
					// 안전 연잎 뭉치
					for (int offset = -1; offset <= 1; ++offset) {
						LilyPad pad;
						pad.z = (float)currentZ;
						pad.x = (float)(pathX + offset);
						if (pad.x > -15 && pad.x < 15) lilyPads.push_back(pad);
					}
					// 장식 연잎
					for (int x = -14; x <= 14; x++) {
						if (abs(x - pathX) <= 2) continue;
						if (rand() % 100 < 35) {
							LilyPad pad;
							pad.z = (float)currentZ;
							pad.x = (float)x;
							lilyPads.push_back(pad);
						}
					}
				}
				pathX += (rand() % 3 - 1);
				if (pathX < -12) pathX = -12;
				if (pathX > 12) pathX = 12;
			}
		}
		else {
			goto MAKE_GRASS; 
		}
	}
	// 철길
	else if (randVal < 8) {
		mapType[z] = 3; 

		// 기차 객체 생성
		Train newTrain;
		newTrain.z = (float)z;
		newTrain.x = -50.0f; 
		newTrain.speed = 0.0f;
		newTrain.state = TRAIN_IDLE;
		newTrain.timer = rand() % 200 + 100; 
		newTrain.isLightOn = false;

		trains.push_back(newTrain);
	}

	// 잔디
	else {
	MAKE_GRASS:
		mapType[z] = 0; 

		// 나무 심기 
		for (int x = -15; x <= 15; ++x) {
			if (rand() % 10 < 1) {
				treeMap[z].push_back(x);
			}
		}

		// Spawn inventory items on grass.
		if (rand() % 100 < 80) {
			int spawnX = (rand() % 13) - 6;
			if (!isTreeAt(spawnX, z) && rand() % 100 < 35) {
				spawnItem(spawnX, z, static_cast<ItemType>(rand() % 4));
			}
		}
	}
}

void spawnParticles(glm::vec3 pos, glm::vec3 color, int count, float speedScale) {
	for (int i = 0; i < count; i++) {
		Particle p;
		p.position = pos;

		float rX = ((rand() % 100) / 100.0f - 0.5f) * speedScale;
		float rY = ((rand() % 100) / 100.0f) * (speedScale * 0.5f);
		float rZ = ((rand() % 100) / 100.0f - 0.5f) * speedScale;

		p.velocity = glm::vec3(rX, rY, rZ);
		p.color = color;
		p.scale = (rand() % 5) / 20.0f + 0.1f; 
		p.life = 1.0f;

		particles.push_back(p);
	}
}

void resetGame() {
	playerPos = glm::vec3(0.0f, 0.5f, 0.0f);
	playerTargetPos = playerPos;
	playerStartPos = playerPos;
	isMoving = false;

	items.clear();
    localItems.ResetRound();
    localItems.AddPlayer(1);
    nextItemId = 1;
    itemFeedbackMessage.clear();
    itemFeedbackUntilMs = 0;

	score = 0;
	minZ = 0;

	cars.clear();
	logs.clear();
	lilyPads.clear();
	treeMap.clear();
	mapType.clear();
	trains.clear();
	particles.clear();
	weatherParticles.clear();

	for (int z = -10; z < 10; ++z) generateLane(z);
    spawnItem(0, -1, static_cast<ItemType>(ITEM_REVERSE + rand() % 3));

	isGameOver = false;
}

void timer(int value)
{
	if (glutGetWindow() == 0) {
		return;
	}

    static int previousTickMs = glutGet(GLUT_ELAPSED_TIME);
    const int nowMs = glutGet(GLUT_ELAPSED_TIME);
    const std::uint32_t elapsedMs = nowMs > previousTickMs
        ? static_cast<std::uint32_t>(nowMs - previousTickMs) : 0;
    previousTickMs = nowMs;
	// [인트로 애니메이션 로직]
	if (introState != 0) { 

		// 상태 1: 로고 등장 (왼쪽 -> 중앙)
		if (introState == 1) {
			logoX = logoX + (logoTargetX - logoX) * 0.05f;
			if (std::abs(logoX - logoTargetX) < 1.0f) {
				logoX = logoTargetX;
				introState = 2; 
			}
		}
		// 상태 2: 대기
		else if (introState == 2) {
		}
		// 상태 3: 로고 퇴장 (중앙 -> 왼쪽)
		else if (introState == 3) {
			logoX = logoX + (logoStartX - logoX) * 0.05f;
			if (std::abs(logoX - logoStartX) < 10.0f) {
				introState = 0; 
			}
		}

		glutPostRedisplay();
		glutTimerFunc(16, timer, 0);
		return;
	}

	if (isGameOver) {
		for (auto it = particles.begin(); it != particles.end(); ) {
			it->position += it->velocity;
			it->life -= 0.05f;
			it->scale -= 0.005f;
			if (it->life <= 0.0f || it->scale <= 0.0f) it = particles.erase(it);
			else ++it;
		}

		glutPostRedisplay();
		glutTimerFunc(16, timer, 0);
		return;
	}

	if (shakeTimer > 0.0f) {
		shakeTimer -= 0.016f;
		if (shakeTimer <= 0.0f) {
			shakeTimer = 0.0f;
			shakeMagnitude = 0.0f; 
		}
	}

	float restingY = 0.5f;

	// 파티클 물리 업데이트
	for (auto it = particles.begin(); it != particles.end(); ) {
		it->position += it->velocity;
		it->life -= 0.05f;
		it->scale -= 0.005f;
		if (it->life <= 0.0f || it->scale <= 0.0f) it = particles.erase(it);
		else ++it;
	}

    localItems.AdvanceEffects(elapsedMs);
    static bool wasBlockedByFullInventory = false;
    bool blockedByFullInventory = false;
	for (auto& item : items) {
		if (item.isCollected) continue;
		if (std::abs(item.z - playerPos.z) < 0.5f && std::abs(item.x - playerPos.x) < 0.5f) {
            if (item.type >= ITEM_SHIELD) {
                const auto picked = localItems.ResolvePickups({{1,
                    (int)std::round(playerPos.x), (int)std::round(playerPos.z), true}});
                bool collectedThisItem = false;
                for (auto& other : items) {
                    for (const auto& result : picked) {
                        if (other.id == result.objectId) {
                            other.isCollected = true;
                            if (other.id == item.id) collectedThisItem = true;
                        }
                    }
                }
                if (!collectedThisItem) {
                    const auto* inventory = localItems.GetInventory(1);
                    if (inventory &&
                        inventory->slots[0] != ItemKind::Empty &&
                        inventory->slots[1] != ItemKind::Empty) {
                        blockedByFullInventory = true;
                    }
                    continue;
                }
                showItemFeedback("ITEM ACQUIRED");
            }
            item.isCollected = true;
            PlaySound(TEXT("item.wav"), NULL, SND_ASYNC | SND_FILENAME);

		}
	}
    if (blockedByFullInventory && !wasBlockedByFullInventory)
        showItemFeedback("INVENTORY FULL");
    wasBlockedByFullInventory = blockedByFullInventory;

    if (localItems.ResolveBananaHit(1, (int)std::round(playerPos.x),
        (int)std::round(playerPos.z))) {
        playerPos.z += 1.0f;
        playerTargetPos = playerPos;
        playerStartPos = playerPos;
        isMoving = false;
    }

		for (auto& logObj : logs) {
			// 슬로우 효과 적용
			logObj.x += logObj.speed;
			if (logObj.speed > 0 && logObj.x > 20.0f) logObj.x = -20.0f;
			else if (logObj.speed < 0 && logObj.x < -20.0f) logObj.x = 20.0f;

			bool onLog = (std::abs(playerPos.z - logObj.z) < 0.1f) &&
				(playerPos.x >= logObj.x - logObj.width / 2.0f - 0.3f) &&
				(playerPos.x <= logObj.x + logObj.width / 2.0f + 0.3f);

			if (!isMoving && onLog) {
				playerPos.x += logObj.speed;
				playerTargetPos.x += logObj.speed;
				playerStartPos.x += logObj.speed;
				restingY = 1.1f;
			}
		}

		// [충돌 검사 시작]
		bool isDead = false;
        const auto* itemEffects = localItems.GetPlayerEffects(1);
        const bool shieldActive = itemEffects && itemEffects->shieldRemainingMs > 0;
        const bool isInvincible = isGodMode || shieldActive;

		// 자동차 충돌
		for (auto& car : cars) {
			//  슬로우 효과 적용
			car.x += car.speed;
			if (car.x > 18.0f && car.speed > 0) car.x = -18.0f;
			if (car.x < -18.0f && car.speed < 0) car.x = 18.0f;

			// 충돌 체크
			bool hit = (abs(car.z - playerPos.z) < 0.2f && abs(car.x - playerPos.x) < 0.8f) ||
				(abs(car.z - playerPos.z) < 0.08f && abs(car.x - playerPos.x) < 1.2f);

            if (hit && !isGodMode) {
                if (shieldActive) {
                    car.x = (car.speed > 0) ? 20.0f : -20.0f;
                } else {
                    isDead = true;
                }
            }
        }

		// 익사(물) 판정
		int pZ = (int)std::round(playerPos.z);
		if (!isMoving && mapType.count(pZ) && mapType[pZ] == 2) {
			bool safe = false;
			{
				// 통나무 체크
				for (const auto& logObj : logs) {
					if (logObj.z == (float)pZ &&
						playerPos.x >= logObj.x - logObj.width / 2.0f - 0.4f &&
						playerPos.x <= logObj.x + logObj.width / 2.0f + 0.4f) {
						safe = true;
						restingY = 1.1f;
						break;
					}
				}
				// 연잎 체크
				if (!safe) {
					for (const auto& pad : lilyPads) {
						if (pad.z == (float)pZ && std::abs(playerPos.x - pad.x) < 0.6f) {
							safe = true;
							restingY = 0.93f;
							break;
						}

					}
				}
				if (!safe) {
					if (isInvincible) {
						restingY = 0.5f;
					}
					else {
						isDead = true;
					}
				}
				if ((playerPos.x < -16.0f || playerPos.x > 16.0f) && !isInvincible) isDead = true;
			}
		}

		// 기차 충돌 검사
		for (auto& t : trains) {
			switch (t.state) {
			case TRAIN_IDLE:
				t.timer--;
				if (t.timer <= 0) {
					t.state = TRAIN_WARNING;
					t.timer = 120;
				}
				break;
			case TRAIN_WARNING:
				t.timer--;
				if ((t.timer / 10) % 2 == 0) t.isLightOn = true;
				else t.isLightOn = false;

				if (t.timer <= 0) {
					t.state = TRAIN_PASSING;
					t.x = -60.0f;
					t.speed = 3.0f;
				}
				break;
			case TRAIN_PASSING:
				t.x += t.speed;

				if (t.x > -20.0f && t.x < 25.0f &&
					std::abs(t.z - playerPos.z) < 10.0f) {

					shakeTimer = 0.1f;
					shakeMagnitude = 0.2f;
				}

				// 충돌 체크
				if (t.state == TRAIN_PASSING)
				{
					int playerGridZ = (int)std::round(playerPos.z);
					int trainGridZ = (int)std::round(t.z);

					if (playerGridZ == trainGridZ) {
						if (playerPos.x > t.x - 9.0f && playerPos.x < t.x + 9.0f) {
                            if (!isInvincible) isDead = true;
						}
					}
				}

				if (t.x > 60.0f) {
					t.state = TRAIN_IDLE;
					t.timer = rand() % 300 + 200;
				}
				break;
			}
		}

		if (isDead && !shieldActive) {

			if (isGameOver) {
				glutPostRedisplay();
				glutTimerFunc(16, timer, 0);
				return;
			}

			// 파티클 생성 
			spawnParticles(playerPos, glm::vec3(1.0f, 1.0f, 1.0f), 10, 1.0f);
			spawnParticles(playerPos, glm::vec3(1.0f, 0.2f, 0.2f), 5, 1.0f);
			spawnParticles(playerPos, glm::vec3(1.0f, 0.6f, 0.0f), 5, 1.0f);

			isGameOver = true; 

			glutPostRedisplay();
			glutTimerFunc(16, timer, 0);
			return;
		}

		if (isMoving) {
			moveTime += 0.032f;
			float t = glm::clamp(moveTime / MOVE_DURATION, 0.0f, 1.0f);

			playerPos.x = glm::mix(playerStartPos.x, playerTargetPos.x, t);
			playerPos.z = glm::mix(playerStartPos.z, playerTargetPos.z, t);

			float jumpY = JUMP_HEIGHT * 4.0f * t * (1.0f - t);
			playerPos.y = 0.5f + jumpY;

			if (t >= 1.0f) {
				playerPos = playerTargetPos;
				isMoving = false;

				int landZ = (int)std::round(playerPos.z);
				if (mapType.count(landZ) && mapType[landZ] == 2) {
					glm::vec3 particleColor = glm::vec3(0.0f, 0.5f, 1.0f);
					spawnParticles(playerPos, particleColor, 8, 0.2f);
				}
			}
		}

		if (!isMoving) {
			playerPos.y = restingY;
			playerStartPos.y = restingY;
		}

	// 계절별 날씨 파티클 생성 로직
	int currentPZ = (int)std::round(playerPos.z);
	GameSeason nowSeason = getSeasonByZ(currentPZ);

	if (nowSeason != SUMMER) {
		for (int i = 0; i < 3; ++i) {
			WeatherParticle wp;

			float rangeX = ((rand() % 500) / 10.0f) - 25.0f;
			float rangeZ = ((rand() % 500) / 10.0f) - 25.0f;
			wp.pos = glm::vec3(playerPos.x + rangeX, 15.0f, playerPos.z + rangeZ);

			wp.swayPhase = (rand() % 100) / 10.0f; 

			// 계절별 속성 설정
			if (nowSeason == SPRING) {
				// 봄: 벚꽃
				wp.color = glm::vec3(0.9f, 0.4f, 0.6f); 
				wp.scaleVec = glm::vec3(0.15f, 0.02f, 0.15f);
				wp.speed = 0.03f + (rand() % 5) / 100.0f;
			}
			else if (nowSeason == AUTUMN) {
				// 가을: 낙엽
				float rVar = (rand() % 3) / 10.0f; 
				wp.color = glm::vec3(0.7f + rVar, 0.35f, 0.05f);
				wp.scaleVec = glm::vec3(0.2f, 0.02f, 0.2f);
				wp.speed = 0.06f + (rand() % 5) / 100.0f;
			}
			else if (nowSeason == WINTER) {
				// 겨울: 눈 
				wp.color = glm::vec3(1.0f, 1.0f, 1.0f); 
				wp.scaleVec = glm::vec3(0.12f, 0.12f, 0.12f);
				wp.speed = 0.1f + (rand() % 10) / 100.0f;
			}

			weatherParticles.push_back(wp);
		}
	}

	// 3. 파티클 업데이트 
	for (auto it = weatherParticles.begin(); it != weatherParticles.end(); ) {
		it->pos.y -= it->speed; 

		// 흔들리는 효과
		it->swayPhase += 0.05f;
		it->pos.x += sin(it->swayPhase) * 0.03f;

		if (it->pos.y < -1.0f) {
			it = weatherParticles.erase(it);
		}
		else {
			++it;
		}
	}

	glutPostRedisplay();
	glutTimerFunc(16, timer, 0);

}

void initGame()
{
    localItems.AddPlayer(1);
	transLoc = glGetUniformLocation(shaderProgramID, "trans");

	float vertices[] = {
		// 위치(X,Y,Z) // 텍스처 좌표(U,V)
		-0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
		0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		-0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
		0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
		-0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

		-0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
		-0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
		-0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

		0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
		0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
		0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
		0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
		-0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

		-0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
		0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
		-0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
		-0.5f, 0.5f, -0.5f, 0.0f, 1.0f
	};

	// 바퀴창문
	glm::vec3 wheelColor = glm::vec3(0.1f, 0.1f, 0.1f); // 검은색
	glm::vec3 windowColor = glm::vec3(0.2f, 0.2f, 0.3f); // 짙은 파란색/회색

	//바퀴4개
	float wheelXOffset = 0.6f;
	float wheelZOffset = 0.45f;
	float wheelYOffset = -0.2f; 
	float wheelScale = 0.3f;

	//세단스타일
	CarDesign sedan;
	sedan.baseScale = 1.0f;
	//바디길고납작
	sedan.parts.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.7f, 0.5f, 0.8f), glm::vec3(0.0f) }); 
	// 캐빈/지붕앞쪽위
	sedan.parts.push_back({ glm::vec3(0.1f, 0.35f, 0.0f), glm::vec3(0.8f, 0.4f, 0.7f), glm::vec3(1.0f, 1.0f, 1.0f) }); // 흰색 지붕

	//앞바퀴
	sedan.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	sedan.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });
	//뒷바퀴
	sedan.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	sedan.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });

	//창문 추가
	float windowXOffset = 0.1f; 
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
	suv_pickup.parts.push_back({ glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(1.5f, 0.5f, 0.9f), glm::vec3(0.0f) }); 
	//앞부분
	suv_pickup.parts.push_back({ glm::vec3(0.65f, 0.2f, 0.0f), glm::vec3(0.6f, 0.2f, 0.8f), glm::vec3(0.0f) }); 
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
	truck.parts.push_back({ glm::vec3(0.5f, 0.1f, 0.0f), glm::vec3(0.7f, 0.7f, 0.9f), glm::vec3(0.0f) });
	//뒷부분
	truck.parts.push_back({ glm::vec3(-0.7f, 0.2f, 0.0f), glm::vec3(2.0f, 1.2f, 0.95f), glm::vec3(0.9f, 0.9f, 0.9f) }); // 회색 화물칸
	//캐빈/지붕 (흰색)
	truck.parts.push_back({ glm::vec3(0.5f, 0.6f, 0.0f), glm::vec3(0.7f, 0.4f, 0.8f), glm::vec3(1.0f, 1.0f, 1.0f) }); // 흰색 지붕
	truck.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	truck.parts.push_back({ glm::vec3(wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });
	//뒷바퀴
	truck.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, wheelZOffset), glm::vec3(wheelScale), wheelColor });
	truck.parts.push_back({ glm::vec3(-wheelXOffset, wheelYOffset, -wheelZOffset), glm::vec3(wheelScale), wheelColor });

	// 창문 추가
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
    spawnItem(0, -1, static_cast<ItemType>(ITEM_REVERSE + rand() % 3));

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

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);

	loadTexture("river.jpg", &riverTexture);
	loadTexture("grass.jpg", &grassTexture);
	loadTexture("wood.jpg", &logTexture);
	loadTexture("lilypad.jpg", &lilyPadTexture);
	loadTexture("logo.png", &logoTexture);
}