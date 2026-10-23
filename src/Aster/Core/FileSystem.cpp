#include "Aster/Core/FileSystem.h"

#include "Aster/Core/Log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Aster::FileSystem
{
    bool Exists(const Path& path)
    {
        std::error_code error;
        return std::filesystem::exists(path, error) && !error;
    }

    bool IsDirectory(const Path& path)
    {
        std::error_code error;
        return std::filesystem::is_directory(path, error) && !error;
    }

    std::optional<std::string> ReadTextFile(const Path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            ASTER_LOG_ERROR("FileSystem", "Could not open text file '%s'", Normalize(path).c_str());
            return std::nullopt;
        }
        stream.seekg(0, std::ios::end);
        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            return std::nullopt;
        }
        stream.seekg(0, std::ios::beg);
        std::string contents(static_cast<size_t>(size), '\0');
        stream.read(contents.data(), size);
        if (!stream)
        {
            ASTER_LOG_ERROR("FileSystem", "Could not read text file '%s'", Normalize(path).c_str());
            return std::nullopt;
        }
        return contents;
    }

    std::optional<std::vector<uint8>> ReadBinaryFile(const Path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            ASTER_LOG_ERROR("FileSystem", "Could not open binary file '%s'", Normalize(path).c_str());
            return std::nullopt;
        }
        stream.seekg(0, std::ios::end);
        const std::streamoff size = stream.tellg();
        if (size < 0)
        {
            return std::nullopt;
        }
        stream.seekg(0, std::ios::beg);
        std::vector<uint8> contents(static_cast<size_t>(size));
        stream.read(reinterpret_cast<char*>(contents.data()), size);
        if (!stream)
        {
            ASTER_LOG_ERROR("FileSystem", "Could not read binary file '%s'", Normalize(path).c_str());
            return std::nullopt;
        }
        return contents;
    }

    bool WriteTextFile(const Path& path, std::string_view contents)
    {
        std::error_code error;
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path(), error);
            if (error)
            {
                ASTER_LOG_ERROR("FileSystem", "Could not create directory '%s'", Normalize(path.parent_path()).c_str());
                return false;
            }
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            ASTER_LOG_ERROR("FileSystem", "Could not open text file for writing '%s'", Normalize(path).c_str());
            return false;
        }
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return stream.good();
    }

    bool WriteBinaryFile(const Path& path, const void* data, size_t size)
    {
        std::error_code error;
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path(), error);
            if (error)
            {
                ASTER_LOG_ERROR("FileSystem", "Could not create directory '%s'", Normalize(path.parent_path()).c_str());
                return false;
            }
        }
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            ASTER_LOG_ERROR("FileSystem", "Could not open binary file for writing '%s'", Normalize(path).c_str());
            return false;
        }
        stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        return stream.good();
    }

    bool CreateDirectories(const Path& path)
    {
        std::error_code error;
        std::filesystem::create_directories(path, error);
        return !error;
    }

    std::string GetExtension(const Path& path)
    {
        std::string extension = path.extension().string();
        if (!extension.empty() && extension.front() == '.')
        {
            extension.erase(extension.begin());
        }
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return extension;
    }

    std::string GetStem(const Path& path)
    {
        return path.stem().string();
    }

    std::string GetFilename(const Path& path)
    {
        return path.filename().string();
    }

    Path GetDirectory(const Path& path)
    {
        return path.parent_path();
    }

    std::string Normalize(const Path& path)
    {
        std::string result = path.generic_string();
        while (result.size() > 1 && result.back() == '/')
        {
            result.pop_back();
        }
        return result;
    }

    Path Join(const Path& left, const Path& right)
    {
        return left / right;
    }

    Path Resolve(const Path& root, const Path& path)
    {
        if (path.is_absolute())
        {
            return path;
        }
        return root / path;
    }

    Path Absolute(const Path& path)
    {
        std::error_code error;
        const Path absolute = std::filesystem::absolute(path, error);
        return error ? path : absolute.lexically_normal();
    }

    Path ExecutableDirectory()
    {
#if defined(_WIN32)
        std::vector<wchar_t> buffer(512);
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                break;
            }
            if (length < buffer.size())
            {
                return Path(std::wstring(buffer.data(), length)).parent_path();
            }
            buffer.resize(buffer.size() * 2);
        }
#endif
        std::error_code error;
        const Path current = std::filesystem::current_path(error);
        return error ? Path{} : current;
    }

    std::vector<Path> EnumerateFiles(const Path& directory, std::string_view extension, bool recursive)
    {
        std::vector<Path> files;
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error) || error)
        {
            return files;
        }

        const auto addIfMatching = [&](const Path& path)
        {
            if (!std::filesystem::is_regular_file(path, error) || error)
            {
                return;
            }
            if (!extension.empty() && GetExtension(path) != extension)
            {
                return;
            }
            files.push_back(path);
        };

        if (recursive)
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, error))
            {
                if (error)
                {
                    break;
                }
                addIfMatching(entry.path());
            }
        }
        else
        {
            for (const auto& entry : std::filesystem::directory_iterator(directory, error))
            {
                if (error)
                {
                    break;
                }
                addIfMatching(entry.path());
            }
        }

        std::sort(files.begin(), files.end());
        return files;
    }
}
