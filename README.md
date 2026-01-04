# The Other Side Updater

A simple Nintendo Switch homebrew updater for Atmosphere, Hekate, and custom firmware packs.

This app lets you update your CFW (Atmosphere + Hekate) directly on your Switch with one click. It also supports downloading custom firmware packs (ready for Daybreak) and self-updates the app itself.

**Features:**
- Update Atmosphere (latest from your pack)
- Update Hekate (latest from your pack)
- Combined "Update CFW" option (both + auto-reboot)
- Download multiple custom firmware packs (extracted to `/firmware/` for Daybreak)
- Self-update the updater app (no manual .nro replacement)
- Download progress bar
- Confirmation prompts
- Full power-off reboot (works on all Switches)
- Clean console UI

**Important Notes:**
- Use on Atmosphere CFW only.
- Firmware packs are extracted to SD card — install with Daybreak.nro (safe offline method).
- Keep your pack links permanent for evergreen updates.

## Download

**[Download the latest version here](https://github.com/eradicatinglove/the-other-side-updater/releases/latest/download/the_other_side_updater.nro)**

(Direct .nro file — place at `/switch/the_other_side_updater.nro` on your SD card)

## Screenshots

(Coming soon)

## Installation

1. Click the download link above.
2. Copy `the_other_side_updater.nro` to `/switch/` on your SD card.
3. Launch from the Homebrew Menu (hold R on a game for full RAM recommended).

## Usage

- Navigate with Up/Down, confirm with A.
- "Update CFW" → both packs + reboot.
- Firmware → select pack → download .zip / extract zip yourself → use Daybreak.
- "Update App" → new .nro → exit and relaunch.

## Building from Source

Requires devkitPro with libnx.

make clean 
make


## Credits

- Built with libnx and minizip.
- Thanks to the Atmosphere and Hekate teams.

## Disclaimer

Use at your own risk. For educational purposes only.

---
