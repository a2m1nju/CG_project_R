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

int main(int argc, char** argv)
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
        exit(EXIT_FAILURE);
    }

    make_vertexShaders();
    make_fragmentShaders();
    shaderProgramID = make_shaderProgram();
    loadDepthShader();

    initGame();
    initFont("Cafe24PROUP.ttf", 60.0f);

    glEnable(GL_DEPTH_TEST);

    mciSendString(TEXT("play bgm.wav"), NULL, 0, NULL);

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutSpecialFunc(specialKeyboard);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, timer, 0);

    glutMainLoop();
    return 0;
}