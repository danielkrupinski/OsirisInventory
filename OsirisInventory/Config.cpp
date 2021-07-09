#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <system_error>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#include <ShlObj.h>
#endif

#include "nlohmann/json.hpp"

#include "Config.h"
#include "InventoryChanger/InventoryChanger.h"

Config::Config() noexcept
{
#ifdef _WIN32
    if (PWSTR pathToDocuments; SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &pathToDocuments))) {
        path = pathToDocuments;
        CoTaskMemFree(pathToDocuments);
    }
#else
    if (const char* homeDir = getenv("HOME"))
        path = homeDir;
#endif

    path /= "Osiris Inventory";
    listConfigs();

    load(u8"default.json", false);
}

void Config::load(size_t id, bool incremental) noexcept
{
    load(configs[id].c_str(), incremental);
}

void Config::load(const char8_t* name, bool incremental) noexcept
{
    json j;

    if (std::ifstream in{ path / name }; in.good()) {
        j = json::parse(in, nullptr, false, true);
        if (j.is_discarded())
            return;
    } else {
        return;
    }

    if (!incremental)
        reset();

    InventoryChanger::fromJson(j["Inventory Changer"]);
}

void removeEmptyObjects(json& j) noexcept
{
    for (auto it = j.begin(); it != j.end();) {
        auto& val = it.value();
        if (val.is_object() || val.is_array())
            removeEmptyObjects(val);
        if (val.empty() && !j.is_array())
            it = j.erase(it);
        else
            ++it;
    }
}

void Config::save(size_t id) const noexcept
{
    json j;

    j["Inventory Changer"] = InventoryChanger::toJson();

    removeEmptyObjects(j);

    createConfigDir();
    if (std::ofstream out{ path / configs[id] }; out.good())
        out << std::setw(2) << j;
}

void Config::add(const char8_t* name) noexcept
{
    if (*name && std::ranges::find(configs, name) == configs.cend()) {
        configs.emplace_back(name);
        save(configs.size() - 1);
    }
}

void Config::remove(size_t id) noexcept
{
    std::error_code ec;
    std::filesystem::remove(path / configs[id], ec);
    configs.erase(configs.cbegin() + id);
}

void Config::rename(size_t item, const char8_t* newName) noexcept
{
    std::error_code ec;
    std::filesystem::rename(path / configs[item], path / newName, ec);
    configs[item] = newName;
}

void Config::reset() noexcept
{
    InventoryChanger::resetConfig();
}

void Config::listConfigs() noexcept
{
    configs.clear();

    std::error_code ec;
    std::transform(std::filesystem::directory_iterator{ path, ec },
                   std::filesystem::directory_iterator{ },
                   std::back_inserter(configs),
                   [](const auto& entry) { return entry.path().filename().u8string(); });
}

void Config::createConfigDir() const noexcept
{
    std::error_code ec; std::filesystem::create_directory(path, ec);
}

void Config::openConfigDir() const noexcept
{
    createConfigDir();
#ifdef _WIN32
    ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    int ret = std::system(("xdg-open " + path.string()).c_str());
#endif
}
