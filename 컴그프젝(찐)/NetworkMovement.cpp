#include "MoveClient.h"

#include "NetworkMovement.h"
#include "Global.h"

namespace
{
    MoveClient network;

    Protocol::Direction pendingDirection =
        Protocol::Direction::Up;

    bool running = false;
}

bool StartNetworkMovement()
{
    if (!network.Start("127.0.0.1", 9000))
        return false;

    // 서버의 최초 좌표 (x=0, z=0)와 맞춘다.
    playerPos = glm::vec3(
        0.0f,
        0.5f,
        0.0f
    );

    playerStartPos = playerPos;
    playerTargetPos = playerPos;

    playerRotation = 0.0f;

    isMoving = false;
    moveTime = 0.0f;

    // 이동 연결 테스트에서는 인트로를 건너뛴다.
    introState = 0;
    isGameOver = false;

    running = true;

    return true;
}

void StopNetworkMovement()
{
    running = false;
    network.Stop();
}

void NetworkSpecialKeyboard(
    int key,
    int,
    int)
{
    if (!running)
        return;

    // 점프 중이거나 서버 응답 대기 중이면 입력을 받지 않는다.
    if (isMoving || network.Pending())
        return;

    Protocol::Direction direction;

    switch (key) {
    case GLUT_KEY_UP:
        direction = Protocol::Direction::Up;
        break;

    case GLUT_KEY_DOWN:
        direction = Protocol::Direction::Down;
        break;

    case GLUT_KEY_LEFT:
        direction = Protocol::Direction::Left;
        break;

    case GLUT_KEY_RIGHT:
        direction = Protocol::Direction::Right;
        break;

    default:
        return;
    }

    if (network.Request(direction)) {
        pendingDirection = direction;
    }

    // 여기서는 playerTargetPos, isMoving을 변경하지 않는다.
}

void NetworkKeyboard(
    unsigned char key,
    int,
    int)
{
    if (key == 'q' || key == 'Q' || key == 27) {
        StopNetworkMovement();
        glutLeaveMainLoop();
    }
}

void NetworkTimer(int)
{
    if (!running)
        return;

    // 기다리지 않고, 지금 가능한 송수신만 진행한다.
    network.Pump();

    if (!network.Connected()) {
        std::printf(
            "[game] disconnected. Restart the game to reconnect.\n"
        );

        running = false;

        glutPostRedisplay();
        return;
    }

    Protocol::MoveResult result;

    if (network.Poll(result)) {
        std::printf(
            "[game] input=%u target=(%d,%d) status=%u\n",
            static_cast<unsigned>(result.inputSequence),
            static_cast<int>(result.x),
            static_cast<int>(result.z),
            static_cast<unsigned>(result.status)
        );

        if (result.status ==
            Protocol::MoveStatus::Accepted) {

            // 서버가 승인한 위치로 애니메이션을 시작한다.
            playerStartPos = playerPos;

            playerTargetPos = glm::vec3(
                static_cast<float>(result.x),
                0.5f,
                static_cast<float>(result.z)
            );

            switch (pendingDirection) {
            case Protocol::Direction::Up:
                playerRotation = 0.0f;
                break;

            case Protocol::Direction::Down:
                playerRotation = 180.0f;
                break;

            case Protocol::Direction::Left:
                playerRotation = 90.0f;
                break;

            case Protocol::Direction::Right:
                playerRotation = -90.0f;
                break;
            }

            moveTime = 0.0f;
            isMoving = true;

            // 화면 표시용 거리 점수.
            // 최종 순위 계산은 이후 서버에서 처리해야 한다.
            if (result.z < minZ) {
                minZ = result.z;
                score = -minZ;
            }
        }
        else {
            std::printf("[game] move rejected\n");
        }
    }

    // 기존 게임과 같은 방식의 점프 애니메이션.
    if (isMoving) {
        moveTime += 0.016f;

        float t = glm::clamp(
            moveTime / MOVE_DURATION,
            0.0f,
            1.0f
        );

        playerPos = glm::mix(
            playerStartPos,
            playerTargetPos,
            t
        );

        float jumpHeight =
            JUMP_HEIGHT * 4.0f * t * (1.0f - t);

        playerPos.y = 0.5f + jumpHeight;

        if (t >= 1.0f) {
            playerPos = playerTargetPos;
            isMoving = false;
        }
    }

    glutPostRedisplay();

    glutTimerFunc(
        16,
        NetworkTimer,
        0
    );
}