#pragma once

#include "Aster/Core/Base.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Aster
{
    enum class ContentItemType : uint8
    {
        Folder = 0,
        Scene,
        Mesh,
        Texture,
        Environment,
        Script,
        Audio,
        Other
    };

    struct ContentItem
    {
        std::string Name;
        // Path relative to the browser's root using '/' separators. The root itself is "".
        std::string RelativePath;
        std::string Extension;
        ContentItemType Type = ContentItemType::Other;
        uintmax_t Size = 0;
        bool IsDirectory = false;
    };

    // Filesystem model behind the editor content browser. It owns navigation, filtering and the
    // small set of write operations exposed by the panel. The class has no ImGui dependency so the
    // navigation and mutation rules can be tested without an editor window.
    class ContentBrowser
    {
    public:
        ContentBrowser() = default;

        // Sets the root directory (usually the project root) and navigates to it. A fresh root
        // always refreshes the listing; a root that does not exist leaves the browser invalid.
        void SetRoot(const std::filesystem::path& rootDirectory);
        void Clear();

        [[nodiscard]] const std::filesystem::path& GetRoot() const { return m_Root; }
        [[nodiscard]] bool IsValid() const { return m_Valid; }
        [[nodiscard]] const std::string& GetCurrentPath() const { return m_CurrentDirectory; }
        [[nodiscard]] const std::string& GetLastError() const { return m_LastError; }

        void Refresh();

        // Navigates to a directory relative to the asset root. Rejects absolute paths, ".."
        // traversal and paths that resolve outside of the root.
        [[nodiscard]] bool NavigateTo(std::string_view relativeDirectory);
        [[nodiscard]] bool NavigateUp();

        void SetFilter(std::string filter);
        [[nodiscard]] const std::string& GetFilter() const { return m_Filter; }

        // Items in the current directory after filtering, directories first then files, both
        // ordered case-insensitively by name.
        [[nodiscard]] const std::vector<ContentItem>& GetItems() const { return m_Items; }
        [[nodiscard]] const std::vector<ContentItem>& GetAllItems() const { return m_AllItems; }
        // Immediate subdirectories of a directory relative to the root. Cached until Refresh().
        [[nodiscard]] const std::vector<std::string>& GetSubdirectories(std::string_view relativeDirectory);

        [[nodiscard]] std::filesystem::path Resolve(std::string_view relativePath) const;
        [[nodiscard]] std::filesystem::path Resolve(const ContentItem& item) const
        {
            return Resolve(item.RelativePath);
        }
        // Project-relative path used by the asset manager and scene loader. Items outside the
        // conventional assets/scenes/prefabs folders are treated as asset-manager paths.
        [[nodiscard]] std::string GetAssetPath(const ContentItem& item) const;

        [[nodiscard]] bool CreateFolder(std::string_view name, std::string* error = nullptr);
        [[nodiscard]] bool Rename(const ContentItem& item, std::string_view newName,
            std::string* error = nullptr);
        [[nodiscard]] bool Delete(const ContentItem& item, std::string* error = nullptr);

        [[nodiscard]] static ContentItemType Classify(std::string_view fileName);
        [[nodiscard]] static std::string_view TypeToString(ContentItemType type);

    private:
        [[nodiscard]] bool Fail(std::string message, std::string* error);
        [[nodiscard]] bool IsValidName(std::string_view name) const;
        [[nodiscard]] bool IsInsideRoot(const std::filesystem::path& candidate) const;
        void SortAndFilter();

        std::filesystem::path m_Root;
        std::string m_CurrentDirectory;
        std::string m_Filter;
        std::string m_LastError;
        std::vector<ContentItem> m_AllItems;
        std::vector<ContentItem> m_Items;
        std::unordered_map<std::string, std::vector<std::string>> m_SubdirectoryCache;
        bool m_Valid = false;
    };
}
