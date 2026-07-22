#include <iostream>
#include <cstdio>
#include <exception>

#include "Core/Application.h"
#include "Core/Logger.h"

int main(int argc, char* argv[])
{
    printf("--- Omnix Engine: main() started ---\n");
    fflush(stdout);

    try {
        std::cout.setf(std::ios::unitbuf);
        std::cout << "--- Omnix Engine: Initializing Application ---" << std::endl;
        std::cout.flush();

        // 1. Core Logger bootstrap
        Logger::Init("Omnix.log", LogLevel::Trace);

        // 2. Central Application Lifecycle
        {
            eng::app::Application app;
            if (app.Initialize(argc, argv)) {
                try {
                    app.Run();
                }
                catch (const std::exception& e) {
                    fprintf(stderr, "FATAL ERROR: Exception inside app.Run(): %s\n", e.what());
                    fflush(stderr);
                }
                catch (...) {
                    fprintf(stderr, "FATAL ERROR: Unknown exception inside app.Run()\n");
                    fflush(stderr);
                }
                app.Shutdown();
            } else {
                fprintf(stderr, "FATAL ERROR: Failed to initialize Application\n");
            }
        }

        // 3. Logger teardown
        Logger::Shutdown();
        
        std::cout << "--- Omnix Engine: main() finished cleanly ---" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        fprintf(stderr, "FATAL ERROR: Uncaught exception: %s\n", e.what());
        return -1;
    }
    catch (...) {
        fprintf(stderr, "FATAL ERROR: Unknown uncaught exception\n");
        return -1;
    }
}
