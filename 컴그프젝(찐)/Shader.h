#pragma once
#include "Common.h"

char* filetobuf(const char* file);
void make_vertexShaders();
void make_fragmentShaders();
GLuint make_shaderProgram();
void loadDepthShader();