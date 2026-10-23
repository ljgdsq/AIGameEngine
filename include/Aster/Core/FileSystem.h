#pragma once

#include "Aster/Core/Base.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Aster::FileSystem
{
    using Path = std::filesystem::path;

    [[nodiscard]] bool Exists(const Path& path);
    [[nodiscard]] bool IsDirectory(const Path& path);

    [[nodiscard]] std::optional<std::string> ReadTextFile(const Path& path);
    [[nodiscard]] std::optional<std::vector<uint8>> ReadBinaryFile(const Path& path);

    [[nodiscard]] bool WriteTextFile(const Path& path, std::string_view contents);
    [[nodiscard]] bool WriteBinaryFile(const Path& path, const void* data, size_t size);

    [[nodiscard]] bool CreateDirectories(const Path& path);

    [[nodiscard]] std::string GetExtension(const Path& path);
    [[nodiscard]] std::string GetStem(const Path& path);
    [[nodiscard]] std::string GetFilename(const Path& path);
    [[nodiscard]] Path GetDirectory(const Path& path);

    [[nodiscard]] std::string Normalize(const Path& path);
    [[nodiscard]] Path Join(const Path& left, const Path& right);
    [[nodiscard]] Path Resolve(const Path& root, const Path& path);
    [[nodiscard]] Path Absolute(const Path& path);
    [[nodiscard]] Path ExecutableDirectory();

    [[nodiscard]] std::vector<Path> EnumerateFiles(const Path& directory, std::string_view extension, bool recursive);
}
