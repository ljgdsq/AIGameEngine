#pragma once

#include <filesystem>
#include <string>

namespace Aster
{
    class Project;

    struct GameExportOptions
    {
        std::filesystem::path OutputDirectory;
        std::filesystem::path PlayerExecutable;
        std::filesystem::path ShaderDirectory;
    };

    // Copies a project and a prebuilt player into a self-contained game directory.
    class GameExporter
    {
    public:
        [[nodiscard]] static bool Export(const Project& project, const GameExportOptions& options,
            std::string* error = nullptr);
    };
}
