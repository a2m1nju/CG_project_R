#include "Common.h"
#include "Global.h"
#include "Shader.h"
#include "Renderer.h"
#include "Logic.h"
#include "Input.h"
#include "NetworkMovement.h"

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

int main(int argc, char** argv)
{
    srand((unsigned int)time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(1280, 960);
    glutCreateWindow("길건너 친구들 - 네트워크 이동 테스트");

    // 창을 닫으면 glutMainLoop에서 돌아오도록 한다.
    glutSetOption(
        GLUT_ACTION_ON_WINDOW_CLOSE,
        GLUT_ACTION_GLUTMAINLOOP_RETURNS
    );

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Unable to initialize GLEW" << std::endl;
        exit(EXIT_FAILURE);
    }

    make_vertexShaders();
    make_fragmentShaders();
    shaderProgramID = make_shaderProgram();
    loadDepthShader();

    initGame();
    initFont("Cafe24PROUP.ttf", 60.0f);

    // 게임 루프 시작 전에 서버에 접속한다.
    if (!StartNetworkMovement()) {
        std::printf(
            "Network connection failed. "
            "Start MovementTestServer first.\n"
        );

        return EXIT_FAILURE;
    }

    glutCloseFunc(StopNetworkMovement);

    glEnable(GL_DEPTH_TEST);

    mciSendString(TEXT("play bgm.wav"), NULL, 0, NULL);

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutSpecialFunc(NetworkSpecialKeyboard);
    glutKeyboardFunc(NetworkKeyboard);
    glutTimerFunc(16, NetworkTimer, 0);

    glutMainLoop();
    StopNetworkMovement();
    return 0;
}