#include "ServerApp.h"
#include <iostream>
#include <string>
#include <thread>

bool ServerApp::Initialize(std::uint16_t port) {
    if (initialized) { //중복 초기화 방지
        return false;
    }

    if (port == 0) { //고정 접속 포트로 사용할 값인지 확인
        return false;
    }

    if (!network.Initialize(port)) {
        return false;
    }

    this->port = port;
    room = RoomState{}; //방 상태 초기화

    // Network loop owns transport and fixed-step movement state.
    // GameThread 구현 시 접속 대기 스레드보다 먼저 시작
    // 중간 실패 시 생성된 자원만 정리하고 false 반환

    initialized = true;
    return true;
}

void ServerApp::Run() {
    if (!initialized) { //초기화 완료 후 실행
        return;
    }

    std::cout << "Server listening on port " << port << ".\n";
    std::cout << "Press Enter to stop." << std::endl;
    std::thread acceptThread(&ServerNetwork::RunAcceptLoop, &network);
    std::string input;
    std::getline(std::cin, input);
    network.StopAccepting();
    acceptThread.join();
}

void ServerApp::Shutdown() {
    if (!initialized) { //중복 종료 방지
        return;
    }

    // 스레드 구현 시 네트워크 종료 전에 정리
    network.Shutdown();
    room = RoomState{};
    port = 0;
    initialized = false;
}
