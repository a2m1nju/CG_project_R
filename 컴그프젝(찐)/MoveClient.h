#pragma once

#include "MoveProtocol.h"

#include <ws2tcpip.h>
#include <chrono>
#include <cstdio>

class MoveClient
{
public:
    MoveClient() = default;

    ~MoveClient()
    {
        Stop();
    }

    MoveClient(const MoveClient&) = delete;
    MoveClient& operator=(const MoveClient&) = delete;

    bool Start(
        const char* ip = "127.0.0.1",
        uint16_t port = 9000)
    {
        Stop();

        WSADATA wsa{};

        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return false;

        wsaStarted_ = true;

        socket_ = socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

        if (socket_ == INVALID_SOCKET) {
            Stop();
            return false;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);

        if (inet_pton(
            AF_INET,
            ip,
            &address.sin_addr) != 1) {

            Stop();
            return false;
        }

        // 최초 접속은 게임 루프를 시작하기 전에 수행한다.
        if (connect(
            socket_,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == SOCKET_ERROR) {

            std::printf(
                "[client] connect failed: %d\n",
                WSAGetLastError()
            );

            Stop();
            return false;
        }

        // 이후 게임 중 송수신은 기다리지 않는 방식으로 처리한다.
        u_long nonblocking = 1;

        if (ioctlsocket(
            socket_,
            FIONBIO,
            &nonblocking) == SOCKET_ERROR) {

            Stop();
            return false;
        }

        packetSequence_ = 0;
        inputSequence_ = 0;
        lastResponseSequence_ = 0;

        std::printf("[client] connected\n");

        return true;
    }

    void Stop()
    {
        if (socket_ != INVALID_SOCKET) {
            closesocket(socket_);
            socket_ = INVALID_SOCKET;
        }

        if (wsaStarted_) {
            WSACleanup();
            wsaStarted_ = false;
        }

        pending_ = false;
        ready_ = false;

        sent_ = 0;
        received_ = 0;
    }

    bool Connected() const
    {
        return socket_ != INVALID_SOCKET;
    }

    bool Pending() const
    {
        return pending_;
    }

    bool Request(Protocol::Direction direction)
    {
        // 이 실습에서는 요청 하나의 응답을 확인한 뒤
        // 다음 요청을 보낸다.
        if (!Connected() || pending_)
            return false;

        outgoing_ = Protocol::MakeMovePacket(
            ++packetSequence_,
            Protocol::TEST_ROUND_ID,
            ++inputSequence_,
            direction
        );

        pending_ = true;
        ready_ = false;

        sent_ = 0;
        received_ = 0;

        requestStarted_ =
            std::chrono::steady_clock::now();

        return true;
    }

    void Pump()
    {
        if (!Connected() || !pending_ || ready_)
            return;

        auto elapsed =
            std::chrono::steady_clock::now()
            - requestStarted_;

        if (elapsed > std::chrono::seconds(5)) {
            Fail("response timeout");
            return;
        }

        // 요청 17바이트를 끝까지 전송한다.
        while (sent_ <
            static_cast<int>(outgoing_.size())) {

            int result = send(
                socket_,
                reinterpret_cast<const char*>(
                    outgoing_.data() + sent_),
                static_cast<int>(outgoing_.size()) - sent_,
                0
            );

            if (result == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                    return;

                Fail("send failed");
                return;
            }

            if (result == 0) {
                Fail("send returned zero");
                return;
            }

            sent_ += result;
        }

        // 응답 25바이트를 모은다.
        while (received_ <
            static_cast<int>(incoming_.size())) {

            int result = recv(
                socket_,
                reinterpret_cast<char*>(
                    incoming_.data() + received_),
                static_cast<int>(incoming_.size()) - received_,
                0
            );

            if (result == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                    return;

                Fail("receive failed");
                return;
            }

            if (result == 0) {
                Fail("server closed");
                return;
            }

            received_ += result;
        }

        uint32_t responseSequence =
            Protocol::Get32(incoming_.data() + 4);

        if (!Protocol::ReadMoveResult(
            incoming_,
            result_)) {

            Fail("invalid response");
            return;
        }

        if (result_.inputSequence != inputSequence_) {
            Fail("input sequence mismatch");
            return;
        }

        if (responseSequence <= lastResponseSequence_) {
            Fail("invalid response sequence");
            return;
        }

        lastResponseSequence_ = responseSequence;
        ready_ = true;
    }

    bool Poll(Protocol::MoveResult& result)
    {
        if (!ready_)
            return false;

        result = result_;

        ready_ = false;
        pending_ = false;

        return true;
    }

private:
    void Fail(const char* message)
    {
        std::printf("[client] %s\n", message);
        Stop();
    }

    SOCKET socket_ = INVALID_SOCKET;

    bool wsaStarted_ = false;
    bool pending_ = false;
    bool ready_ = false;

    uint32_t packetSequence_ = 0;
    uint32_t inputSequence_ = 0;
    uint32_t lastResponseSequence_ = 0;

    int sent_ = 0;
    int received_ = 0;

    Protocol::MovePacket outgoing_{};
    Protocol::ResultPacket incoming_{};
    Protocol::MoveResult result_{};

    std::chrono::steady_clock::time_point
        requestStarted_{};
};
