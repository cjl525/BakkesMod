#pragma once

#include <string>
#include <vector>

struct PresetPaintColor
{
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};

    [[nodiscard]] bool operator==(const PresetPaintColor &other) const noexcept
    {
        return r == other.r && g == other.g && b == other.b;
    }
};

struct PresetCustomization
{
    PresetPaintColor primaryColor{0.18f, 0.18f, 0.18f};
    PresetPaintColor accentColor{0.9f, 0.35f, 0.15f};
    std::string carLabel{"Octane"};
    std::string decalLabel{"None"};
    std::string wheelsLabel{"OEM"};
    bool paintFinishMatte{false};
    bool paintFinishPearlescent{false};
};

struct CustomPreset
{
    std::string name;
    std::string loadoutCode;
    PresetCustomization customization{};
};

using CustomPresetCollection = std::vector<CustomPreset>;

