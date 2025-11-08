#pragma once

#include "PresetManager.h"

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/plugin/PluginWindow.h"

#include <memory>
#include <string>

class ExpandedPresetsPlugin final : public BakkesMod::Plugin::BakkesModPlugin,
                                    public BakkesMod::Plugin::PluginSettingsWindow,
                                    public BakkesMod::Plugin::PluginWindow,
                                    public std::enable_shared_from_this<ExpandedPresetsPlugin>
{
public:
    ExpandedPresetsPlugin();

    void onLoad() override;
    void onUnload() override;

    // PluginWindow
    void Render() override;
    void RenderCanvas(CanvasWrapper canvas) override;
    std::string GetMenuName() override;
    std::string GetMenuTitle() override;
    bool ShouldBlockInput() override;
    bool IsActiveOverlay() override;
    void OnOpen() override;
    void OnClose() override;

    // PluginSettingsWindow
    void RenderSettings() override;
    std::string GetPluginName() override;

private:
    std::unique_ptr<PresetManager> presetManager;
    std::shared_ptr<bool> windowOpen;

    std::string pendingFilter;
    CustomPreset editingPreset{};
    int selectedPresetIndex{-1};

    void RegisterConsoleCommands();
    void RenderPresetList();
    void RenderPresetEditor();
    void RenderPreviewPanel();
    void ImportVanillaPresets();
    void ImportBakkesPluginsCatalog(bool overwriteExisting);
    void ApplyPresetToCar(const CustomPreset &preset, bool previewOnly) const;
    void ResetEditingPreset();
};

