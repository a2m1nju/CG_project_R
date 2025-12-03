#define _CRT_SECURE_NO_WARNINGS 

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <gl/glew.h>				
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h> 
#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "freeglut.lib")
#pragma warning(disable: 4711 4710 4100)

GLuint vao, vbo;
GLuint transLoc; 
glm::vec3 playerPos(0.0f, 0.5f, 0.0f);
glm::vec3 cameraPos(0.0f, 5.0f, 5.0f);

GLvoid drawScene(GLvoid);
GLvoid Reshape(int w, int h);
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
char* filetobuf(const char* file);

void initGame();
void specialKeyboard(int key, int x, int y);


GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(1280, 960);
	glutCreateWindow("2025 Coding Test-Computer Graphics");

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		std::cerr << "Unable to initialize GLEW" << std::endl;
		exit(EXIT_FAILURE);
	}
	else
		std::cout << "GLEW Initialized\n";

	make_vertexShaders();
	make_fragmentShaders();
	shaderProgramID = make_shaderProgram();

	initGame();

	glEnable(GL_DEPTH_TEST);

	glutDisplayFunc(drawScene);
	glutReshapeFunc(Reshape);
	glutSpecialFunc(specialKeyboard);

	glutMainLoop();
}


GLvoid drawScene()
{
	glClearColor(0.6f, 0.8f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(shaderProgramID);
	glBindVertexArray(vao);

	glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1280.0f / 960.0f, 0.1f, 100.0f);

	glm::vec3 target = playerPos;
	glm::vec3 cameraOffset(0.0f, 8.0f, 15.0); 
	glm::mat4 view = glm::lookAt(playerPos + cameraOffset, playerPos, glm::vec3(0, 1, 0));

	glm::mat4 PV = proj * view; 

	for (int z = -10; z < 20; ++z) {
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, glm::vec3(0.0f, -0.5f, (float)z)); 
		model = glm::scale(model, glm::vec3(15.0f, 1.0f, 1.0f)); 

		glm::mat4 MVP = PV * model;
		glUniformMatrix4fv(transLoc, 1, GL_FALSE, glm::value_ptr(MVP));

		if (z % 2 == 0) glVertexAttrib3f(1, 0.2f, 0.6f, 0.2f);
		else            glVertexAttrib3f(1, 0.3f, 0.8f, 0.3f);

		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	glm::mat4 pModel = glm::mat4(1.0f);
	pModel = glm::translate(pModel, playerPos);
	pModel = glm::scale(pModel, glm::vec3(0.8f, 0.8f, 0.8f)); 

	glm::mat4 pMVP = PV * pModel;
	glUniformMatrix4fv(transLoc, 1, GL_FALSE, glm::value_ptr(pMVP));

	glVertexAttrib3f(1, 1.0f, 0.5f, 0.0f);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	glutSwapBuffers();
}

void specialKeyboard(int key, int x, int y)
{
	float speed = 1.0f; // 이동 속도
	switch (key)
	{
	case GLUT_KEY_UP:    playerPos.z -= speed; break; // 위쪽 화살표
	case GLUT_KEY_DOWN:  playerPos.z += speed; break; // 아래쪽 화살표
	case GLUT_KEY_LEFT:  playerPos.x -= speed; break; // 왼쪽 화살표
	case GLUT_KEY_RIGHT: playerPos.x += speed; break; // 오른쪽 화살표
	}
	glutPostRedisplay(); // 화면 갱신
}
void initGame()
{
	transLoc = glGetUniformLocation(shaderProgramID, "trans");

	float vertices[] = {
		// 앞면
		-0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
		 0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
		// 뒷면
		-0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
		 0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
		// 왼쪽
		-0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
		-0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
		// 오른쪽
		0.5f,  0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
		// 위
		-0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
	     0.5f,  0.5f,  0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
	    // 아래
	    -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
		 0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f
	};

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

}

GLvoid Reshape(int w, int h)
{
	glViewport(0, 0, w, h);
}

char* filetobuf(const char* file)
{
	FILE* fptr;
	long length;
	char* buf;

	fptr = fopen(file, "rb");
	if (!fptr)
		return NULL;
	fseek(fptr, 0, SEEK_END);
	length = ftell(fptr);
	buf = (char*)malloc(length + 1);
	fseek(fptr, 0, SEEK_SET);
	fread(buf, length, 1, fptr);
	fclose(fptr);
	buf[length] = 0;

	return buf;
}

void make_vertexShaders()
{
	GLchar* vertexSource;

	vertexSource = filetobuf("vertex.glsl");
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);

	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, errorLog);
		std::cerr << "ERROR: vertex shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

void make_fragmentShaders()
{
	GLchar* fragmentSource;

	fragmentSource = filetobuf("fragment.glsl");
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);

	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, errorLog);
		std::cerr << "ERROR: frag_shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

GLuint make_shaderProgram()
{
	GLint result;
	GLchar* errorLog = NULL;
	GLuint shaderID;
	shaderID = glCreateProgram();

	glAttachShader(shaderID, vertexShader);
	glAttachShader(shaderID, fragmentShader);

	glLinkProgram(shaderID);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glGetProgramiv(shaderID, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(shaderID, 512, NULL, errorLog);
		std::cerr << "ERROR: shader program 연결 실패\n" << errorLog << std::endl;
		return false;
	}

	glUseProgram(shaderID);

	return shaderID;
}