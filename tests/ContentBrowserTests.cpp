#include "TestFramework.h"

#include "Aster/Editor/ContentBrowser.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Aster;

namespace
{
    struct TempAssetRoot
    {
        std::filesystem::path Path;

        TempAssetRoot()
        {
            const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            Path = std::filesystem::temp_directory_path() /
                ("aster_content_browser_test_" + std::to_string(stamp));
            std::filesystem::remove_all(Path);
            std::filesystem::create_directories(Path);
        }

        ~TempAssetRoot()
        {
            std::error_code code;
            std::filesystem::remove_all(Path, code);
        }
    };

    void WriteFile(const std::filesystem::path& path, const std::string& contents = "test")
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        stream << contents;
    }

    const ContentItem* FindItem(const std::vector<ContentItem>& items, const std::string& name)
    {
        for (const ContentItem& item : items)
        {
            if (item.Name == name)
            {
                return &item;
            }
        }
        return nullptr;
    }
}

ASTER_TEST(ContentBrowser, ListsAndClassifiesAssets)
{
    TempAssetRoot root;
    WriteFile(root.Path / "Audio" / "ambient.wav");
    WriteFile(root.Path / "Scripts" / "spin.lua");
    WriteFile(root.Path / "Textures" / "checker.png");
    WriteFile(root.Path / "Main.aster.yaml", "scene: true");
    WriteFile(root.Path / "readme.txt");

    ContentBrowser browser;
    browser.SetRoot(root.Path);
    ASTER_CHECK(browser.IsValid());
    ASTER_CHECK_EQ(browser.GetRoot(), root.Path);
    ASTER_CHECK(browser.GetCurrentPath().empty());

    const std::vector<ContentItem>& items = browser.GetItems();
    ASTER_CHECK_EQ(items.size(), 5U);

    // Directories come first, then files, both case-insensitively sorted.
    ASTER_CHECK_EQ(items[0].Name, std::string("Audio"));
    ASTER_CHECK(items[0].IsDirectory);
    ASTER_CHECK(items[0].Type == ContentItemType::Folder);
    ASTER_CHECK_EQ(items[1].Name, std::string("Scripts"));
    ASTER_CHECK_EQ(items[2].Name, std::string("Textures"));
    ASTER_CHECK_EQ(items[3].Name, std::string("Main.aster.yaml"));
    ASTER_CHECK(items[3].Type == ContentItemType::Scene);
    ASTER_CHECK_EQ(items[3].Extension, std::string(".aster.yaml"));
    ASTER_CHECK_EQ(items[4].Name, std::string("readme.txt"));
    ASTER_CHECK(items[4].Type == ContentItemType::Other);

    const ContentItem* texture = FindItem(items, "Textures");
    ASTER_CHECK(texture != nullptr);
    if (texture != nullptr)
    {
        ASTER_CHECK_EQ(browser.GetAssetPath(*texture), std::string("assets/Textures"));
    }

    ASTER_CHECK(ContentBrowser::Classify("a.obj") == ContentItemType::Mesh);
    ASTER_CHECK(ContentBrowser::Classify("a.gltf") == ContentItemType::Mesh);
    ASTER_CHECK(ContentBrowser::Classify("a.PNG") == ContentItemType::Texture);
    ASTER_CHECK(ContentBrowser::Classify("a.hdr") == ContentItemType::Environment);
    ASTER_CHECK(ContentBrowser::Classify("a.lua") == ContentItemType::Script);
    ASTER_CHECK(ContentBrowser::Classify("a.wav") == ContentItemType::Audio);
    ASTER_CHECK(ContentBrowser::Classify("a.aster.yaml") == ContentItemType::Scene);
    ASTER_CHECK(ContentBrowser::Classify("a.yaml") == ContentItemType::Other);
}

ASTER_TEST(ContentBrowser, NavigationRejectsTraversalAndMissingFolders)
{
    TempAssetRoot root;
    WriteFile(root.Path / "Textures" / "checker.png");
    WriteFile(root.Path / "Textures" / "crate.png");
    WriteFile(root.Path / "Scripts" / "spin.lua");

    ContentBrowser browser;
    browser.SetRoot(root.Path);
    ASTER_CHECK(browser.IsValid());
    ASTER_CHECK_EQ(browser.GetSubdirectories("").size(), 2U);

    ASTER_CHECK(browser.NavigateTo("Textures"));
    ASTER_CHECK_EQ(browser.GetCurrentPath(), std::string("Textures"));
    ASTER_CHECK_EQ(browser.GetItems().size(), 2U);
    ASTER_CHECK_EQ(browser.GetItems()[0].RelativePath, std::string("Textures/checker.png"));

    ASTER_CHECK(browser.NavigateUp());
    ASTER_CHECK(browser.GetCurrentPath().empty());

    ASTER_CHECK(!browser.NavigateTo("../Scripts"));
    ASTER_CHECK(browser.GetCurrentPath().empty());
    ASTER_CHECK(!browser.NavigateTo("Textures/../../"));
    ASTER_CHECK(browser.GetCurrentPath().empty());
    ASTER_CHECK(!browser.NavigateTo("/absolute"));
    ASTER_CHECK(browser.GetCurrentPath().empty());
    ASTER_CHECK(!browser.NavigateTo("Textures/../Scripts"));
    ASTER_CHECK(!browser.NavigateTo("Missing"));
    ASTER_CHECK(browser.GetCurrentPath().empty());

    // ".." is rejected even if it would resolve back inside the root.
    ASTER_CHECK(browser.NavigateTo("Scripts/."));
    ASTER_CHECK_EQ(browser.GetCurrentPath(), std::string("Scripts"));
    ASTER_CHECK(!browser.NavigateTo("Scripts/.."));
    ASTER_CHECK_EQ(browser.GetCurrentPath(), std::string("Scripts"));
}

ASTER_TEST(ContentBrowser, FilterIsCaseInsensitive)
{
    TempAssetRoot root;
    WriteFile(root.Path / "Textures" / "Checker.png");
    WriteFile(root.Path / "Textures" / "crate.png");
    WriteFile(root.Path / "Textures" / "readme.txt");

    ContentBrowser browser;
    browser.SetRoot(root.Path);
    ASTER_CHECK(browser.NavigateTo("Textures"));

    browser.SetFilter("CHECK");
    ASTER_CHECK_EQ(browser.GetItems().size(), 1U);
    ASTER_CHECK_EQ(browser.GetItems()[0].Name, std::string("Checker.png"));

    browser.SetFilter("TEX");
    ASTER_CHECK_EQ(browser.GetItems().size(), 0U);

    browser.SetFilter("read");
    ASTER_CHECK_EQ(browser.GetItems().size(), 1U);
    browser.SetFilter("");
    ASTER_CHECK_EQ(browser.GetItems().size(), 3U);
}

ASTER_TEST(ContentBrowser, CreateRenameAndDeleteAssets)
{
    TempAssetRoot root;
    WriteFile(root.Path / "Textures" / "checker.png");

    ContentBrowser browser;
    browser.SetRoot(root.Path);
    ASTER_CHECK(browser.IsValid());

    std::string error;
    ASTER_CHECK(browser.CreateFolder("NewFolder", &error));
    ASTER_CHECK(std::filesystem::is_directory(root.Path / "NewFolder"));
    ASTER_CHECK(!browser.CreateFolder("NewFolder", &error));
    ASTER_CHECK(!browser.CreateFolder("../escape", &error));
    ASTER_CHECK(!browser.CreateFolder("bad/name", &error));
    ASTER_CHECK(!std::filesystem::exists(root.Path / "escape"));

    const ContentItem* folder = FindItem(browser.GetItems(), "NewFolder");
    ASTER_CHECK(folder != nullptr);
    if (folder != nullptr)
    {
        ASTER_CHECK(browser.Rename(*folder, "Renamed", &error));
        ASTER_CHECK(!std::filesystem::exists(root.Path / "NewFolder"));
        ASTER_CHECK(std::filesystem::is_directory(root.Path / "Renamed"));
        ASTER_CHECK(!browser.Rename(*folder, "../../Renamed", &error));
    }

    const ContentItem* renamed = FindItem(browser.GetItems(), "Renamed");
    ASTER_CHECK(renamed != nullptr);
    if (renamed != nullptr)
    {
        ASTER_CHECK(browser.Delete(*renamed, &error));
        ASTER_CHECK(!std::filesystem::exists(root.Path / "Renamed"));
    }

    ASTER_CHECK(browser.NavigateTo("Textures"));
    const ContentItem* texture = FindItem(browser.GetItems(), "checker.png");
    ASTER_CHECK(texture != nullptr);
    if (texture != nullptr)
    {
        ASTER_CHECK(browser.Delete(*texture, &error));
        ASTER_CHECK(!std::filesystem::exists(root.Path / "Textures" / "checker.png"));
    }
}

ASTER_TEST(ContentBrowser, RefreshPicksUpExternalChanges)
{
    TempAssetRoot root;
    WriteFile(root.Path / "Existing.txt");

    ContentBrowser browser;
    browser.SetRoot(root.Path);
    ASTER_CHECK_EQ(browser.GetItems().size(), 1U);
    ASTER_CHECK_EQ(browser.GetSubdirectories("").size(), 0U);

    WriteFile(root.Path / "Added.txt");
    std::filesystem::create_directories(root.Path / "NewFolder");

    // The cached listing stays stable until Refresh is requested.
    ASTER_CHECK_EQ(browser.GetItems().size(), 1U);
    browser.Refresh();
    ASTER_CHECK_EQ(browser.GetItems().size(), 3U);
    ASTER_CHECK_EQ(browser.GetSubdirectories("").size(), 1U);
    ASTER_CHECK_EQ(browser.GetSubdirectories("")[0], std::string("NewFolder"));
}

ASTER_TEST(ContentBrowser, RejectsMissingRootAndEmptyNames)
{
    TempAssetRoot root;
    ContentBrowser browser;
    browser.SetRoot(root.Path / "missing");
    ASTER_CHECK(!browser.IsValid());
    ASTER_CHECK(browser.GetItems().empty());

    browser.SetRoot(root.Path);
    std::string error;
    ASTER_CHECK(!browser.CreateFolder("", &error));
    ASTER_CHECK(!browser.CreateFolder("..", &error));
    ASTER_CHECK(!browser.CreateFolder(".hidden", &error));
    ASTER_CHECK(!browser.CreateFolder("bad:name", &error));
}
