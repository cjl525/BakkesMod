#pragma once

#include "PresetTypes.h"

#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <filesystem>
#include <optional>

class PresetManager
{
public:
    PresetManager(std::shared_ptr<GameWrapper> gameWrapper,
                  std::shared_ptr<CVarManagerWrapper> cvarManager);

    void RefreshFromVanillaPresets();
    void LoadFromStorage();
    void SaveToStorage() const;

    [[nodiscard]] const CustomPresetCollection &GetPresets() const noexcept;
    [[nodiscard]] CustomPresetCollection &GetPresets() noexcept;

    std::optional<CustomPreset> FindPreset(const std::string &name) const;
    std::size_t FindPresetIndex(const std::string &name) const;

    void AddOrUpdatePreset(const CustomPreset &preset);
    void RemovePreset(const std::string &name);

private:
    std::shared_ptr<GameWrapper> gameWrapper;
    std::shared_ptr<CVarManagerWrapper> cvarManager;
    CustomPresetCollection presets;
    std::filesystem::path storageFilePath;
    std::filesystem::path vanillaPresetsPath;

    static constexpr std::string_view storageFileName{"expanded_presets.cfg"};

    static std::filesystem::path ResolveDataFolder(const std::shared_ptr<GameWrapper> &gameWrapper);
    static std::filesystem::path ResolveVanillaPresetPath(const std::shared_ptr<GameWrapper> &gameWrapper);

    void EnsureStorageDirectory() const;
    static std::vector<std::string> TokenizeLine(const std::string &line);
    static PresetPaintColor ParseColorToken(const std::string &token);
    static std::string SerializeColorToken(const PresetPaintColor &color);
};

