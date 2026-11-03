# 아이템 시험 서버

`ItemSystem`을 실제 TCP 접속으로 검증하기 위한 독립 실행 프로그램입니다. `127.0.0.1:19001`에서 한 클라이언트씩 처리하며, 서버 프로세스가 살아 있는 동안 상태를 유지합니다. 기존 OpenGL 게임은 아직 이 서버에 연결되지 않습니다.

## 빌드와 실행

Visual Studio에서 `ItemTestServer/ItemTestServer.vcxproj`를 열고 `Release|x64`로 빌드한 뒤 실행합니다. 또는 PowerShell에서:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'ItemTestServer\ItemTestServer.vcxproj' /p:Configuration=Release /p:Platform=x64
& '.\ItemTestServer\x64\Release\ItemTestServer.exe'
```

다른 터미널에서 `python ItemTestServer/client.py`를 실행하면 명령을 직접 입력할 수 있습니다. 자동 검증을 실행하려면 먼저 수동 실행한 서버를 종료하고 `python ItemTestServer/smoke_test.py`를 실행합니다. 스크립트가 서버를 직접 시작하고 종료합니다.

## 시험 명령

명령과 응답은 ASCII 한 줄씩이며, 숫자 아이템 종류는 `1=키 반전`, `2=블랙아웃`, `3=바나나`입니다.
강의자료의 가변 길이 데이터 전송 방식처럼 줄바꿈(`\n`)으로 메시지 끝을 구분합니다. 서버와 시험 클라이언트는 `send()`가 실제로 보낸 바이트 수를 확인하고, 남은 바이트가 있으면 다시 `send()`합니다.

```text
ADD_PLAYER 1
SPAWN 10 1 0 0
PICKUP 1 0 0
STATUS 1
RESET
QUIT
```

`TICK 2 2 0 0 1 1 0 0 1`은 플레이어 2와 1이 `(0,0)`에 동시에 있고 둘 다 살아 있는 상황입니다. 작은 플레이어 ID인 1이 먼저 획득합니다. `TICK`의 형식은 `TICK 인원 [플레이어ID x z 생존여부]...`이고 인원은 1~3명입니다.

이 텍스트 명령은 **시험 전용**입니다. 추진계획서의 12바이트 헤더를 쓰는 바이너리 패킷, 3인 동시 접속, 30Hz 틱, 클라이언트 동기화는 구현하지 않았습니다. 본 서버를 통합할 때는 월드가 확정한 위치를 `ItemSystem::ResolvePickups`에 전달하고 결과를 정식 패킷으로 송신해야 합니다.
