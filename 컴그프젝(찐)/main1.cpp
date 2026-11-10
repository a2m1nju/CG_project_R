#include "ClientNetwork.h"
#include "Common.h"
#include "Global.h"
#include "Shader.h"
#include "Renderer.h"
#include "Logic.h"
#include "Input.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "freeglut.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "glew32.lib")
#pragma warning(disable: 4711 4710 4100)

void Reshape(int w, int h)
{
    glViewport(0, 0, w, h);
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

    mciSendString(TEXT("play bgm.wav"), NULL, 0, NULL);

    return true;
}

namespace {
    void PollServerConnection(int value)
    {
        if (activeClientNetwork == nullptr || glutGetWindow() == 0) {
            return;
        }

        if (activeClientNetwork->PollDisconnected()) {
            std::cout << "Server disconnected." << std::endl;
            return;
        }

        glutTimerFunc(16, PollServerConnection, value);
    }
}

int main(int argc, char** argv)
{
    if (argc != 2 || argv[1][0] == '\0') {
        if (argc <= 2) {
            std::cerr << "Server IP address is required.\n";
        }
        else {
            std::cerr << "Expected exactly one argument: the server IP address.\n";
        }
        std::cerr << "Usage: \"" << argv[0] << "\" <server-ip>\n";
        return EXIT_FAILURE;
    }

    ClientNetwork network;
    if (!network.Connect(argv[1], 9000)) {
        return EXIT_FAILURE;
    }

    std::cout << "Connected to " << argv[1] << ":9000\n";

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
    glutTimerFunc(16, timer, 0);

    activeClientNetwork = &network;
    glutTimerFunc(16, PollServerConnection, 0);

    glutMainLoop();
    activeClientNetwork = nullptr;
    return EXIT_SUCCESS;
}