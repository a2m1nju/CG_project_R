#include "ClientNetwork.h"
#include "Common.h"
#include "Global.h"
#include "Shader.h"
#include "Renderer.h"
#include "Logic.h"
#include "Input.h"
#include "GameViewport.h"

#include <algorithm>
#include <string>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "freeglut.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "glew32.lib")
#pragma warning(disable: 4711 4710 4100)

namespace {
    enum class BackgroundTrack { Default, Sans };

    BackgroundTrack currentBackgroundTrack = BackgroundTrack::Default;
    bool backgroundMusicOpen = false;

    void PlayBackgroundTrack(BackgroundTrack track)
    {
        if (backgroundMusicOpen && currentBackgroundTrack == track) {
            return;
        }

        if (backgroundMusicOpen) {
            mciSendStringA("close game_bgm", nullptr, 0, nullptr);
            backgroundMusicOpen = false;
        }

        const char* filename = track == BackgroundTrack::Sans
            ? "MEGALOVANIA_BGM.wav" : "bgm.wav";
        const std::string command = std::string("open \"") + filename +
            "\" type waveaudio alias game_bgm";
        if (mciSendStringA(command.c_str(), nullptr, 0, nullptr) != 0) {
            return;
        }
        if (mciSendStringA("play game_bgm from 0", nullptr, 0, nullptr) != 0) {
            mciSendStringA("close game_bgm", nullptr, 0, nullptr);
            return;
        }

        currentBackgroundTrack = track;
        backgroundMusicOpen = true;
    }

    void KeepBackgroundMusicPlaying()
    {
        if (!backgroundMusicOpen ||
            currentBackgroundTrack != BackgroundTrack::Sans) {
            return;
        }

        static int nextCheckMs = 0;
        const int nowMs = glutGet(GLUT_ELAPSED_TIME);
        if (nowMs < nextCheckMs) {
            return;
        }
        nextCheckMs = nowMs + 500;

        char mode[32]{};
        if (mciSendStringA("status game_bgm mode", mode, sizeof(mode), nullptr) == 0 &&
            std::string(mode) == "stopped") {
            mciSendStringA("play game_bgm from 0", nullptr, 0, nullptr);
        }
    }

    bool AllPlayersSelectedSans(const LobbyView& lobby)
    {
        return lobby.members.size() == 3 &&
            std::all_of(lobby.members.begin(), lobby.members.end(),
                [](const LobbyMemberView& member) {
                    return member.characterId == CharacterId::Sans;
                });
    }
}

void Reshape(int w, int h)
{
    const GameViewport viewport = FitGameViewport(w, h);
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glutPostRedisplay();
}

bool InitializeClient(int& argc, char** argv)
{
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(1280, 960);
    glutCreateWindow("길건너 친구들 모작");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Unable to initialize GLEW" << std::endl;
        return false;
    }

    make_vertexShaders();
    make_fragmentShaders();
    shaderProgramID = make_shaderProgram();
    loadDepthShader();

    initGame();
    initFont("Cafe24PROUP.ttf", 60.0f);

    glEnable(GL_DEPTH_TEST);

    PlayBackgroundTrack(BackgroundTrack::Default);

    return true;
}

namespace {
    void PollServerConnection(int value)
    {
        if (activeClientNetwork == nullptr ||
            glutGetWindow() == 0)
        {
            return;
        }

        if (activeClientNetwork->PollDisconnected()) {
            std::cout << "Server disconnected.\n";
        }
        const auto& lobby = activeClientNetwork->GetLobby();
        static bool wasPlaying = false;
        const bool isPlaying = lobby.stage == LobbyStage::Playing;
        if (isPlaying && !wasPlaying) {
            PlayBackgroundTrack(AllPlayersSelectedSans(lobby)
                ? BackgroundTrack::Sans : BackgroundTrack::Default);
        }
        wasPlaying = isPlaying;
        KeepBackgroundMusicPlaying();
        ClientScreen nextScreen = ClientScreen::Title;
        switch (lobby.stage) {
        case LobbyStage::Joining:
        case LobbyStage::Matching:
            nextScreen = ClientScreen::Matching;
            break;
        case LobbyStage::Loading:
        case LobbyStage::Ready:
            nextScreen = ClientScreen::Lobby;
            break;
        case LobbyStage::Playing:
            nextScreen = ClientScreen::Playing;
            break;
        default:
            nextScreen = ClientScreen::Title;
            break;
        }
        if (nextScreen == ClientScreen::Title &&
            clientScreen != ClientScreen::Title)
        {
            introState = 2;
            logoX = logoTargetX;
        }
        if (nextScreen != ClientScreen::Title) {
            introState = 0;
        }
        clientScreen = nextScreen;
        glutPostRedisplay();
        glutTimerFunc(16, PollServerConnection, value);
    }
}

int main(int argc, char** argv)
{
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [server-ip]\n";
        return EXIT_FAILURE;
    }

    ClientNetwork network;

    if (argc == 2) {
        if (!network.Connect(argv[1], 9000)) {
            return EXIT_FAILURE;
        }
        activeClientNetwork = &network;
        std::cout << "Connected to " << argv[1] << ":9000\n";
    }

    // GLUT에는 실행 파일명만 전달
    int glutArgc = 1;
    char* glutArgv[] = {argv[0], nullptr};

    if (!InitializeClient(glutArgc, glutArgv)) {
        return EXIT_FAILURE;
    }

    // 창을 닫으면 main()으로 돌아와 연결 정리
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutSpecialFunc(specialKeyboard);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutMotionFunc(mouseMotion);
    glutTimerFunc(16, timer, 0);

    if (argc == 2) {
        activeClientNetwork = &network;
        glutTimerFunc(16, PollServerConnection, 0);
    }

    glutMainLoop();
    if (backgroundMusicOpen) {
        mciSendStringA("close game_bgm", nullptr, 0, nullptr);
        backgroundMusicOpen = false;
    }
    activeClientNetwork = nullptr;
    return EXIT_SUCCESS;
}