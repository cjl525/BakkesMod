# Expanded Presets BakkesMod Plugin

The Expanded Presets plugin extends Rocket League's standard preset management by adding a fully featured UI for creating, previewing, and organising custom presets. It imports the vanilla `presets.data` file, allows you to add metadata such as paint finishes and wheels, and offers a stylised preview that highlights how the preset will look before you equip it.

## Features

- 🗂️ **Preset library** – Import the stock `presets.data` file and curate an unlimited list of presets with search and filtering.
- 🎨 **Customization options** – Store car, decal, wheel labels, and paint finish metadata alongside each loadout code. Override primary and accent paint colours using convenient RGB pickers.
- 👀 **Live preview** – The ImGui-based preview panel renders a stylised car silhouette coloured with your chosen paints so you can visualise the preset instantly.
- 🚀 **Quick apply / preview** – Send the loadout code to BakkesMod's console command system for previewing or equipping. The loadout code is copied to the clipboard so you can manually import it if your build does not support automatic commands.
- 💾 **Persistent storage** – All presets are saved to `bakkesmod/data/ExpandedPresets/expanded_presets.cfg`. The format is human readable for easy sharing and editing.
- 🌐 **BakkesPlugins catalog import** – Download thousands of community presets from [bakkesplugins.com/cars](https://bakkesplugins.com/cars) and merge them into your library.

## Getting started

1. **Clone the repository** and grab a copy of the [BakkesMod SDK](https://github.com/bakkesmodorg/BakkesModSDK). Point the `BM_SDK_DIR` CMake cache variable at the SDK checkout.
2. **Configure the project** with CMake:

   ```bash
   cmake -S . -B build -DBM_SDK_DIR=/path/to/BakkesModSDK
   cmake --build build --config Release
   ```

   The compiled plugin (`ExpandedPresets.dll`) will be placed in the `bin/` directory.

3. **Install the plugin** by copying the DLL into your `BakkesMod/plugins/` folder and add the following line to `plugins.cfg`:

   ```
   plugin load ExpandedPresets
   ```

4. Launch Rocket League and bind the **“Expanded Presets”** hotkey in the BakkesMod F2 → Plugins tab to open the UI.

## Data format

Custom presets are stored using the pipe-delimited format described below:

```
Name|LoadoutCode|primaryR,primaryG,primaryB|accentR,accentG,accentB|Car|Decal|Wheels|MatteFlag|PearlescentFlag
```

- Colour components accept either 0–1 floats or 0–255 values.
- `MatteFlag` / `PearlescentFlag` accept `0`, `1`, `true`, `false`, `matte`, or `pearlescent`.

## Commands

| Command | Description |
| --- | --- |
| `expandedpresets_toggle` | Toggle the Expanded Presets ImGui window. |
| `expandedpresets_import` | Re-import presets from the vanilla `presets.data` file. |
| `expandedpresets_import_bakkesplugins` | Import presets from `bakkesplugins_cars.cfg` located in the data folder. |

A persistent CVar named `expandedpresets_window_open` is also exposed so you can tie the UI to an external toggle or execute it via binds.

## Importing the BakkesPlugins.com catalog

1. Install the Python dependencies (optional but recommended):

   ```bash
   pip install requests cloudscraper
   ```

2. Run the helper script to download the catalogue. By default this generates `bakkesplugins_cars.cfg` in the current directory.

   ```bash
   python tools/download_bakkesplugins_cars.py
   ```

   Use `--limit 100` while testing to avoid hammering the API. The script falls back to standard `requests` headers, but installing `cloudscraper` helps bypass Cloudflare challenges if you hit HTTP 403 errors.

3. Copy the generated file into `bakkesmod/data/ExpandedPresets/`. A sample file containing three presets is available at `data/bakkesplugins_cars.sample.cfg`.

4. Launch Rocket League, open the Expanded Presets UI, and click **Import catalog**. Hold **Shift** while clicking (or pass `overwrite` to the `expandedpresets_import_bakkesplugins` command) to overwrite any presets with matching names.

The importer merges thousands of presets in a few seconds and persists them to your storage file.

## Preview rendering

The preview intentionally focuses on paints, finish, and wheel accents to provide a quick visual comparison without requiring full 3D rendering support. The editor still stores the full loadout code, so equipping the preset applies your Rocket League loadout exactly.

## Contributing

Issues and pull requests are welcome! If you extend the plugin with additional metadata, keep the storage format backwards compatible where possible.

