#include "Room.h"

JoinResult MatchCoordinator::Enqueue(SessionId id) { // 서버가 참가자의 JOIN 요청을 처리할 때 호출
	JoinResult result {};
	
	if (room.phase != ServicePhase::MATCHING) { // 매칭단계인지 확인
		result.code = 1;
		return result;
	}
	if (std::find(room.queue.begin(), room.queue.end(), id) != room.queue.end()) {
			result.code = 2;
			return result;
		}
	
	if (room.queue.size() >= 3) {
        result.code = 3;
        return result;
    }

	room.queue.push_back(id);
	result.code = 0;
	result.playerId = id;
	result.queuePosition = static_cast<std::uint16_t>(room.queue.size());
	return result; // 얘 호출한데서 다음단계(trypromote) 호출해야함.
}

void MatchCoordinator::TryPromote(){ // 준비실 구성
	if (room.phase != ServicePhase::MATCHING) { // 매칭단계인지 확인
		return;
	}
	if (room.queue.size() != 3) { // 3명인지 확인
		return;
	}
	for (std::size_t i = 0; i < room.members.size(); ++i) { // 대기실로 이동
		room.members[i] = room.queue[i];
	}
	room.queue.clear(); // 큐 비우기

	room.ready.fill(false); // 준비상태 초기화

	// 6. 새로운 준비 대기실 세대
	++room.lobbyEpoch;

	room.phase = ServicePhase::READY_LOBBY; // 준비실로 전환

	// 8. 통신 구현 시 READY_LOBBY_STATE 전송 연결
}

bool MatchCoordinator::SetReady(SessionId id, LobbyEpoch epoch, bool ready) {
	if (room.phase != ServicePhase::READY_LOBBY) { // 준비실인지 확인
		return false;
	}
	if (epoch != room.lobbyEpoch) { // 이전 대기실 요청 차단
		return false;
	}
	const auto it = std::find(room.members.begin(), room.members.end(), id); // 옳은 멤버인지 확인
	if (it == room.members.end()) {
		return false;
	}
	const auto index = static_cast<std::size_t>(it - room.members.begin()); // 개인 준비상태 변경
	room.ready[index] = ready;

	// 5. 통신 구현 시 전체 멤버에게
	// READY_LOBBY_STATE 전송

	const bool allReady = std::all_of( // 전원 준비여부 확인
		room.ready.begin(), room.ready.end(),
		[](bool value) { return value; });

	if (allReady) {
		// RoundController 구현 시 BeginRound(false) 연결
	}

	return true;
}