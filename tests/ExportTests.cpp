#include "TestFramework.h"

#include "Aster/Core/FileSystem.h"
#include "Aster/Project/GameExporter.h"
#include "Aster/Project/Project.h"

#include <chrono>
#include <filesystem>

using namespace Aster;

namespace
{
    std::filesystem::path MakeTempRoot()
    {
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() / ("aster_export_test_" + std::to_string(stamp));
    }

    struct ExportFixture
    {
        std::filesystem::path Root;
        std::filesystem::path Output;
        GameExportOptions Options;
    };

    ExportFixture MakeFixture()
    {
        const std::filesystem::path root = MakeTempRoot();
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "tool" / "shaders");
        (void)FileSystem::WriteTextFile(root / "tool" / "AsterPlayer.exe", "player");
        for (const char* shader : { "Pbr.vs.dxil", "Pbr.ps.dxil", "Pbr.shadow.vs.dxil", "Pbr.shadow.ps.dxil",
                 "Pbr.post.vs.dxil", "Pbr.post.ps.dxil", "Pbr.ssao.ps.dxil", "Pbr.sky.ps.dxil",
                 "DebugLine.vs.dxil", "DebugLine.ps.dxil" })
        {
            (void)FileSystem::WriteTextFile(root / "tool" / "shaders" / shader, "shader");
        }

        ExportFixture fixture;
        fixture.Root = root / "project";
        fixture.Output = root / "exported";
        fixture.Options.PlayerExecutable = root / "tool" / "AsterPlayer.exe";
        fixture.Options.ShaderDirectory = root / "tool" / "shaders";
        fixture.Options.OutputDirectory = fixture.Output;
        return fixture;
    }
}

ASTER_TEST(Export, CopiesProjectPlayerAndShaders)
{
    ExportFixture fixture = MakeFixture();
    Project project;
    std::string error;
    ASTER_CHECK(project.Create(fixture.Root, "Exported Game", &error));

    ASTER_CHECK(GameExporter::Export(project, fixture.Options, &error));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "Game.exe"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "Pbr.vs.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "Pbr.ps.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "Pbr.shadow.vs.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "Pbr.shadow.ps.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "Pbr.post.vs.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "Pbr.post.ps.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "Pbr.ssao.ps.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "Pbr.sky.ps.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "DebugLine.vs.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "shaders" / "DebugLine.ps.dxil"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "project.aster.yaml"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "export.aster.yaml"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "scenes" / "main.aster.yaml"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "assets" / "scripts" / "spin.lua"));
    ASTER_CHECK(std::filesystem::exists(fixture.Output / "assets" / "models" / "cube.obj"));
    ASTER_CHECK_EQ(FileSystem::ReadTextFile(fixture.Output / "Game.exe").value_or(""), std::string("player"));

    const std::string manifest = FileSystem::ReadTextFile(fixture.Output / "export.aster.yaml").value_or("");
    ASTER_CHECK(manifest.find("name: Exported Game") != std::string::npos);
    ASTER_CHECK(manifest.find("executable: Game.exe") != std::string::npos);

    std::filesystem::remove_all(fixture.Root.parent_path());
}

ASTER_TEST(Export, RejectsClosedProject)
{
    ExportFixture fixture = MakeFixture();
    Project project;
    std::string error;
    ASTER_CHECK(!GameExporter::Export(project, fixture.Options, &error));
    ASTER_CHECK(!error.empty());
    std::filesystem::remove_all(fixture.Root.parent_path());
}

ASTER_TEST(Export, RejectsOutputInsideProject)
{
    ExportFixture fixture = MakeFixture();
    Project project;
    std::string error;
    ASTER_CHECK(project.Create(fixture.Root, "Export", &error));

    fixture.Options.OutputDirectory = fixture.Root / "build";
    ASTER_CHECK(!GameExporter::Export(project, fixture.Options, &error));
    ASTER_CHECK(error.find("outside") != std::string::npos);
    std::filesystem::remove_all(fixture.Root.parent_path());
}

ASTER_TEST(Export, RejectsNonEmptyOutput)
{
    ExportFixture fixture = MakeFixture();
    Project project;
    std::string error;
    ASTER_CHECK(project.Create(fixture.Root, "Export", &error));
    std::filesystem::create_directories(fixture.Output);
    (void)FileSystem::WriteTextFile(fixture.Output / "stale.txt", "stale");

    ASTER_CHECK(!GameExporter::Export(project, fixture.Options, &error));
    ASTER_CHECK(error.find("empty") != std::string::npos);
    std::filesystem::remove_all(fixture.Root.parent_path());
}

ASTER_TEST(Export, RejectsMissingPlayer)
{
    ExportFixture fixture = MakeFixture();
    Project project;
    std::string error;
    ASTER_CHECK(project.Create(fixture.Root, "Export", &error));
    fixture.Options.PlayerExecutable = fixture.Root.parent_path() / "missing.exe";

    ASTER_CHECK(!GameExporter::Export(project, fixture.Options, &error));
    ASTER_CHECK(error.find("Player") != std::string::npos);
    std::filesystem::remove_all(fixture.Root.parent_path());
}
