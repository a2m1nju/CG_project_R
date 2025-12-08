#pragma once
#include "Common.h"

void initFont(const char* filename, float pixelHeight);
void drawScene(GLvoid);
void renderTextTTF(float x, float y, const char* text, float r, float g, float b);
void renderTextWithOutline(float x, float y, const char* text);
void drawTree(int x, int z, GLuint shader);
void drawCar(const Car& car, GLuint shader);
void drawCoin(const Coin& coin, GLuint shader);
void drawLog(const Log& logObj, GLuint shader);
void drawLilyPad(const LilyPad& pad, GLuint shader);
void drawRail(int z, bool isWarning, bool isLightOn, GLuint shader);
void drawTrain(const Train& train, GLuint shader);
void drawItem(const Item& item, GLuint shader);
void drawParticles(GLuint shader);
void drawCloud(const Cloud& cloud, GLuint shader);
void drawClouds(GLuint shader);
void drawLogo();