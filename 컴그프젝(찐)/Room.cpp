#include "Room.h"
#include <cmath>
#include <limits>

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

ReadyResult MatchCoordinator::SetReady(SessionId id, LobbyEpoch epoch, bool ready) {
	if (room.phase != ServicePhase::READY_LOBBY) { // 준비실인지 확인
		return ReadyResult::BadPhase;
	}
	if (epoch != room.lobbyEpoch) { // 이전 대기실 요청 차단
		return ReadyResult::Stale;
	}
	const auto it = std::find(room.members.begin(), room.members.end(), id); // 옳은 멤버인지 확인
	if (it == room.members.end()) {
		return ReadyResult::NotMember;
	}
	const auto index = static_cast<std::size_t>(it - room.members.begin()); // 개인 준비상태 변경
	room.ready[index] = ready;

	return ReadyResult::Accepted;
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
	// 준비 방이 해체되면 대기열 상태로 전환
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
	world.Players().fill(PlayerState{});
	// 5. 월드 구현 시 초기 월드와 관련 입력·예약 작업 폐기

	room.phase = ServicePhase::MATCHING; //상태변경
	// 7. 통신 구현 시 남은 참가자에게 순서대로 전송
	// 입장이 취소되어 방이 닫히면 대기열 상태로 전환
}

void MatchCoordinator::OnDisconnect(SessionId id) {
	if (room.phase == ServicePhase::MATCHING) { //큐에서 이탈자만 제거
		const auto it = std::find(room.queue.begin(), room.queue.end(), id);
		if (it != room.queue.end()) {
			room.queue.erase(it);
		}
		// 통신 구현 시 남은 참가자에게 QUEUE_STATE 전송
		return;
	}
	if (room.phase == ServicePhase::READY_LOBBY) { //준비실 해산
		DissolveReadyLobby(id);
		return;
	}
	if (room.phase == ServicePhase::LOADING) { //로딩 취소
		CancelLoading(id);
		return;
	}
	const auto it = std::find(room.members.begin(), room.members.end(), id); //실존id
	if (it == room.members.end()) {
		return;
	}
	const auto index = static_cast<std::size_t>(it - room.members.begin());
	if (world.Players()[index].departed) { //중복 이탈 처리 방지
		return;
	}
	if (room.phase == ServicePhase::PLAYING) {
		RoundController controller(room, world);

		controller.RecordDeath(
			id,
			room.recordTick,
			true
		);
		room.records[index].departed = true;

		// 기록 구현 시 alive 변경 전 생존 여부를 확인해 기록 동결
		// 이미 사망한 참가자의 사망 시각·생존 시간은 유지
		// 월드 구현 시 해당 캐릭터 제거
		// 통신 구현 시 PLAYER_LEFT·STATE 전송
		// 호출부에서 ShouldFinish()로 종료 조건 재검사
		return;
	}
	if (room.phase == ServicePhase::RESULT) { //결과 중 이탈하면 재경기 취소
		world.Players()[index].departed = true;
		room.records[index].departed = true;

		// 호출부에서 Evaluate() → CLOSE 확인 후 방 종료 처리
	}
}

bool RoundController::BeginRound(bool rematch) { // 첫경기인지 재경기인지 구분
	if (rematch) { //재경기인지 확인
		if (room.phase != ServicePhase::RESULT) { // 재경기: 호출 전에 연결된 3명의 전원 동의 확인 필요
			return false;
		}
	}
	else { // 첫 경기: 준비 대기실에서만 시작 가능
		if (room.phase != ServicePhase::READY_LOBBY) {
			return false;
		}
		
		const bool allReady = std::all_of( // 첫 경기는 전원 준비 필요
			room.ready.begin(), room.ready.end(),
			[](bool value) { return value; });
		if (!allReady) {
			return false;
		}
	}
	PrepareRound();
	return true;
}

void RoundController::PrepareRound()
{
	++room.roundId;

	room.ready.fill(false);
	room.loaded.fill(false);
	room.rematchVotes.fill(RematchVote::UNSET);
	room.rematchOpened.fill(false);
	room.rematchOpenedTick.fill(0);

	room.roundStartTick = 0;
	room.recordTick = 0;
	room.recordClockStarted = false;

	room.resultReady = false;
	room.result = ResultData{};

	for (std::size_t i = 0; i < world.Players().size(); ++i)
	{
		auto& player = world.Players()[i];

		// 참가자 식별 정보는 유지하고 경기 상태만 초기화
		player.playerId = room.members[i];
		player.alive = true;
		player.departed = false;

		auto& record = room.records[i];

		record = RoundRecord{};
		record.playerId = player.playerId;
		record.slot = static_cast<std::uint8_t>(i);
	}

	room.phase = ServicePhase::LOADING;
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

		// 8. 호출부에서 전체 참가자에게 START 전송
	}
	return true;
}

bool RoundController::ShouldFinish() const
{
	if (room.phase != ServicePhase::PLAYING) {
		return false;
	}

	return std::none_of(
		world.Players().begin(),
		world.Players().end(),
		[](const PlayerState& player) {
			return player.alive &&
				!player.departed;
		}
	);
}

ResultData RoundController::BuildResult(const RankConfig& config)
{
	ResultData result{};
	result.roundId = room.roundId;

	for (std::size_t i = 0; i < room.records.size(); ++i)
	{
		const auto& record = room.records[i];
		auto& entry = result.entries[i];

		entry.playerId = record.playerId;
		entry.slot = record.slot;
		entry.distanceLanes = record.maxForwardLanes;
		entry.survivalTicks = record.survivalTicks;
		entry.itemsUsed = record.itemsUsed;
		entry.departed = record.departed;
	}

	const auto better = [&](const ResultEntry& a, const ResultEntry& b)
		{
			if (config.departedLast &&
				a.departed != b.departed)
			{
				return !a.departed;
			}

			if (a.distanceLanes != b.distanceLanes) {
				return a.distanceLanes >
					b.distanceLanes;
			}

			return a.survivalTicks >
				b.survivalTicks;
		};

	std::sort(
		result.entries.begin(),
		result.entries.end(),
		[&](const ResultEntry& a,
			const ResultEntry& b)
		{
			if (better(a, b)) {
				return true;
			}

			if (better(b, a)) {
				return false;
			}

			// 동률 기록의 표시 순서만 슬롯으로 고정.
			// 공동 순위를 깨는 기준은 아님.
			return a.slot < b.slot;
		}
	);

	for (std::size_t i = 0; i < result.entries.size();++i)
	{
		auto& entry = result.entries[i];
		entry.rank = static_cast<std::uint8_t>(i + 1);
		if (config.jointTies && i > 0) {
			const auto& previous = result.entries[i - 1];

			if (!better(previous, entry) &&
				!better(entry, previous))
			{
				entry.rank = previous.rank;
			}
		}
	}

	return result;
}

void RematchController::SetVote(PlayerId p, bool agree)
{
	if (room.phase != ServicePhase::RESULT) { // 단계 확인
		return;
	}
	const bool hasDeparted = std::any_of( //참가자 중 이탈자가 있으면 재경기 불가
		world.Players().begin(), world.Players().end(),
		[](const PlayerState& player) {
			return player.departed;
		});

	if (hasDeparted) {
		return;
	}

	// 세션 관리 구현 시 현재 연결 참가자 3명인지 추가 검사

	const auto it = std::find_if( // 옳은 참가자인지 확인
		world.Players().begin(), world.Players().end(),
		[p](const PlayerState& player) {
			return player.playerId == p;
		});
	if (it == world.Players().end()) {
		return;
	}

	const auto index = static_cast<std::size_t>(it - world.Players().begin());

	room.rematchVotes[index] = agree ? RematchVote::AGREE : RematchVote::DISAGREE;

	// 5. 통신 구현 시 REMATCH_STATE 전송

	// 6. 이후 Evaluate()로 재경기·종료·대기 판단 연결
}

RematchDecision RematchController::Evaluate() const {
	if (room.phase != ServicePhase::RESULT) { //결과 단계인지 확인
		return RematchDecision::WAIT;
	}
	const bool hasDeparted = std::any_of( //이탈자가 있으면 종료
		world.Players().begin(), world.Players().end(),
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

void RoundController::StartRecordClock(std::uint64_t tick)
{
	if (room.phase != ServicePhase::PLAYING ||
		room.recordClockStarted)
	{
		return;
	}

	room.roundStartTick = tick;
	room.recordTick = tick;
	room.recordClockStarted = true;
}

void RoundController::RecordPosition(PlayerId id, float z)
{
	if (room.phase != ServicePhase::PLAYING ||
		!room.recordClockStarted ||
		!world.IsAlive(id) ||
		!std::isfinite(z))
	{
		return;
	}

	for (auto& record : room.records) {
		if (record.playerId != id ||
			record.frozen)
		{
			continue;
		}

		const double distance =
			(std::max)(
				0.0,
				static_cast<double>(record.startZ) - z
				);

		const auto lanes =
			static_cast<std::uint32_t>(
				(std::min)(
					1000000.0,
					std::floor(distance + .0001)
					)
				);

		record.maxForwardLanes =
			(std::max)(
				record.maxForwardLanes,
				lanes
				);

		return;
	}
}

void RoundController::RecordDeath(PlayerId id, std::uint64_t tick, bool departed)
{
	if (room.phase != ServicePhase::PLAYING) {
		return;
	}

	for (auto& record : room.records) {
		if (record.playerId != id || id == 0) {
			continue;
		}

		if (!record.frozen &&
			room.recordClockStarted)
		{
			const auto elapsed = tick >= room.roundStartTick ? tick - room.roundStartTick : 0;
			record.survivalTicks = static_cast<std::uint32_t>((std::min)(elapsed, static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)()
							)
						)
					);

			record.deathTick = tick;
			record.frozen = true;
		}

		record.departed = record.departed || departed;

		if (auto* player = world.FindPlayer(id)) {
			player->alive = false;

			player->departed =
				player->departed || departed;
		}

		return;
	}
}

void RoundController::RecordItemUsed(PlayerId id)
{
	if (room.phase != ServicePhase::PLAYING ||
		!room.recordClockStarted ||
		!world.IsAlive(id))
	{
		return;
	}

	for (auto& record : room.records) {
		if (record.playerId == id &&
			!record.frozen &&
			record.itemsUsed <
			(std::numeric_limits<
				std::uint32_t>::max)())
		{
			++record.itemsUsed;
			return;
		}
	}
}

void RoundController::UpdateSurvival(std::uint64_t tick)
{
	if (room.phase != ServicePhase::PLAYING ||
		!room.recordClockStarted)
	{
		return;
	}

	room.recordTick = tick;

	for (auto& record : room.records) {
		if (record.playerId == 0 ||
			record.frozen)
		{
			continue;
		}

		if (!world.IsAlive(record.playerId)) {
			const auto* player = world.FindPlayer(record.playerId);

			RecordDeath(record.playerId, tick, !player || player->departed);
		}
		else {
			const auto elapsed = tick >= room.roundStartTick ? tick - room.roundStartTick : 0;

			record.survivalTicks =
				static_cast<std::uint32_t>(
					(std::min)(
						elapsed,
						static_cast<std::uint64_t>(
							(std::numeric_limits<
								std::uint32_t>::max)()
							)
						)
					);
		}
	}
}