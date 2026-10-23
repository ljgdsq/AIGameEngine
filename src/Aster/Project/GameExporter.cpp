#include "Aster/Project/GameExporter.h"

#include "Aster/Core/FileSystem.h"
#include "Aster/Core/Log.h"
#include "Aster/Project/Project.h"

#include <system_error>

namespace Aster
{
    namespace
    {
        void SetError(std::string* error, std::string message)
        {
            if (error != nullptr)
            {
                *error = std::move(message);
            }
        }

        bool CopyFileChecked(const std::filesystem::path& source, const std::filesystem::path& destination,
            std::string* error)
        {
            std::error_code code;
            if (!std::filesystem::is_regular_file(source, code) || code)
            {
                SetError(error, "Missing file: " + FileSystem::Normalize(source));
                return false;
            }
            std::filesystem::create_directories(destination.parent_path(), code);
            if (code)
            {
                SetError(error, "Could not create directory: " + FileSystem::Normalize(destination.parent_path()));
                return false;
            }
            std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing,
                code);
            if (code)
            {
                SetError(error, "Could not copy " + FileSystem::Normalize(source) + ": " + code.message());
                return false;
            }
            return true;
        }

        bool CopyDirectoryChecked(const std::filesystem::path& source, const std::filesystem::path& destination,
            std::string* error)
        {
            std::error_code code;
            if (!std::filesystem::is_directory(source, code) || code)
            {
                SetError(error, "Missing directory: " + FileSystem::Normalize(source));
                return false;
            }
            std::filesystem::copy(source, destination,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                code);
            if (code)
            {
                SetError(error, "Could not copy " + FileSystem::Normalize(source) + ": " + code.message());
                return false;
            }
            return true;
        }
    }

    bool GameExporter::Export(const Project& project, const GameExportOptions& options, std::string* error)
    {
        if (!project.IsOpen())
        {
            SetError(error, "Open a project before exporting a game");
            return false;
        }
        if (options.OutputDirectory.empty())
        {
            SetError(error, "Choose an export directory");
            return false;
        }
        if (options.PlayerExecutable.empty() || options.ShaderDirectory.empty())
        {
            SetError(error, "The player executable and shader directory are required");
            return false;
        }

        const std::filesystem::path output = FileSystem::Absolute(options.OutputDirectory);
        const std::filesystem::path root = FileSystem::Absolute(project.GetInfo().RootPath);
        std::error_code code;
        const std::filesystem::path relative = std::filesystem::relative(output, root, code);
        if (!code && !relative.empty() && relative.native().rfind(L"..", 0) != 0)
        {
            SetError(error, "The export directory must be outside the project directory");
            return false;
        }

        const char* shaderFiles[] = {
            "Pbr.vs.dxil", "Pbr.ps.dxil", "Pbr.shadow.vs.dxil", "Pbr.shadow.ps.dxil", "Pbr.post.vs.dxil",
            "Pbr.post.ps.dxil"
        };
        for (const char* shaderFile : shaderFiles)
        {
            if (!std::filesystem::is_regular_file(options.ShaderDirectory / shaderFile, code) || code)
            {
                SetError(error, std::string("Compiled shader was not found: ") + shaderFile + " in " +
                    FileSystem::Normalize(options.ShaderDirectory));
                return false;
            }
            code.clear();
        }

        if (!std::filesystem::is_regular_file(options.PlayerExecutable, code) || code)
        {
            SetError(error, "Player executable not found: " + FileSystem::Normalize(options.PlayerExecutable));
            return false;
        }

        if (std::filesystem::exists(output, code) && !std::filesystem::is_directory(output, code))
        {
            SetError(error, "The export path is not a directory: " + FileSystem::Normalize(output));
            return false;
        }
        if (std::filesystem::is_directory(output, code) &&
            std::filesystem::directory_iterator(output, code) != std::filesystem::directory_iterator())
        {
            SetError(error, "The export directory must be empty: " + FileSystem::Normalize(output));
            return false;
        }

        std::filesystem::create_directories(output, code);
        if (code)
        {
            SetError(error, "Could not create export directory: " + FileSystem::Normalize(output));
            return false;
        }

        const auto fail = [&](std::string message) {
            std::error_code cleanupCode;
            std::filesystem::remove_all(output, cleanupCode);
            SetError(error, std::move(message));
            return false;
        };

        if (!CopyFileChecked(options.PlayerExecutable, output / "Game.exe", error))
        {
            return fail(error != nullptr ? *error : "Could not copy the player executable");
        }
        for (const char* shaderFile : shaderFiles)
        {
            if (!CopyFileChecked(options.ShaderDirectory / shaderFile, output / "shaders" / shaderFile, error))
            {
                return fail(error != nullptr ? *error : "Could not copy the compiled shaders");
            }
        }
        if (!CopyFileChecked(root / "project.aster.yaml", output / "project.aster.yaml", error))
        {
            return fail(error != nullptr ? *error : "Could not copy the project file");
        }

        const char* directories[] = { "assets", "scenes", "prefabs" };
        for (const char* directory : directories)
        {
            if (std::filesystem::is_directory(root / directory, code))
            {
                if (!CopyDirectoryChecked(root / directory, output / directory, error))
                {
                    return fail(error != nullptr ? *error : "Could not copy project content");
                }
            }
        }

        const std::string manifest = "format: aster-export\nversion: 1\nname: " + project.GetInfo().Name +
            "\nproject: project.aster.yaml\nexecutable: Game.exe\n";
        if (!FileSystem::WriteTextFile(output / "export.aster.yaml", manifest))
        {
            return fail("Could not write the export manifest");
        }

        ASTER_LOG_INFO("Exporter", "Exported '%s' to %s", project.GetInfo().Name.c_str(),
            FileSystem::Normalize(output).c_str());
        return true;
    }
}
