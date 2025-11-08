#include "PresetManager.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "bakkesmod/wrappers/GameWrapper.h"

namespace
{
std::string TrimWhitespace(const std::string &text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}
} // namespace

PresetManager::PresetManager(std::shared_ptr<GameWrapper> gameWrapper,
                             std::shared_ptr<CVarManagerWrapper> cvarManager)
    : gameWrapper(std::move(gameWrapper)),
      cvarManager(std::move(cvarManager))
{
    const auto dataFolder = ResolveDataFolder(this->gameWrapper);
    storageFilePath = dataFolder / storageFileName;
    vanillaPresetsPath = ResolveVanillaPresetPath(this->gameWrapper);
    EnsureStorageDirectory();
}

void PresetManager::RefreshFromVanillaPresets()
{
    presets.clear();

    if (vanillaPresetsPath.empty() || !std::filesystem::exists(vanillaPresetsPath))
    {
        cvarManager->log("ExpandedPresets: Could not find vanilla presets.data file to import presets.");
        return;
    }

    std::ifstream file(vanillaPresetsPath);
    if (!file)
    {
        cvarManager->log("ExpandedPresets: Failed to open vanilla presets file: " + vanillaPresetsPath.string());
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        line = TrimWhitespace(line);
        if (line.empty() || line.front() == '#')
        {
            continue;
        }

        const auto delimiterPos = line.find_last_of("\t ");
        if (delimiterPos == std::string::npos)
        {
            continue;
        }

        CustomPreset preset;
        preset.name = TrimWhitespace(line.substr(0, delimiterPos));
        preset.loadoutCode = TrimWhitespace(line.substr(delimiterPos + 1));
        if (preset.name.empty() || preset.loadoutCode.empty())
        {
            continue;
        }

        AddOrUpdatePreset(preset);
    }
}

void PresetManager::LoadFromStorage()
{
    presets.clear();

    std::ifstream file(storageFilePath);
    if (!file)
    {
        cvarManager->log("ExpandedPresets: No stored presets were found, importing from presets.data instead.");
        RefreshFromVanillaPresets();
        SaveToStorage();
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        line = TrimWhitespace(line);
        if (line.empty() || line.front() == '#')
        {
            continue;
        }

        const auto tokens = TokenizeLine(line);
        if (tokens.size() < 2)
        {
            continue;
        }

        CustomPreset preset;
        preset.name = TrimWhitespace(tokens[0]);
        preset.loadoutCode = TrimWhitespace(tokens[1]);

        if (tokens.size() >= 3)
        {
            preset.customization.primaryColor = ParseColorToken(tokens[2]);
        }
        if (tokens.size() >= 4)
        {
            preset.customization.accentColor = ParseColorToken(tokens[3]);
        }
        if (tokens.size() >= 5)
        {
            preset.customization.carLabel = TrimWhitespace(tokens[4]);
        }
        if (tokens.size() >= 6)
        {
            preset.customization.decalLabel = TrimWhitespace(tokens[5]);
        }
        if (tokens.size() >= 7)
        {
            preset.customization.wheelsLabel = TrimWhitespace(tokens[6]);
        }
        if (tokens.size() >= 8)
        {
            preset.customization.paintFinishMatte = tokens[7] == "1" || tokens[7] == "true" || tokens[7] == "matte";
        }
        if (tokens.size() >= 9)
        {
            preset.customization.paintFinishPearlescent = tokens[8] == "1" || tokens[8] == "true" || tokens[8] == "pearlescent";
        }

        AddOrUpdatePreset(preset);
    }
}

void PresetManager::SaveToStorage() const
{
    EnsureStorageDirectory();
    std::ofstream file(storageFilePath, std::ios::trunc);
    if (!file)
    {
        cvarManager->log("ExpandedPresets: Failed to open storage file for writing: " + storageFilePath.string());
        return;
    }

    for (const auto &preset : presets)
    {
        file << preset.name << '|'
             << preset.loadoutCode << '|'
             << SerializeColorToken(preset.customization.primaryColor) << '|'
             << SerializeColorToken(preset.customization.accentColor) << '|'
             << preset.customization.carLabel << '|'
             << preset.customization.decalLabel << '|'
             << preset.customization.wheelsLabel << '|'
             << (preset.customization.paintFinishMatte ? '1' : '0') << '|'
             << (preset.customization.paintFinishPearlescent ? '1' : '0')
             << '\n';
    }
}

const CustomPresetCollection &PresetManager::GetPresets() const noexcept
{
    return presets;
}

CustomPresetCollection &PresetManager::GetPresets() noexcept
{
    return presets;
}

std::optional<CustomPreset> PresetManager::FindPreset(const std::string &name) const
{
    const auto it = std::find_if(presets.begin(), presets.end(),
                                 [&name](const CustomPreset &preset)
                                 {
                                     return preset.name == name;
                                 });
    if (it == presets.end())
    {
        return std::nullopt;
    }

    return *it;
}

std::size_t PresetManager::FindPresetIndex(const std::string &name) const
{
    const auto it = std::find_if(presets.begin(), presets.end(),
                                 [&name](const CustomPreset &preset)
                                 {
                                     return preset.name == name;
                                 });
    if (it == presets.end())
    {
        return presets.size();
    }

    return static_cast<std::size_t>(std::distance(presets.begin(), it));
}

void PresetManager::AddOrUpdatePreset(const CustomPreset &preset)
{
    const auto index = FindPresetIndex(preset.name);
    if (index >= presets.size())
    {
        presets.push_back(preset);
    }
    else
    {
        presets[index] = preset;
    }
}

void PresetManager::RemovePreset(const std::string &name)
{
    const auto index = FindPresetIndex(name);
    if (index >= presets.size())
    {
        return;
    }

    presets.erase(presets.begin() + static_cast<std::ptrdiff_t>(index));
}

std::filesystem::path PresetManager::ResolveDataFolder(const std::shared_ptr<GameWrapper> &gameWrapper)
{
    std::filesystem::path dataFolder{"./bakkesmod/data"};

    if (gameWrapper)
    {
        try
        {
            const auto folder = gameWrapper->GetDataFolder();
            if (!folder.empty())
            {
                dataFolder = folder;
            }
        }
        catch (...)
        {
            // If the BakkesMod SDK we are linked against does not provide GetDataFolder
            // just keep the fallback path instead of crashing.
        }
    }

    dataFolder /= "ExpandedPresets";
    return dataFolder;
}

std::filesystem::path PresetManager::ResolveVanillaPresetPath(const std::shared_ptr<GameWrapper> &gameWrapper)
{
    std::filesystem::path path{"./bakkesmod/data/presets.data"};
    if (gameWrapper)
    {
        try
        {
            const auto folder = gameWrapper->GetDataFolder();
            if (!folder.empty())
            {
                path = std::filesystem::path(folder) / "presets.data";
            }
        }
        catch (...)
        {
            // Fall back to default path.
        }
    }

    return path;
}

void PresetManager::EnsureStorageDirectory() const
{
    const auto directory = storageFilePath.parent_path();
    if (!directory.empty() && !std::filesystem::exists(directory))
    {
        std::filesystem::create_directories(directory);
    }
}

std::vector<std::string> PresetManager::TokenizeLine(const std::string &line)
{
    std::vector<std::string> tokens;
    std::stringstream stream(line);
    std::string token;
    while (std::getline(stream, token, '|'))
    {
        tokens.push_back(token);
    }

    return tokens;
}

PresetPaintColor PresetManager::ParseColorToken(const std::string &token)
{
    PresetPaintColor color{};

    std::stringstream stream(token);
    std::string component;
    std::size_t index = 0;
    while (std::getline(stream, component, ','))
    {
        float value = 0.0f;
        try
        {
            value = std::stof(component);
        }
        catch (...)
        {
            value = 0.0f;
        }

        value = std::max(0.0f, value);
        const float normalized = value > 1.0f ? value / 255.0f : value;
        switch (index)
        {
        case 0: color.r = normalized; break;
        case 1: color.g = normalized; break;
        case 2: color.b = normalized; break;
        default: break;
        }
        ++index;
    }

    return color;
}

std::string PresetManager::SerializeColorToken(const PresetPaintColor &color)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3)
           << color.r << ','
           << color.g << ','
           << color.b;
    return stream.str();
}

