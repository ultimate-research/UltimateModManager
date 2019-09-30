# Ultimate Mod Manager

## Download

Download the latest from [the releases page](https://github.com/ultimate-research/UltimateModManager/releases).

## Installation
- Place the NRO in `sdmc:/switch/`
- Put mods in `sdmc:/UltimateModManager/mods` in any folder structure. Folders at the top-level here will be the selectable modpacks. All compressed mod files need to start with their offset in hexadecimal, such as `0x35AA26F08_model.numatb`. All uncompressed mod files must be in the ARC folder layout and have the correct original naming. An example of a correct uncompressed mod would be `UltimateModManager/mods/organized_css/ui/param/database/ui_chara_db.prc`, and it would show up in the application as `organized_css`.

## Usage
- Use the DataArcDumper to dump your `data.arc` first if you have not. If you want to use this functionality, please start the HBL by overriding Smash itself, which is done by starting Smash while holding the R button.
- Use the FTP server to transfer over mods to your Switch over your Wi-Fi connection. Open this server, use its IP address in conjunction with either a file transfer client like WinSCP or the ArcInstaller. Completely optional, as mods can be transferred over via the SD card as well.


## Build from source

### Dependencies
- [A DEVKITPRO environment](https://devkitpro.org/wiki/Getting_Started)
- Install the mbedtls library: `pacman -S switch-mbedtls`

At this point, you can build with a simple `make`.
