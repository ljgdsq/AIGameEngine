#include "TestFramework.h"

#include "Aster/Assets/AssetManager.h"
#include "Aster/Assets/Primitives.h"
#include "Aster/Core/FileSystem.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using namespace Aster;

namespace
{
    std::filesystem::path MakeTempRoot()
    {
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() / ("aster_asset_test_" + std::to_string(stamp));
    }

    void WriteU16(std::vector<uint8>& data, size_t offset, uint16 value)
    {
        data[offset] = static_cast<uint8>(value & 0xFF);
        data[offset + 1] = static_cast<uint8>((value >> 8) & 0xFF);
    }

    void WriteU32(std::vector<uint8>& data, size_t offset, uint32 value)
    {
        data[offset] = static_cast<uint8>(value & 0xFF);
        data[offset + 1] = static_cast<uint8>((value >> 8) & 0xFF);
        data[offset + 2] = static_cast<uint8>((value >> 16) & 0xFF);
        data[offset + 3] = static_cast<uint8>((value >> 24) & 0xFF);
    }

    std::vector<uint8> MakeTwoByTwoBmp()
    {
        constexpr uint32 width = 2;
        constexpr uint32 height = 2;
        constexpr uint32 rowSize = 8;
        std::vector<uint8> data(54 + rowSize * height, 0);
        data[0] = 'B';
        data[1] = 'M';
        WriteU32(data, 2, static_cast<uint32>(data.size()));
        WriteU32(data, 10, 54);
        WriteU32(data, 14, 40);
        WriteU32(data, 18, width);
        WriteU32(data, 22, height);
        WriteU16(data, 26, 1);
        WriteU16(data, 28, 24);
        WriteU32(data, 34, rowSize * height);

        const uint8 colors[4][3] = {
            { 0, 0, 255 }, { 0, 255, 0 }, { 255, 0, 0 }, { 255, 255, 255 }
        };
        for (uint32 y = 0; y < height; ++y)
        {
            for (uint32 x = 0; x < width; ++x)
            {
                const size_t offset = 54 + y * rowSize + x * 3;
                const uint8* color = colors[y * width + x];
                data[offset] = color[0];
                data[offset + 1] = color[1];
                data[offset + 2] = color[2];
            }
        }
        return data;
    }
}

ASTER_TEST(Assets, LoadsTextureAndReportsMissingFiles)
{
    const std::filesystem::path root = MakeTempRoot();
    std::filesystem::remove_all(root);
    ASTER_CHECK(FileSystem::CreateDirectories(root / "assets" / "textures"));
    const std::vector<uint8> bmp = MakeTwoByTwoBmp();
    ASTER_CHECK(FileSystem::WriteBinaryFile(root / "assets" / "textures" / "checker.bmp", bmp.data(), bmp.size()));

    AssetManager assets(root);
    std::string error;
    const AssetRef<Texture> texture = assets.LoadTexture("assets/textures/checker.bmp", &error);
    ASTER_CHECK(texture.IsValid());
    ASTER_CHECK(texture.Asset->Pixels.Width == 2);
    ASTER_CHECK(texture.Asset->Pixels.Height == 2);
    ASTER_CHECK_EQ(texture.Asset->Pixels.Data.size(), static_cast<size_t>(16));

    const AssetRef<Texture> missing = assets.LoadTexture("assets/textures/missing.png", &error);
    ASTER_CHECK(!missing.IsValid());
    ASTER_CHECK(!error.empty());

    std::filesystem::remove_all(root);
}

ASTER_TEST(Assets, RejectsUnsupportedTextureExtension)
{
    const std::filesystem::path root = MakeTempRoot();
    std::filesystem::remove_all(root);
    ASTER_CHECK(FileSystem::CreateDirectories(root / "assets"));
    ASTER_CHECK(FileSystem::WriteTextFile(root / "assets" / "bad.txt", "not an image"));

    AssetManager assets(root);
    std::string error;
    const AssetRef<Texture> texture = assets.LoadTexture("assets/bad.txt", &error);
    ASTER_CHECK(!texture.IsValid());
    ASTER_CHECK(!error.empty());

    std::filesystem::remove_all(root);
}

ASTER_TEST(Assets, ExecutableDirectoryIsUsable)
{
    const std::filesystem::path directory = FileSystem::ExecutableDirectory();
    ASTER_CHECK(!directory.empty());
    ASTER_CHECK(std::filesystem::is_directory(directory));
}

ASTER_TEST(Assets, PrimitiveCubeIncludesTextureCoordinates)
{
    const std::string cube = Primitives::CubeObj();
    ASTER_CHECK(cube.find("\nvt ") != std::string::npos);
    ASTER_CHECK(cube.find("f 1/1/1") != std::string::npos);
    ASTER_CHECK(cube.find("//") == std::string::npos);

    const std::string plane = Primitives::PlaneObj();
    ASTER_CHECK(plane.find("\nvt ") != std::string::npos);
}
