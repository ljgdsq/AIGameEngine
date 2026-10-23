#include "Aster/Core/FileSystem.h"
#include "Aster/Runtime/RuntimeApp.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    void PrintUsage()
    {
        std::cout << "Usage: AsterPlayer [--project <directory>] [--shaders <directory>] [--width N] "
                     "[--height N] [--frames N] [--screenshot <file>]\n";
    }

    bool ReadInt(const char* value, int& result)
    {
        if (value == nullptr)
        {
            return false;
        }
        try
        {
            const int parsed = std::stoi(value);
            if (parsed <= 0)
            {
                return false;
            }
            result = parsed;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}

int main(int argc, char** argv)
{
    Aster::RuntimeOptions options;
    const std::filesystem::path executable = Aster::FileSystem::Absolute(argc > 0 ? argv[0] : "AsterPlayer.exe");
    options.ProjectRoot = executable.parent_path();
    options.ShaderDirectory = executable.parent_path() / "shaders";

    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        const auto nextValue = [&](const char* name) -> const char* {
            if (index + 1 >= argc)
            {
                std::cerr << "Missing value for " << name << "\n";
                return nullptr;
            }
            return argv[++index];
        };

        if (argument == "--project")
        {
            const char* value = nextValue("--project");
            if (value == nullptr)
            {
                return 2;
            }
            options.ProjectRoot = Aster::FileSystem::Absolute(value);
        }
        else if (argument == "--shaders")
        {
            const char* value = nextValue("--shaders");
            if (value == nullptr)
            {
                return 2;
            }
            options.ShaderDirectory = Aster::FileSystem::Absolute(value);
        }
        else if (argument == "--width")
        {
            const char* value = nextValue("--width");
            if (!ReadInt(value, options.Width))
            {
                std::cerr << "Invalid --width\n";
                return 2;
            }
        }
        else if (argument == "--height")
        {
            const char* value = nextValue("--height");
            if (!ReadInt(value, options.Height))
            {
                std::cerr << "Invalid --height\n";
                return 2;
            }
        }
        else if (argument == "--frames")
        {
            const char* value = nextValue("--frames");
            if (!ReadInt(value, options.FrameLimit))
            {
                std::cerr << "Invalid --frames\n";
                return 2;
            }
        }
        else if (argument == "--screenshot")
        {
            const char* value = nextValue("--screenshot");
            if (value == nullptr)
            {
                return 2;
            }
            options.ScreenshotPath = value;
        }
        else if (argument == "--help" || argument == "-h")
        {
            PrintUsage();
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument: " << argument << "\n";
            PrintUsage();
            return 2;
        }
    }

    Aster::RuntimeApp app;
    return app.Run(options);
}
