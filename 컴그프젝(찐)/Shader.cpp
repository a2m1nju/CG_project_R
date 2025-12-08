#include "Shader.h"
#include "Global.h" 

char* filetobuf(const char* file) {
    FILE* fptr;
    long length;
    char* buf;

    fptr = fopen(file, "rb");
    if (!fptr) {
        fprintf(stderr, "--------------------------------------------------------\n");
        fprintf(stderr, "[치명적 오류] 파일을 찾을 수 없습니다: %s\n", file);
        fprintf(stderr, "프로젝트 폴더에 해당 파일이 있는지 확인해주세요.\n");
        fprintf(stderr, "--------------------------------------------------------\n");
        system("pause");
        exit(EXIT_FAILURE);
        return NULL;
    }

    fseek(fptr, 0, SEEK_END);
    length = ftell(fptr);
    buf = (char*)malloc(length + 1);
    fseek(fptr, 0, SEEK_SET);
    fread(buf, length, 1, fptr);
    fclose(fptr);
    buf[length] = 0;

    return buf;
}

void make_vertexShaders() {
    GLchar* vertexSource;
    vertexSource = filetobuf("vertex.glsl");
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    GLint result;
    GLchar errorLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
    if (!result) {
        glGetShaderInfoLog(vertexShader, 512, NULL, errorLog);
        std::cerr << "ERROR: vertex shader 컴파일 실패\n" << errorLog << std::endl;
    }
}

void make_fragmentShaders() {
    GLchar* fragmentSource;
    fragmentSource = filetobuf("fragment.glsl");
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);

    GLint result;
    GLchar errorLog[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
    if (!result) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, errorLog);
        std::cerr << "ERROR: frag_shader 컴파일 실패\n" << errorLog << std::endl;
    }
}

GLuint make_shaderProgram() {
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

void loadDepthShader() {
    GLint result;
    GLchar errorLog[512];

    depthShader = glCreateProgram();

    GLuint depthVertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLchar* dvSource = filetobuf("depthvertex.glsl");
    glShaderSource(depthVertexShader, 1, &dvSource, NULL);
    glCompileShader(depthVertexShader);
    glAttachShader(depthShader, depthVertexShader);

    GLuint depthFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    GLchar* dfSource = filetobuf("depthfragment.glsl");
    glShaderSource(depthFragmentShader, 1, &dfSource, NULL);
    glCompileShader(depthFragmentShader);
    glAttachShader(depthShader, depthFragmentShader);

    glLinkProgram(depthShader);

    glDeleteShader(depthVertexShader);
    glDeleteShader(depthFragmentShader);
}