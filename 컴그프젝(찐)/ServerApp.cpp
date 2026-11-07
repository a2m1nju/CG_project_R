#include "ServerApp.h"
#include <iostream>
#include <string>

ServerApp::ServerApp()
    : matchCoordinator(room),
    roundController(room),
    rematchController(room)
{
}

bool ServerApp::Initialize(std::uint16_t port) {
    if (initialized) { //중복 초기화 방지
        return false;
    }

    if (port == 0) { //고정 접속 포트로 사용할 값인지 확인
        return false;
    }

    this->port = port;
    room = RoomState{}; //방 상태 초기화

    // 네트워크 구현 시 Winsock·listen 초기화 연결
    // 스레드 구현 시 명령 큐·CS·이벤트 초기화 연결
    // 준비 완료 후 GameThread → AcceptThread 생성
    // 중간 실패 시 생성된 자원만 정리하고 false 반환

    initialized = true;
    return true;
}

void ServerApp::Run() {
    if (!initialized) { //초기화 완료 후 실행
        return;
    }

    // 네트워크 구현 전 서버 실행·종료 흐름 확인
    std::cout << "Server ready (configured port: " << port << ").\n";
    std::cout << "Press Enter to stop." << std::endl;
    std::string input;
    std::getline(std::cin, input);
}

void ServerApp::Shutdown() {
    if (!initialized) { //중복 종료 방지
        return;
    }

    // 스레드와 소켓 구현 시 방 상태 초기화 전에 정리
    room = RoomState{};
    port = 0;
    initialized = false;
}
