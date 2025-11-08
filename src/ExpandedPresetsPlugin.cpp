#include "ExpandedPresetsPlugin.h"

#include "imgui/imgui.h"

#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GuiManagerWrapper.h"
#include "bakkesmod/wrappers/canvaswrapper.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <sstream>
#include <vector>

using namespace std::string_literals;

BAKKESMOD_PLUGIN(ExpandedPresetsPlugin, "Expanded preset management with live previews", "1.0.0", PLUGINTYPE_FREEPLAY)

namespace
{
std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c)
                   {
                       return static_cast<char>(std::tolower(c));
                   });
    return value;
}

ImU32 ToImColor(const PresetPaintColor &color)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, 1.0f));
}
}

ExpandedPresetsPlugin::ExpandedPresetsPlugin() = default;

void ExpandedPresetsPlugin::onLoad()
{
    presetManager = std::make_unique<PresetManager>(gameWrapper, cvarManager);
    presetManager->LoadFromStorage();

    RegisterConsoleCommands();
    gameWrapper->RegisterDrawable(std::bind(&ExpandedPresetsPlugin::RenderCanvas, this, std::placeholders::_1));

    const auto menuName = GetMenuName();
    guiManager->RegisterHotkey(menuName, "Toggle expanded preset window", [this]
                               {
                                   *windowOpen = !*windowOpen;
                                   if (*windowOpen)
                                   {
                                       OnOpen();
                                   }
                                   else
                                   {
                                       OnClose();
                                   }
                               });

    guiManager->RegisterPluginWindow(shared_from_this());
    guiManager->RegisterPluginSettingsWindow(shared_from_this());

    ResetEditingPreset();
}

void ExpandedPresetsPlugin::onUnload()
{
    if (presetManager)
    {
        presetManager->SaveToStorage();
    }

    guiManager->RemoveHotkey(GetMenuName());
    guiManager->RemovePluginWindow(GetMenuName());
    guiManager->RemovePluginSettingsWindow();
}

void ExpandedPresetsPlugin::Render()
{
    if (!windowOpen || !*windowOpen)
    {
        return;
    }

    if (ImGui::Begin("Expanded Presets", windowOpen.get(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Columns(2, nullptr, false);

        ImGui::BeginChild("preset_list_child", ImVec2(280.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
        RenderPresetList();
        ImGui::EndChild();

        ImGui::NextColumn();
        ImGui::BeginChild("preset_editor_child", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
        RenderPresetEditor();
        ImGui::EndChild();

        ImGui::Columns(1);
    }
    ImGui::End();
}

void ExpandedPresetsPlugin::RenderCanvas(CanvasWrapper canvas)
{
    if (!windowOpen || !*windowOpen)
    {
        return;
    }

    if (!presetManager)
    {
        return;
    }

    const auto &presets = presetManager->GetPresets();
    if (selectedPresetIndex < 0 || selectedPresetIndex >= static_cast<int>(presets.size()))
    {
        return;
    }

    const auto &preset = presets[static_cast<std::size_t>(selectedPresetIndex)];

    canvas.SetColor(255, 255, 255, 255);
    canvas.SetPosition(35.0f, 35.0f);
    canvas.DrawString("Previewing preset: " + preset.name, 2.0f, 2.0f);
}

bool ExpandedPresetsPlugin::ShouldBlockInput()
{
    return windowOpen && *windowOpen;
}

bool ExpandedPresetsPlugin::IsActiveOverlay()
{
    return windowOpen && *windowOpen;
}

std::string ExpandedPresetsPlugin::GetMenuName()
{
    return "expandedpresets";
}

std::string ExpandedPresetsPlugin::GetMenuTitle()
{
    return "Expanded Presets";
}

void ExpandedPresetsPlugin::OnOpen()
{
    if (cvarManager)
    {
        cvarManager->log("ExpandedPresets: Window opened.");
    }
}

void ExpandedPresetsPlugin::OnClose()
{
    if (presetManager)
    {
        presetManager->SaveToStorage();
    }
    if (cvarManager)
    {
        cvarManager->log("ExpandedPresets: Window closed.");
    }
}

void ExpandedPresetsPlugin::RenderSettings()
{
    ImGui::TextUnformatted("Expanded Presets Plugin");
    ImGui::Separator();
    ImGui::TextWrapped("Use the \"Expanded Presets\" hotkey (default: unbound) or run 'expandedpresets_toggle' in the BakkesMod console to open the UI.");
    ImGui::TextWrapped("Presets are stored in the bakkesmod/data/ExpandedPresets/expanded_presets.cfg file. You can safely edit this file while Rocket League is closed.");
    if (ImGui::Button("Import vanilla presets now"))
    {
        ImportVanillaPresets();
    }
}

std::string ExpandedPresetsPlugin::GetPluginName()
{
    return "Expanded Presets";
}

void ExpandedPresetsPlugin::RegisterConsoleCommands()
{
    if (!cvarManager)
    {
        return;
    }

    windowOpen = std::make_shared<bool>(false);
    auto windowCvar = cvarManager->registerCvar("expandedpresets_window_open", "0", "Whether the expanded presets UI is visible", true, true, 0.0f, true, 1.0f);
    windowCvar.bindTo(windowOpen);

    cvarManager->registerNotifier("expandedpresets_toggle",
                                  [this](const std::vector<std::string> &)
                                  {
                                      *windowOpen = !*windowOpen;
                                      if (*windowOpen)
                                      {
                                          OnOpen();
                                      }
                                      else
                                      {
                                          OnClose();
                                      }
                                  },
                                  "Toggle the expanded presets window", PERMISSION_ALL);

    cvarManager->registerNotifier("expandedpresets_import",
                                  [this](const std::vector<std::string> &)
                                  {
                                      ImportVanillaPresets();
                                  },
                                  "Import presets from presets.data into the expanded manager", PERMISSION_ALL);

    cvarManager->registerNotifier("expandedpresets_import_bakkesplugins",
                                  [this](const std::vector<std::string> &args)
                                  {
                                      const bool overwrite = args.size() > 1 && (args[1] == "overwrite" || args[1] == "1");
                                      ImportBakkesPluginsCatalog(overwrite);
                                  },
                                  "Import presets exported from bakkesplugins.com", PERMISSION_ALL);
}

void ExpandedPresetsPlugin::RenderPresetList()
{
    auto &presets = presetManager->GetPresets();

    ImGui::Text("Presets (%zu)", presets.size());

    ImGui::InputTextWithHint("##preset_search", "Search by name or loadout code", &pendingFilter);

    if (ImGui::Button("Import vanilla"))
    {
        ImportVanillaPresets();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save all"))
    {
        presetManager->SaveToStorage();
    }
    ImGui::SameLine();
    const bool overwriteExisting = ImGui::GetIO().KeyShift;
    if (ImGui::Button("Import catalog"))
    {
        ImportBakkesPluginsCatalog(overwriteExisting);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Imports bakkesplugins.com car presets from bakkesplugins_cars.cfg.");
        ImGui::TextUnformatted("Hold Shift to overwrite presets with matching names.");
        ImGui::EndTooltip();
    }

    ImGui::Separator();

    const auto filterLower = ToLower(pendingFilter);

    if (ImGui::BeginChild("preset_list_scroller", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar))
    {
        std::vector<std::size_t> filteredIndices;
        filteredIndices.reserve(presets.size());

        for (std::size_t i = 0; i < presets.size(); ++i)
        {
            const auto &preset = presets[i];
            if (!filterLower.empty())
            {
                const auto nameLower = ToLower(preset.name);
                const auto codeLower = ToLower(preset.loadoutCode);
                if (nameLower.find(filterLower) == std::string::npos && codeLower.find(filterLower) == std::string::npos)
                {
                    continue;
                }
            }

            filteredIndices.push_back(i);
        }

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filteredIndices.size()));
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const auto index = filteredIndices[static_cast<std::size_t>(row)];
                const auto &preset = presets[index];
                const bool selected = static_cast<int>(index) == selectedPresetIndex;
                const std::string label = preset.name + "##preset_item_" + std::to_string(index);
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    selectedPresetIndex = static_cast<int>(index);
                    editingPreset = preset;
                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted("Loadout code:");
                    ImGui::TextWrapped("%s", preset.loadoutCode.c_str());
                    ImGui::EndTooltip();
                }
            }
        }
    }
    ImGui::EndChild();
}

void ExpandedPresetsPlugin::RenderPresetEditor()
{
    ImGui::TextUnformatted("Preset details");
    ImGui::Separator();

    ImGui::InputText("Name", &editingPreset.name);
    ImGui::InputText("Loadout code", &editingPreset.loadoutCode);
    ImGui::InputText("Car", &editingPreset.customization.carLabel);
    ImGui::InputText("Decal", &editingPreset.customization.decalLabel);
    ImGui::InputText("Wheels", &editingPreset.customization.wheelsLabel);

    ImGui::Checkbox("Matte paint finish", &editingPreset.customization.paintFinishMatte);
    ImGui::Checkbox("Pearlescent sheen", &editingPreset.customization.paintFinishPearlescent);

    float primaryColor[3] = {editingPreset.customization.primaryColor.r,
                             editingPreset.customization.primaryColor.g,
                             editingPreset.customization.primaryColor.b};
    float accentColor[3] = {editingPreset.customization.accentColor.r,
                            editingPreset.customization.accentColor.g,
                            editingPreset.customization.accentColor.b};

    if (ImGui::ColorEdit3("Primary color", primaryColor, ImGuiColorEditFlags_DisplayRGB))
    {
        editingPreset.customization.primaryColor = {primaryColor[0], primaryColor[1], primaryColor[2]};
    }
    if (ImGui::ColorEdit3("Accent color", accentColor, ImGuiColorEditFlags_DisplayRGB))
    {
        editingPreset.customization.accentColor = {accentColor[0], accentColor[1], accentColor[2]};
    }

    RenderPreviewPanel();

    if (ImGui::Button("Add / Update"))
    {
        if (editingPreset.name.empty() || editingPreset.loadoutCode.empty())
        {
            if (cvarManager)
            {
                cvarManager->log("ExpandedPresets: A preset name and loadout code are required.");
            }
        }
        else
        {
            presetManager->AddOrUpdatePreset(editingPreset);
            presetManager->SaveToStorage();
            selectedPresetIndex = static_cast<int>(presetManager->FindPresetIndex(editingPreset.name));
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset form"))
    {
        ResetEditingPreset();
    }

    if (selectedPresetIndex >= 0)
    {
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
        {
            auto &presets = presetManager->GetPresets();
            if (selectedPresetIndex < static_cast<int>(presets.size()))
            {
                presetManager->RemovePreset(presets[static_cast<std::size_t>(selectedPresetIndex)].name);
                presetManager->SaveToStorage();
                selectedPresetIndex = -1;
                ResetEditingPreset();
            }
        }

        if (ImGui::Button("Preview on car"))
        {
            ApplyPresetToCar(editingPreset, true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Equip preset"))
        {
            ApplyPresetToCar(editingPreset, false);
        }
    }
}

void ExpandedPresetsPlugin::RenderPreviewPanel()
{
    ImGui::Separator();
    ImGui::TextUnformatted("Preset preview");

    const float previewHeight = 160.0f;
    const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, previewHeight);
    ImGui::InvisibleButton("preset_preview_canvas", canvasSize);
    const ImVec2 canvasMin = ImGui::GetItemRectMin();
    const ImVec2 canvasMax = ImGui::GetItemRectMax();

    auto *drawList = ImGui::GetWindowDrawList();
    const ImU32 background = ImGui::ColorConvertFloat4ToU32(ImVec4(0.07f, 0.08f, 0.09f, 1.0f));
    drawList->AddRectFilled(canvasMin, canvasMax, background, 12.0f);

    const ImVec2 padding(18.0f, 18.0f);
    const ImVec2 bodyMin = canvasMin + padding;
    const ImVec2 bodyMax = canvasMax - padding;

    drawList->AddRectFilled(bodyMin, bodyMax, ToImColor(editingPreset.customization.primaryColor), 22.0f);

    const ImVec2 stripeMin = bodyMin + ImVec2(0.0f, (bodyMax.y - bodyMin.y) * 0.45f);
    const ImVec2 stripeMax = bodyMax - ImVec2(0.0f, (bodyMax.y - bodyMin.y) * 0.25f);
    drawList->AddRectFilled(stripeMin, stripeMax, ToImColor(editingPreset.customization.accentColor), 18.0f);

    const ImVec2 wheelRadius(30.0f, 30.0f);
    const ImVec2 leftWheelCenter(bodyMin.x + 60.0f, bodyMax.y - 25.0f);
    const ImVec2 rightWheelCenter(bodyMax.x - 60.0f, bodyMax.y - 25.0f);
    const ImU32 wheelColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    drawList->AddCircleFilled(leftWheelCenter, wheelRadius.x, wheelColor, 32);
    drawList->AddCircleFilled(rightWheelCenter, wheelRadius.x, wheelColor, 32);

    ImGui::SetCursorScreenPos(canvasMin + ImVec2(12.0f, 12.0f));
    ImGui::BeginGroup();
    ImGui::Text("Car: %s", editingPreset.customization.carLabel.c_str());
    ImGui::Text("Decal: %s", editingPreset.customization.decalLabel.c_str());
    ImGui::Text("Wheels: %s", editingPreset.customization.wheelsLabel.c_str());
    ImGui::Text("Finish: %s%s",
                editingPreset.customization.paintFinishMatte ? "Matte" : "Gloss",
                editingPreset.customization.paintFinishPearlescent ? ", Pearlescent" : "");
    ImGui::EndGroup();
}

void ExpandedPresetsPlugin::ImportVanillaPresets()
{
    if (!presetManager)
    {
        return;
    }

    const auto previousCount = presetManager->GetPresets().size();
    presetManager->RefreshFromVanillaPresets();
    presetManager->SaveToStorage();
    const auto newCount = presetManager->GetPresets().size();
    selectedPresetIndex = -1;
    ResetEditingPreset();

    if (cvarManager)
    {
        std::stringstream stream;
        stream << "ExpandedPresets: Imported " << newCount << " presets from presets.data";
        if (newCount < previousCount)
        {
            stream << " (duplicates were overwritten)";
        }
        cvarManager->log(stream.str());
    }
}

void ExpandedPresetsPlugin::ImportBakkesPluginsCatalog(bool overwriteExisting)
{
    if (!presetManager)
    {
        return;
    }

    const auto storageDir = presetManager->GetStorageDirectory();
    const std::filesystem::path catalogPath = storageDir / "bakkesplugins_cars.cfg";

    if (!std::filesystem::exists(catalogPath))
    {
        if (cvarManager)
        {
            cvarManager->log("ExpandedPresets: Catalog file not found at " + catalogPath.string());
            cvarManager->log("ExpandedPresets: Run the download script or copy bakkesplugins_cars.cfg into this folder.");
        }
        return;
    }

    const auto imported = presetManager->ImportFromFile(catalogPath, overwriteExisting);
    if (imported > 0)
    {
        selectedPresetIndex = -1;
        ResetEditingPreset();
    }
}

void ExpandedPresetsPlugin::ApplyPresetToCar(const CustomPreset &preset, bool previewOnly) const
{
    if (!cvarManager)
    {
        return;
    }

    try
    {
        const std::string command = previewOnly ? "cl_itemmod preview " : "cl_itemmod apply ";
        cvarManager->executeCommand(command + preset.loadoutCode);
    }
    catch (...)
    {
        cvarManager->log("ExpandedPresets: Your BakkesMod build does not support automated loadout previews. The preset code has been copied to your clipboard instead.");
    }

    try
    {
        cvarManager->setClipboardText(preset.loadoutCode);
    }
    catch (...)
    {
        // Clipboard access is optional; ignore errors so we do not crash on unsupported platforms.
    }

    if (previewOnly)
    {
        cvarManager->log("ExpandedPresets: Preview command triggered for preset '" + preset.name + "'.");
    }
    else
    {
        cvarManager->log("ExpandedPresets: Equipped preset '" + preset.name + "'.");
    }
}

void ExpandedPresetsPlugin::ResetEditingPreset()
{
    editingPreset = CustomPreset{};
    editingPreset.name.clear();
    editingPreset.loadoutCode.clear();
    editingPreset.customization.primaryColor = {0.18f, 0.18f, 0.18f};
    editingPreset.customization.accentColor = {0.9f, 0.35f, 0.15f};
    editingPreset.customization.carLabel = "Octane";
    editingPreset.customization.decalLabel = "None";
    editingPreset.customization.wheelsLabel = "OEM";
    editingPreset.customization.paintFinishMatte = false;
    editingPreset.customization.paintFinishPearlescent = false;
}

