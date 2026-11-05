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

void MatchCoordinator::DissolveReadyLobby(SessionId leaverId) {
	if (room.phase != ServicePhase::READY_LOBBY) { //레디방인지 확인
		return;
	}
	const auto it = std::find(room.members.begin(), room.members.end(), leaverId); //실존하는id?
	if (it == room.members.end()) {
		return;
	}
	room.queue.clear(); //3명이상 안받으니까
	for (SessionId memberId : room.members) { //탈주빼고 큐에 재등록
		if (memberId != leaverId) {
			room.queue.push_back(memberId);
		}
	}
	room.members.fill(0); //기존상태로 초기화
	room.ready.fill(false);
	++room.lobbyEpoch; //대기실 번호 변경
	room.phase = ServicePhase::MATCHING; //상태변경

	// 7. 통신 구현 시 남은 참가자에게 순서대로 전송
	// READY_LOBBY_STATE(DISSOLVED) → QUEUE_STATE
}

void MatchCoordinator::CancelLoading(SessionId leaverId) {
	if (room.phase != ServicePhase::LOADING) { //로딩중인지 확인
		return;
	}
	const auto it = std::find(room.members.begin(), room.members.end(), leaverId); //실존id
	if (it == room.members.end()) {
		return;
	}
	room.queue.clear();
	for (SessionId memberId : room.members) { //이탈자 제외 큐 복귀
		if (memberId != leaverId) {
			room.queue.push_back(memberId);
		}
	}
	room.members.fill(0); //기존상태로 초기화
	room.ready.fill(false);
	room.loaded.fill(false);
	// 5. 월드 구현 시 초기 월드와 관련 입력·예약 작업 폐기

	room.phase = ServicePhase::MATCHING; //상태변경
	// 7. 통신 구현 시 남은 참가자에게 순서대로 전송
	// ROOM_CLOSE(CANCEL_LOADING) → QUEUE_STATE
}

void RoundController::BeginRound(bool rematch) { // 첫경기인지 재경기인지 구분
	if (rematch) { //재경기인지 확인
		if (room.phase != ServicePhase::RESULT) { // 재경기: 호출 전에 연결된 3명의 전원 동의 확인 필요
			return;
		}
	}
	else { // 첫 경기: 준비 대기실에서만 시작 가능
		if (room.phase != ServicePhase::READY_LOBBY) {
			return;
		}
		
		const bool allReady = std::all_of( // 첫 경기는 전원 준비 필요
			room.ready.begin(), room.ready.end(),
			[](bool value) { return value; });
		if (!allReady) {
			return;
		}
	}
	++room.roundId; // 새로운 라운드 번호 발급
	room.ready.fill(false); // 이전 준비·로딩 상태 초기화
	room.loaded.fill(false);

	// 5. 월드·플레이어 구현 시 연결
	// - 새로운 초기 월드 생성
	// - 참가자 3명의 위치·생존 상태 초기화
	// - 아이템 슬롯·효과 초기화
	// - 거리·생존 시간·입력 기록 초기화
	for (std::size_t i = 0; i < room.players.size(); ++i) {
		room.players[i] = PlayerState{};
		room.players[i].playerId = room.members[i];
		room.players[i].alive = true;

		room.records[i] = RoundRecord{};
		room.records[i].playerId = room.players[i].playerId;
	}

	// 6. 로딩 단계로 전환
	room.phase = ServicePhase::LOADING;

	// 7. 통신 구현 시 새 roundId의 INIT 전송
}

bool RoundController::MarkLoaded(PlayerId id, RoundId roundId) //로딩완료기록, 플레이로 전환
{
	if (room.phase != ServicePhase::LOADING) { //로딩 단계에서만 처리
		return false;
	}
	if (roundId != room.roundId) { //현재 라운드의 완료 요청인지 확인
		return false;
	}
	const auto it = std::find( //실제 참가자인지 확인
		room.members.begin(), room.members.end(), id);
	if (it == room.members.end()) {
		return false;
	}
	const auto index = static_cast<std::size_t>(it - room.members.begin());
	if (room.loaded[index]) { //중복 완료 요청은 무시
		return false;
	}
	room.loaded[index] = true; //해당 참가자의 로딩 완료 기록
	const bool allLoaded = std::all_of( //전원 로딩 완료 여부 확인
		room.loaded.begin(), room.loaded.end(),
		[](bool value) { return value; });
	if (allLoaded) {
		// 7. 서버 틱 구현 시 시작 틱·생존 시간 기준 설정

		room.phase = ServicePhase::PLAYING;

		// 8. 통신 구현 시 전체 참가자에게 START 전송
	}
	return true;
}

bool RoundController::ShouldFinish() const { //세명 다 죽었는지 확인
	if (room.phase != ServicePhase::PLAYING) { // 경기 중에만 종료 조건 검사
		return false;
	}
	return std::none_of( // 살아 있는 참가자가 한 명도 없으면 종료
		room.players.begin(), room.players.end(),
		[](const PlayerState& player) {
			return player.alive;
		});
}

ResultData RoundController::BuildResult(const RankConfig& config)
{
	ResultData result{};
	result.roundId = room.roundId;

	for (std::size_t i = 0; i < room.records.size(); ++i) {
		const RoundRecord& record = room.records[i];
		ResultEntry& entry = result.entries[i];

		entry.playerId = record.playerId;
		entry.distanceLanes = record.maxForwardLanes;
		entry.survivalTicks = record.survivalTicks;
		entry.departed = record.departed;
		entry.rank = 0; // 순위 미산정
	}

	// 순위 정책 구현 시 config를 RankPolicy에 전달
	(void)config;

	// 세션 관리 연결 시 현재 연결 참가자 수에 따라 설정
	// 연결 3명: rematchAllowed=true, roomClosing=false
	// 연결 1~2명: rematchAllowed=false, roomClosing=true

	return result;
}

void RematchController::SetVote(PlayerId p, bool agree)
{
	if (room.phase != ServicePhase::RESULT) { // 단계 확인
		return;
	}
	const bool hasDeparted = std::any_of( //참가자 중 이탈자가 있으면 재경기 불가
		room.players.begin(), room.players.end(),
		[](const PlayerState& player) {
			return player.departed;
		});

	if (hasDeparted) {
		return;
	}

	// 세션 관리 구현 시 현재 연결 참가자 3명인지 추가 검사

	const auto it = std::find_if( // 옳은 참가자인지 확인
		room.players.begin(), room.players.end(),
		[p](const PlayerState& player) {
			return player.playerId == p;
		});
	if (it == room.players.end()) {
		return;
	}

	const auto index = static_cast<std::size_t>(it - room.players.begin());

	room.rematchVotes[index] = agree ? RematchVote::AGREE : RematchVote::DISAGREE;

	// 5. 통신 구현 시 REMATCH_STATE 전송

	// 6. 이후 Evaluate()로 재경기·종료·대기 판단 연결
}

RematchDecision RematchController::Evaluate() const {
	if (room.phase != ServicePhase::RESULT) { //결과 단계인지 확인
		return RematchDecision::WAIT;
	}
	const bool hasDeparted = std::any_of( //이탈자가 있으면 종료
		room.players.begin(), room.players.end(),
		[](const PlayerState& player) {
			return player.departed;
		});
	if (hasDeparted) {
		return RematchDecision::CLOSE;
	}

	// 세션 관리 구현 시 현재 연결 참가자가 3명 미만이면 CLOSE

	const bool hasDisagree = std::any_of( //한명이라도 비동의하면 종료
		room.rematchVotes.begin(), room.rematchVotes.end(),
		[](RematchVote vote) {
			return vote == RematchVote::DISAGREE;
		});
	if (hasDisagree) {
		return RematchDecision::CLOSE;
	}
	const bool allAgree = std::all_of( //전원 동의하면 재경기
		room.rematchVotes.begin(), room.rematchVotes.end(),
		[](RematchVote vote) {
			return vote == RematchVote::AGREE;
		});
	if (allAgree) {
		return RematchDecision::RESTART;
	}
	return RematchDecision::WAIT; //미응답자가 있으면 대기
}
