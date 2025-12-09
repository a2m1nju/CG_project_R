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

		for (int i = 0; i < numCars; ++i) {
			Car newCar;
			newCar.z = (float)z;
			newCar.x = (float)(rand() % 30 - 15);
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

		// 코인 및 아이템 생성
		if (rand() % 2 == 0) {
			int spawnX = (rand() % 21) - 10;
			if (!isTreeAt(spawnX, z)) {

				int luck = rand() % 100;

				if (luck < 20) {
					Item newItem;
					newItem.x = (float)spawnX;
					newItem.z = (float)z;
					newItem.type = (ItemType)(rand() % 4);
					items.push_back(newItem);
				}
				else {
					Coin newCoin;
					newCoin.x = (float)spawnX;
					newCoin.z = (float)z;
					coins.push_back(newCoin);
				}
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
	isDashing = false;
	dashTimer = 0.0f;
	dashCooldownTimer = 0.0f;

	// 각종 이벤트 초기화
	isWindActive = false;
	windTimer = 0.0f;
	windForce = 0.0f;

	isNightMode = false;
	isNightWarning = false;
	nightModeTimer = 0.0f;
	nightWarningTimer = 0.0f;
	nightEventCooldown = 10.0f;

	isEventActive = false;
	eventProgress = 0.0f;
	requiredTaps = 0;
	lastEventScore = 0;

	// 아이템 초기화
	items.clear();
	hasShield = false;
	isMagnetActive = false;
	isSlowActive = false;
	magnetTimer = 0.0f;
	slowTimer = 0.0f;

	isFlying = false;
	isLanding = false;
	isBirdLeaving = false;

	score = 0;
	minZ = 0;
	coinCount = 0;

	cars.clear();
	logs.clear();
	lilyPads.clear();
	treeMap.clear();
	mapType.clear();
	trains.clear();
	particles.clear();
	weatherParticles.clear();

	for (int z = -10; z < 10; ++z) generateLane(z);

	isGameOver = false;
	isGlitchMode = false;
}

void timer(int value)
{
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

	// [글리치 모드 로직]
	if (isGlitchMode) {
		glitchTimer -= 0.016f;

		// 1. 지속적인 화면 흔들림 
		shakeTimer = 0.1f;     
		shakeMagnitude = 0.15f;  

		// 2. 종료 체크
		if (glitchTimer <= 0.0f) {
			isGlitchMode = false;
		}
	}
	else {
		// [발동 조건] 
		if (!isFlying && !isEventActive && (rand() % 5000 < 2)) {
			isGlitchMode = true;
			glitchTimer = 5.0f;
		}
	}
	// 거대화 모드 시간 감소 로직
	if (isGiantMode) {
		giantTimer -= 0.016f;
		if (giantTimer <= 0.0f) {
			isGiantMode = false;
			giantTimer = 0.0f;
		}
	}

	// 카메라 흔들림 시간 감소 로직
	if (shakeTimer > 0.0f) {
		shakeTimer -= 0.016f;
		if (shakeTimer <= 0.0f) {
			shakeTimer = 0.0f;
			shakeMagnitude = 0.0f; 
		}
	}

	// 핀 조명 로직
	// 1. 경고 단계
	if (isNightWarning) {
		nightWarningTimer -= 0.016f;

		if (nightWarningTimer <= 0.0f) {
			isNightWarning = false;
			isNightMode = true; 
			nightModeTimer = NIGHT_DURATION;
		}
	}
	// 2. 밤(핀 조명) 단계
	else if (isNightMode) {
		nightModeTimer -= 0.016f;
		if (nightModeTimer <= 0.0f) {
			isNightMode = false;
			nightEventCooldown = (rand() % 10) + 10.0f; 
		}
	}
	// 3. 평상시 (대기 단계)
	else {
		if (nightEventCooldown > 0.0f) {
			nightEventCooldown -= 0.016f;
		}
		else {
			if (rand() % 1500 < 1) {
				isNightWarning = true;
				nightWarningTimer = WARNING_DURATION;
			}
		}
	}

	float playerX = playerPos.x;
	float playerZ = playerPos.z;
	float playerSize = 0.5f;
	bool scoreTargetReached = (score > 0) && (score % 50 == 0);

	float restingY = 0.5f;

	// 파티클 물리 업데이트
	for (auto it = particles.begin(); it != particles.end(); ) {
		it->position += it->velocity;
		it->life -= 0.05f;
		it->scale -= 0.005f;
		if (it->life <= 0.0f || it->scale <= 0.0f) it = particles.erase(it);
		else ++it;
	}

	// 대쉬 쿨타임 감소
	if (dashCooldownTimer > 0.0f) {
		dashCooldownTimer -= 0.016f;
		if (dashCooldownTimer < 0.0f) dashCooldownTimer = 0.0f;
	}
	// 무적 시간(recoveryTimer) 감소 로직
	if (recoveryTimer > 0.0f) {
		recoveryTimer -= 0.016f;
		if (recoveryTimer < 0.0f) {
			recoveryTimer = 0.0f;
			isLandingSuccess = false;
		}
	}

	// 5. [아이템] 슬로우 모션 속도 설정
	float globalSpeedRate = 1.0f;
	if (isSlowActive) {
		globalSpeedRate = 0.5f;
		slowTimer -= 0.016f;
		if (slowTimer <= 0.0f) {
			isSlowActive = false;
		}
	}

	// 6. [아이템] 자석 타이머 및 효과
	if (isMagnetActive) {
		magnetTimer -= 0.016f;
		if (magnetTimer <= 0.0f) {
			isMagnetActive = false;
		}
		// 코인 끌어당기기 로직
		for (auto& coin : coins) {
			if (coin.isCollected) continue;
			float dist = glm::distance(playerPos, glm::vec3(coin.x, playerPos.y, coin.z));
			if (dist < 6.0f) {
				glm::vec3 dir = glm::normalize(playerPos - glm::vec3(coin.x, playerPos.y, coin.z));
				coin.x += dir.x * 0.2f;
				coin.z += dir.z * 0.2f;
			}
		}
	}

	// 7. [아이템] 획득 로직
	for (auto& item : items) {
		if (item.isCollected) continue;
		if (std::abs(item.z - playerPos.z) < 0.5f && std::abs(item.x - playerPos.x) < 0.5f) {
			item.isCollected = true;
			PlaySound(TEXT("item.wav"), NULL, SND_ASYNC | SND_FILENAME);

			if (item.type == ITEM_POTION) {
				if (!isGiantMode && !isFlying) {
					isGiantMode = true;
					giantTimer = 8.0f; 
					spawnParticles(playerPos, glm::vec3(0.6f, 0.0f, 0.8f), 30, 1.5f); 
				}
				else {
					giantTimer += 5.0f;
				}

			}
			else if (item.type == ITEM_SHIELD) {
				hasShield = true;
				spawnParticles(playerPos, glm::vec3(0.0f, 0.5f, 1.0f), 20, 1.0f);
			}
			else if (item.type == ITEM_MAGNET) {
				isMagnetActive = true;
				magnetTimer = 10.0f;
				spawnParticles(playerPos, glm::vec3(1.0f, 0.0f, 0.0f), 20, 1.0f);
			}
			else if (item.type == ITEM_CLOCK) {
				isSlowActive = true;
				slowTimer = 5.0f;
				spawnParticles(playerPos, glm::vec3(0.0f, 1.0f, 1.0f), 20, 1.0f);
			}
		}
	}

	// 바람 이벤트 로직
	if (isWindActive) {
		windTimer -= 0.016f;

		// 플레이어 밀림 로직
		if (!isMoving && !isFlying && !isLanding) {

			float nextX = playerPos.x + windForce;
			int currentZ = (int)std::round(playerPos.z);
			float bodyOffset = (windForce > 0) ? 0.4f : -0.4f;

			int checkGridX = (int)std::round(nextX + bodyOffset);

			bool isOnRiver = (mapType.count(currentZ) && mapType[currentZ] == 2);

			bool isTreeBlocking = isTreeAt(checkGridX, currentZ);

			if (!isOnRiver && !isTreeBlocking) {
				playerPos.x += windForce;
				playerStartPos.x += windForce;
				playerTargetPos.x += windForce;
			}
		}

		// 파티클 효과 
		if (rand() % 10 < 3) {
			float spawnX = (windForce > 0) ? -20.0f : 20.0f;
			float spawnZ = playerPos.z + (rand() % 40 - 20);
			float spawnY = (rand() % 10) + 2.0f;

			glm::vec3 pPos(spawnX, spawnY, spawnZ);
			glm::vec3 pColor(0.9f, 0.95f, 1.0f);

			Particle p;
			p.position = pPos;
			p.velocity = glm::vec3(windForce * 50.0f, -0.1f, 0.0f);
			p.color = pColor;
			p.scale = 0.1f;
			p.life = 0.8f;
			particles.push_back(p);
		}

		// 바람 종료 체크
		if (windTimer <= 0.0f) {
			isWindActive = false;
		}
	}
	else { 
		if (!isFlying && !isEventActive && !isNightMode && rand() % 800 < 1) {
			isWindActive = true;
			windTimer = WIND_DURATION;
			windForce = (rand() % 2 == 0 ? 0.04f : -0.04f);
		}
	}

	if (isDashing) {
		const float DASH_SPEED = 0.5f;
		playerPos.z -= DASH_SPEED;
		dashTimer -= 0.016f;
		int currentZ = (int)std::round(playerPos.z);
		if (currentZ < minZ) {
			minZ = currentZ;
			score++;
		}
		if (dashTimer <= 0.0f) {
			isDashing = false;
			dashCooldownTimer = DASH_COOLDOWN;
			playerPos.x = (float)std::round(playerPos.x);
			playerPos.z = (float)std::round(playerPos.z);
			playerTargetPos = playerPos;
			recoveryTimer = 1.0f;
			isLandingSuccess = false;
		}

	}
	else if (isFlying) {
		playerPos.z -= FLY_SPEED; 
		playerPos.y = 3.0f + sin(flyTimer * 5.0f) * 0.5f;

		flyTimer -= 0.016f;

		int currentZ = (int)std::round(playerPos.z);
		if (currentZ < minZ) {
			minZ = currentZ;
			score++;
		}

		// 비행 종료
		if (flyTimer <= 0.0f) {
			isFlying = false;
			isLanding = true;


			birdStartPos = playerPos + glm::vec3(0.0f, 0.5f * 0.7f + 0.5f * 0.7f, 0.0f); 
			birdTargetPos = playerPos + glm::vec3(30.0f, FLY_HEIGHT + 10.0f, -50.0f); 

			playerStartPos = playerPos;
			playerTargetPos = glm::vec3((float)std::round(playerPos.x), 0.5f, (float)std::round(playerPos.z));
		}
	}
	else if (isLanding) {
		landingTime += 0.016f;
		float t = glm::clamp(landingTime / LANDING_DURATION, 0.0f, 1.0f);
		float smoothT = sin(t * 3.14159f / 2.0f);

		playerPos.x = glm::mix(playerStartPos.x, playerTargetPos.x, t);
		playerPos.z = glm::mix(playerStartPos.z, playerTargetPos.z, t);

		playerPos.y = glm::mix(playerStartPos.y, playerTargetPos.y, t);

		if (t >= 1.0f) {
			isLanding = false;
			playerPos = playerTargetPos; 
			isBirdLeaving = true;
			birdLeaveTime = 0.0f;
			recoveryTimer = 2.0f; 
			isLandingSuccess = true;
		}
	}
	else if (isEventActive) {
		eventDuration -= 0.016f;

		// 성공 조건:
		if (eventProgress >= 1.0f) {
			isEventActive = false;
			isFlying = true;
			flyTimer = FLY_DURATION;
			playerPos.y = FLY_HEIGHT;
			spawnParticles(playerPos + glm::vec3(0, 1.0f, 0), glm::vec3(1.0f, 0.5f, 0.0f), 50, 1.5f);
		}
		// 실패 조건
		else if (eventDuration <= 0.0f) {
			isEventActive = false;
			eventProgress = 0.0f;
		}
	}
	// 일반 게임 플레이 (이동, 충돌, 아이템)
	else {
		// 1. 대쉬 자동 발동 조건 체크
		if (!isMoving && dashCooldownTimer <= 0.0f && coinCount >= DASH_COST) {
			coinCount -= DASH_COST;
			isDashing = true;
			dashTimer = DASH_DURATION;
			spawnParticles(playerPos, glm::vec3(1.0f, 0.5f, 0.0f), 20, 0.8f);
		}

		// 2. 랜덤 이벤트 발생 조건 체크
		if (scoreTargetReached && score > lastEventScore && !isMoving) {
			isEventActive = true;
			eventProgress = 0.0f;
			requiredTaps = 0;
			lastEventScore = score;
		}

		// 3. 새 퇴장 애니메이션
		if (isBirdLeaving) {
			birdLeaveTime += 0.016f;
			float t = glm::clamp(birdLeaveTime / BIRD_LEAVE_DURATION, 0.0f, 1.0f);
			if (t >= 1.0f) {
				isBirdLeaving = false;
			}
		}

		// 통나무 이동 및 탑승
		for (auto& logObj : logs) {
			// 슬로우 효과 적용
			logObj.x += logObj.speed * globalSpeedRate;
			if (logObj.speed > 0 && logObj.x > 20.0f) logObj.x = -20.0f;
			else if (logObj.speed < 0 && logObj.x < -20.0f) logObj.x = 20.0f;

			bool onLog = (std::abs(playerPos.z - logObj.z) < 0.1f) &&
				(playerPos.x >= logObj.x - logObj.width / 2.0f - 0.3f) &&
				(playerPos.x <= logObj.x + logObj.width / 2.0f + 0.3f);

			if (!isMoving && onLog) {
				playerPos.x += logObj.speed * globalSpeedRate;
				playerTargetPos.x += logObj.speed * globalSpeedRate;
				playerStartPos.x += logObj.speed * globalSpeedRate;
				restingY = 1.1f;
			}
		}

		// [충돌 검사 시작]
		bool isDead = false;
		bool isInvincible = isDashing || (recoveryTimer > 0.0f) || hasShield || isGiantMode || isGodMode;

		// 자동차 충돌
		for (auto& car : cars) {
			//  슬로우 효과 적용
			car.x += car.speed * globalSpeedRate;
			if (car.x > 18.0f && car.speed > 0) car.x = -18.0f;
			if (car.x < -18.0f && car.speed < 0) car.x = 18.0f;

			// 충돌 체크
			bool hit = (abs(car.z - playerPos.z) < 0.2f && abs(car.x - playerPos.x) < 0.8f) ||
				(abs(car.z - playerPos.z) < 0.08f && abs(car.x - playerPos.x) < 1.2f);

			if (hit) {
				if (isGodMode) continue;

				if (isGiantMode) {
					// 파티클 효과
					spawnParticles(glm::vec3(car.x, 0.5f, car.z), car.color, 30, 1.5f);

					if (car.x > playerPos.x) car.x += 30.0f; 
					else car.x -= 30.0f; 

					score += 5;
					shakeTimer = 0.1f; shakeMagnitude = 0.2f; 
				}
				else if (hasShield) {
					hasShield = false; 
					spawnParticles(playerPos, glm::vec3(0.5f, 0.5f, 1.0f), 30, 2.0f); 
					car.x = (car.speed > 0) ? 20.0f : -20.0f;
				}
				else {
					isDead = true;
				}
			}
		}

		// 익사(물) 판정
		int pZ = (int)std::round(playerPos.z);
		if (!isMoving && mapType.count(pZ) && mapType[pZ] == 2) {
			bool safe = false;
			if (isGiantMode) {
				safe = true;
				restingY = 0.5f; 
			}

			else {
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
				if (playerPos.x < -16.0f || playerPos.x > 16.0f) isDead = true;
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
					!isFlying && !isLanding &&
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
							if (isGodMode) {
							}
							else if (isGiantMode) {
								spawnParticles(playerPos, glm::vec3(1.0f, 0.0f, 0.0f), 50, 2.0f); 
								shakeTimer = 0.3f; shakeMagnitude = 0.5f; 
								score += 10; 
							}
							else {
								isDead = true;
							}
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

		if (isDead) {

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

		// 코인 로직
		for (auto& coin : coins) {
			if (coin.isCollected) continue;
			if (std::abs(coin.z - playerZ) < playerSize && std::abs(coin.x - playerX) < playerSize) {
				coin.isCollected = true;
				coinCount++;
				PlaySound(TEXT("coin.wav"), NULL, SND_ASYNC | SND_FILENAME);
				spawnParticles(glm::vec3(coin.x, 0.5f, coin.z), glm::vec3(1.0f, 0.9f, 0.0f), 10, 0.3f);
			}
		}

		// 점프 애니메이션
		if (isMoving) {
			moveTime += 0.016f;
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

		// 점프 애니메이션 
		if (isMoving && !isDashing) {
			moveTime += 0.016f;
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
		if (!isMoving && !isDashing) {
			playerPos.y = restingY;
			playerStartPos.y = restingY;
		}
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