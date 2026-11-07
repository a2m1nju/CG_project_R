#include "../컴그프젝(찐)/ServerApp.h"
#include <iostream>

int main() {
    ServerApp app;
    if (!app.Initialize(9000)) {
        std::cerr << "Server initialization failed.\n";
        return 1;
    }

    app.Run();
    app.Shutdown();
    return 0;
}
