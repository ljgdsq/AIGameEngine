#include "Aster/Editor/ContentBrowser.h"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

namespace Aster
{
    namespace
    {
        std::string Lower(std::string_view value)
        {
            std::string result(value);
            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return result;
        }

        bool EndsWith(std::string_view value, std::string_view suffix)
        {
            return value.size() >= suffix.size() &&
                value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        bool IsInvalidNameCharacter(char character)
        {
            switch (character)
            {
                case '<':
                case '>':
                case ':':
                case '"':
                case '/':
                case '\\':
                case '|':
                case '?':
                case '*':
                    return true;
                default:
                    return false;
            }
        }

        // Normalizes a browser-relative path: forward slashes, no leading slash, no empty or
        // current-directory components. Returns false when the path escapes the root.
        bool NormalizeRelative(std::string_view input, std::string& output)
        {
            output.clear();
            if (input.empty())
            {
                return true;
            }
            if (input.front() == '/' || input.front() == '\\' || input.find(':') != std::string_view::npos)
            {
                return false;
            }

            size_t start = 0;
            while (start <= input.size())
            {
                const size_t separator = input.find_first_of("/\\", start);
                const size_t end = separator == std::string_view::npos ? input.size() : separator;
                const std::string_view component = input.substr(start, end - start);
                if (!component.empty() && component != ".")
                {
                    if (component == "..")
                    {
                        return false;
                    }
                    if (!output.empty())
                    {
                        output.push_back('/');
                    }
                    output.append(component);
                }
                if (separator == std::string_view::npos)
                {
                    break;
                }
                start = separator + 1;
            }
            return true;
        }
    }

    void ContentBrowser::SetRoot(const std::filesystem::path& rootDirectory)
    {
        Clear();
        if (rootDirectory.empty())
        {
            return;
        }

        std::error_code code;
        std::filesystem::path absolute = std::filesystem::absolute(rootDirectory, code);
        if (code)
        {
            absolute = rootDirectory;
        }
        const std::filesystem::path canonical = std::filesystem::weakly_canonical(absolute, code);
        m_Root = code ? absolute.lexically_normal() : canonical;
        if (!std::filesystem::is_directory(m_Root, code) || code)
        {
            m_Root.clear();
            return;
        }
        m_Valid = true;
        Refresh();
    }

    void ContentBrowser::Clear()
    {
        m_Root.clear();
        m_CurrentDirectory.clear();
        m_Filter.clear();
        m_LastError.clear();
        m_AllItems.clear();

        m_Items.clear();
        m_SubdirectoryCache.clear();
        m_Valid = false;
    }

    bool ContentBrowser::Fail(std::string message, std::string* error)
    {
        m_LastError = std::move(message);
        if (error != nullptr)
        {
            *error = m_LastError;
        }
        return false;
    }

    bool ContentBrowser::IsInsideRoot(const std::filesystem::path& candidate) const
    {
        if (!m_Valid || m_Root.empty())
        {
            return false;
        }
        std::error_code code;
        const std::filesystem::path relative = std::filesystem::relative(candidate, m_Root, code);
        if (code)
        {
            return false;
        }
        const std::string text = relative.generic_string();
        return text.empty() || (text != ".." && !EndsWith(text, "/..") &&
            text.rfind("../", 0) != 0 && !relative.is_absolute());
    }

    void ContentBrowser::Refresh()
    {
        m_AllItems.clear();
        m_Items.clear();
        m_SubdirectoryCache.clear();
        if (!m_Valid)
        {
            return;
        }

        std::filesystem::path directory = Resolve(m_CurrentDirectory);
        std::error_code code;
        if (!std::filesystem::is_directory(directory, code) || code)
        {
            m_CurrentDirectory.clear();
            directory = m_Root;
        }

        std::filesystem::directory_iterator iterator(directory,
            std::filesystem::directory_options::skip_permission_denied, code);
        if (code)
        {
            (void)Fail("Could not read the directory: " + directory.generic_string(), nullptr);
            return;
        }

        for (const std::filesystem::directory_entry& entry : iterator)
        {
            std::error_code entryCode;
            const bool symlink = entry.is_symlink(entryCode);
            entryCode.clear();
            const bool isDirectory = !symlink && entry.is_directory(entryCode) && !entryCode;
            const std::string name = entry.path().filename().string();
            if (name.empty())
            {
                continue;
            }

            ContentItem item;
            item.Name = name;
            item.RelativePath = m_CurrentDirectory.empty() ? name : m_CurrentDirectory + "/" + name;
            item.IsDirectory = isDirectory;
            item.Type = isDirectory ? ContentItemType::Folder : Classify(name);
            item.Extension = isDirectory ? std::string() : Lower(entry.path().extension().string());
            if (!isDirectory && (EndsWith(Lower(name), ".aster.yaml") || EndsWith(Lower(name), ".aster.yml")))
            {
                item.Extension = ".aster.yaml";
            }
            if (!isDirectory)
            {
                entryCode.clear();
                item.Size = entry.file_size(entryCode);
                if (entryCode)
                {
                    item.Size = 0;
                }
            }
            m_AllItems.push_back(std::move(item));
        }

        std::sort(m_AllItems.begin(), m_AllItems.end(), [](const ContentItem& left, const ContentItem& right) {
            if (left.IsDirectory != right.IsDirectory)
            {
                return left.IsDirectory;
            }
            return Lower(left.Name) < Lower(right.Name);
        });

        std::vector<std::string> directories;
        for (const ContentItem& item : m_AllItems)
        {
            if (item.IsDirectory)
            {
                directories.push_back(item.RelativePath);
            }
        }
        m_SubdirectoryCache.emplace(m_CurrentDirectory, std::move(directories));
        SortAndFilter();
    }

    void ContentBrowser::SortAndFilter()
    {
        m_Items.clear();
        const std::string filter = Lower(m_Filter);
        for (const ContentItem& item : m_AllItems)
        {
            if (!filter.empty() && Lower(item.Name).find(filter) == std::string::npos)
            {
                continue;
            }
            m_Items.push_back(item);
        }
    }

    void ContentBrowser::SetFilter(std::string filter)
    {
        if (m_Filter == filter)
        {
            return;
        }
        m_Filter = std::move(filter);
        SortAndFilter();
    }

    bool ContentBrowser::NavigateTo(std::string_view relativeDirectory)
    {
        if (!m_Valid)
        {
            return Fail("No project assets are loaded", nullptr);
        }
        std::string normalized;
        if (!NormalizeRelative(relativeDirectory, normalized))
        {
            return Fail("Invalid asset directory: " + std::string(relativeDirectory), nullptr);
        }

        const std::filesystem::path candidate = Resolve(normalized);
        std::error_code code;
        if (!std::filesystem::is_directory(candidate, code) || code)
        {
            return Fail("Asset directory does not exist: " + normalized, nullptr);
        }
        if (!IsInsideRoot(candidate))
        {
            return Fail("Asset directory is outside of the project", nullptr);
        }
        m_CurrentDirectory = std::move(normalized);
        Refresh();
        return true;
    }

    bool ContentBrowser::NavigateUp()
    {
        if (m_CurrentDirectory.empty())
        {
            return false;
        }
        const size_t separator = m_CurrentDirectory.find_last_of('/');
        const std::string parent = separator == std::string::npos ? std::string() :
            m_CurrentDirectory.substr(0, separator);
        return NavigateTo(parent);
    }

    const std::vector<std::string>& ContentBrowser::GetSubdirectories(std::string_view relativeDirectory)
    {
        static const std::vector<std::string> empty;
        if (!m_Valid)
        {
            return empty;
        }
        std::string normalized;
        if (!NormalizeRelative(relativeDirectory, normalized))
        {
            return empty;
        }
        if (const auto iterator = m_SubdirectoryCache.find(normalized);
            iterator != m_SubdirectoryCache.end())
        {
            return iterator->second;
        }

        std::vector<std::string> directories;
        std::error_code code;
        std::filesystem::directory_iterator iterator(Resolve(normalized),
            std::filesystem::directory_options::skip_permission_denied, code);
        if (!code)
        {
            for (const std::filesystem::directory_entry& entry : iterator)
            {
                std::error_code entryCode;
                if (entry.is_symlink(entryCode) || !entry.is_directory(entryCode) || entryCode)
                {
                    continue;
                }
                const std::string name = entry.path().filename().string();
                if (!name.empty())
                {
                    directories.push_back(normalized.empty() ? name : normalized + "/" + name);
                }
            }
        }
        std::sort(directories.begin(), directories.end(), [](const std::string& left, const std::string& right) {
            return Lower(left) < Lower(right);
        });
        return m_SubdirectoryCache.emplace(normalized, std::move(directories)).first->second;
    }

    std::filesystem::path ContentBrowser::Resolve(std::string_view relativePath) const
    {
        std::string normalized;
        if (!NormalizeRelative(relativePath, normalized) || normalized.empty())
        {
            return m_Root;
        }
        return m_Root / std::filesystem::path(normalized);
    }


    std::string ContentBrowser::GetAssetPath(const ContentItem& item) const
    {
        if (item.RelativePath.empty())
        {
            return "assets";
        }
        // The browser root is the project root so scenes/ and prefabs/ are browsable alongside
        // assets/. Keep those project-relative paths intact; treat everything else as an asset.
        if (item.RelativePath.rfind("assets/", 0) == 0 || item.RelativePath.rfind("scenes/", 0) == 0 ||
            item.RelativePath.rfind("prefabs/", 0) == 0 || item.RelativePath == "project.aster.yaml")
        {
            return item.RelativePath;
        }
        return "assets/" + item.RelativePath;
    }

    bool ContentBrowser::IsValidName(std::string_view name) const
    {
        if (name.empty() || name == "." || name == "..")
        {
            return false;
        }
        if (name.front() == '.' || name.back() == '.' || name.back() == ' ')
        {
            return false;
        }
        for (char character : name)
        {
            if (IsInvalidNameCharacter(character) || static_cast<unsigned char>(character) < 0x20)
            {
                return false;
            }
        }
        return true;
    }

    bool ContentBrowser::CreateFolder(std::string_view name, std::string* error)
    {
        m_LastError.clear();
        if (!m_Valid)
        {
            return Fail("No project assets are loaded", error);
        }
        if (!IsValidName(name))
        {
            return Fail("Invalid folder name: " + std::string(name), error);
        }
        const std::filesystem::path target = Resolve(m_CurrentDirectory) / std::filesystem::path(name);
        std::error_code code;
        if (std::filesystem::exists(target, code))
        {
            return Fail("A file or folder with that name already exists", error);
        }
        if (!std::filesystem::create_directory(target, code) || code)
        {
            return Fail("Could not create the folder: " + target.generic_string(), error);
        }
        Refresh();
        return true;
    }

    bool ContentBrowser::Rename(const ContentItem& item, std::string_view newName, std::string* error)
    {
        m_LastError.clear();
        if (!m_Valid)
        {
            return Fail("No project assets are loaded", error);
        }
        if (item.RelativePath.empty())
        {
            return Fail("The asset root cannot be renamed", error);
        }
        if (!IsValidName(newName))
        {
            return Fail("Invalid asset name: " + std::string(newName), error);
        }
        const std::filesystem::path source = Resolve(item);
        std::error_code code;
        if (!std::filesystem::exists(source, code))
        {
            return Fail("The asset no longer exists", error);
        }
        const std::filesystem::path target = source.parent_path() / std::filesystem::path(newName);
        if (std::filesystem::exists(target, code))
        {
            return Fail("A file or folder with that name already exists", error);
        }
        std::filesystem::rename(source, target, code);
        if (code)
        {
            return Fail("Could not rename the asset: " + code.message(), error);
        }
        Refresh();
        return true;
    }

    bool ContentBrowser::Delete(const ContentItem& item, std::string* error)
    {
        m_LastError.clear();
        if (!m_Valid)
        {
            return Fail("No project assets are loaded", error);
        }
        if (item.RelativePath.empty())
        {
            return Fail("The asset root cannot be deleted", error);
        }
        const std::filesystem::path target = Resolve(item);
        if (!IsInsideRoot(target))
        {
            return Fail("The asset is outside of the project", error);
        }
        std::error_code code;
        if (!std::filesystem::exists(target, code))
        {
            return Fail("The asset no longer exists", error);
        }
        const uintmax_t removed = std::filesystem::remove_all(target, code);
        if (code || removed == 0)
        {
            return Fail("Could not delete the asset: " + (code ? code.message() : "nothing removed"), error);
        }
        Refresh();
        return true;
    }

    ContentItemType ContentBrowser::Classify(std::string_view fileName)
    {
        const std::string lower = Lower(fileName);
        if (EndsWith(lower, ".aster.yaml") || EndsWith(lower, ".aster.yml"))
        {
            return ContentItemType::Scene;
        }

        const std::string extension = Lower(std::filesystem::path(fileName).extension().string());
        if (extension == ".obj" || extension == ".gltf" || extension == ".glb" || extension == ".fbx")
        {
            return ContentItemType::Mesh;
        }
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
            extension == ".tga")
        {
            return ContentItemType::Texture;
        }
        if (extension == ".hdr" || extension == ".exr")
        {
            return ContentItemType::Environment;
        }
        if (extension == ".lua")
        {
            return ContentItemType::Script;
        }
        if (extension == ".wav" || extension == ".mp3" || extension == ".ogg" || extension == ".flac")
        {
            return ContentItemType::Audio;
        }
        return ContentItemType::Other;
    }

    std::string_view ContentBrowser::TypeToString(ContentItemType type)
    {
        switch (type)
        {
            case ContentItemType::Folder: return "Folder";
            case ContentItemType::Scene: return "Scene";
            case ContentItemType::Mesh: return "Mesh";
            case ContentItemType::Texture: return "Texture";
            case ContentItemType::Environment: return "Environment";
            case ContentItemType::Script: return "Script";
            case ContentItemType::Audio: return "Audio";
            case ContentItemType::Other: return "File";
        }
        return "File";
    }
}
