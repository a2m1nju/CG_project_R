#pragma once

// Keep the original 1280x960 game coordinates inside any window size.
struct GameViewport {
    int x;
    int y;
    int width;
    int height;

    float scale() const { return width / 1280.0f; }
    int pixelX(float gameX) const { return x + static_cast<int>(gameX * scale()); }
    int pixelY(float gameY) const { return y + static_cast<int>(gameY * scale()); }
    int pixels(float length) const { return static_cast<int>(length * scale()); }
};

inline GameViewport FitGameViewport(int windowWidth, int windowHeight) {
    if (windowWidth <= 0 || windowHeight <= 0) return { 0, 0, 0, 0 };

    int width = windowWidth;
    int height = windowHeight;
    if (static_cast<long long>(windowWidth) * 3 >
        static_cast<long long>(windowHeight) * 4) {
        width = windowHeight * 4 / 3;
    } else {
        height = windowWidth * 3 / 4;
    }
    return { (windowWidth - width) / 2, (windowHeight - height) / 2,
             width, height };
}
