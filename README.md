# Znax Embedded
![DownloadCountTotal](https://img.shields.io/github/downloads/joyrider3774/znax_embedded/total?label=total%20downloads&style=plastic) ![DownloadCountLatest](https://img.shields.io/github/downloads/joyrider3774/znax_embedded/latest/total?style=plastic) ![LatestVersion](https://img.shields.io/github/v/tag/joyrider3774/znax_embedded?label=Latest%20version&style=plastic) ![License](https://img.shields.io/github/license/joyrider3774/znax_embedded?style=plastic)

Znax is a remake of a game by Nick Kouvaris. It is a sort of puzzle / arcade game where you as the player need to select 4 blocks of the same color as the corners of rectangles as big as you can. 

## Devices
Every [release](https://github.com/joyrider3774/znax_embedded/releases) has a build for every device. `releases/` is where a build of your own puts them, it is not part of the repository:

| Device | File | How to install |
| ------ | ---- | -------------- |
| [ESPboy](https://www.espboy.com/) | ESPboy_Znax.bin | flash it, the board is a LOLIN(WEMOS) D1 mini |
| [Gamebuino META](https://gamebuino.com/gamebuino-meta) | GamebuinoMeta_Znax.bin | copy it into a folder on the SD card, the .hex is for flashing it directly |
| [Adafruit PyBadge](https://www.adafruit.com/product/4200) | PyBadge_Znax.uf2 | double press reset and copy it onto the drive that appears |
| [Adafruit PyGamer](https://www.adafruit.com/product/4242) | PyGamer_Znax.uf2 | same as the PyBadge |
| [Pimoroni PicoSystem](https://shop.pimoroni.com/products/picosystem) | PicoSystem_Znax.uf2 | hold X while switching on and copy it onto the drive that appears |
| [Pimoroni Explorer](https://shop.pimoroni.com/products/explorer?variant=42092697845843) | Explorer_Znax.uf2 | hold BOOT while pressing RESET and copy it onto the drive that appears |
| [Pimoroni Tufty 2350](https://shop.pimoroni.com/products/tufty-2350?variant=55811986227579) | Tufty_Znax.uf2 | hold HOME while pressing RESET and copy it onto the drive that appears |
| [TinyCircuits Thumby Color](https://tinycircuits.com/products/thumby-color) | ThumbyColor_Znax.uf2 | put it into bootloader mode and copy it onto the RPI-RP2 drive that appears |
| [Playdate](https://play.date/) | Playdate_Znax.pdx.zip | unzip it and sideload Znax.pdx, the same pdx runs in the Playdate simulator |
| [Libretro / RetroArch](https://www.retroarch.com/) | Libretro_Znax.zip | copy znax_libretro.dll into RetroArch's cores folder and znax_libretro.info into its info folder, then Load Core and Start Core |
| [Game Boy Advance](https://en.wikipedia.org/wiki/Game_Boy_Advance) | GBA_Znax.gba | put it on a flash cart or open it in an emulator, the high scores are saved in the cartridge's SRAM |
| [PlayStation Portable](https://en.wikipedia.org/wiki/PlayStation_Portable) | PSP_Znax.PBP | rename it to EBOOT.PBP and put it in ms0:/PSP/GAME/Znax/ on the memory stick, or open it in PPSSPP |
| [PlayStation Vita](https://en.wikipedia.org/wiki/PlayStation_Vita) | Vita_Znax.vpk | install it with VitaShell on a Vita with homebrew enabled, or open it in Vita3K |
| Windows | Windows_Znax.exe | runs on its own, the high scores are saved next to it in Znax.sav |

`python tools/build_releases.py` builds all of them, `python tools/convert_skins.py` turns the images in `assets/skins` into the headers the game includes. The Playdate build also needs the Playdate SDK, see `playdate/CMakeLists.txt`, the libretro core libretro-common, see `libretro/CMakeLists.txt`, the Game Boy Advance build devkitARM and libgba, see `gba/CMakeLists.txt`, the PSP build the pspdev toolchain, see `psp/CMakeLists.txt` (pspdev has no Windows build, so on Windows it is built from WSL), and the Vita build VitaSDK, see `vita/CMakeLists.txt`.

### Buttons
The game's buttons on every device:

| Device | D-pad | A | B |
| ------ | ----- | - | - |
| ESPboy | d-pad | ACT | ESC |
| Gamebuino META | d-pad | A | B |
| Adafruit PyBadge | d-pad | A | B |
| Adafruit PyGamer | joystick | A | B |
| Pimoroni PicoSystem | d-pad | A | B |
| Pimoroni Explorer | A up, C down, B left, Y right | X | Z |
| Pimoroni Tufty 2350 | UP up, DOWN down, A left, C right | B | HOME |
| TinyCircuits Thumby Color | d-pad | A | B |
| Playdate | d-pad | A | B |
| Libretro | d-pad | A | B |
| Game Boy Advance | d-pad | A | B |
| PlayStation Portable | d-pad or the analog stick | Cross | Circle |
| PlayStation Vita | d-pad or the left stick | Cross | Circle |
| Windows | arrow keys | X | C |

On the Tufty 2350 a tap of HOME is B when it is let go. It has no speaker, the game is silent there. Holding RESET until the rear LEDs are dark puts it to sleep, a front button wakes it up again, with UP and DOWN held as well it goes into shipping mode instead.

The Thumby Color's display is 128x128, the game's own size, so it is shown 1:1 over the whole screen. That build has not been tried on the device itself yet.

The Playdate shows the black & white skin, scaled up in the middle of its display.

The Game Boy Advance shows the game scaled to 160x160 in the middle of its screen, with black bars at the sides.

On the PlayStation Portable the game is doubled to 256x256 in the middle of the display, and the high scores are saved next to the EBOOT.PBP in Znax.sav.

On the PlayStation Vita the game is blown up four times to 512x512 in the middle of the display, and the high scores are saved in ux0:data/Znax/Znax.sav.

On the Gamebuino META holding HOME for a second goes back to its loader.

## Game Features / changes:
- High score saving and loading
- Optimized Game time, games take less time than in original gp2x game
- Relative Timer game mode tweaks, you gain less time for clearing blocks compared to original gp2x game, this prevents endless games
- Removed Skin support compared to original gp2x game

## Playing the Game:
You as the player need to select 4 blocks of the same color of the corners of rectangles as big as you can. By doing so you will erase all blocks in this rectangle and they will be replaced by new blocks. You keep on doing this untill the time runs out, and try to gain your highest score possible. 

## Game Modes:
There are two game modes, Relative Timer and Fixed Timer.

### Fixed Timer:
In this mode you don't get extra time for deleting blocks but just points added to your score so here you try to get the highest amount of points in the given time period.

### Relative Timer:
In this mode you will gain extra time for deleting blocks and points added, you try to keep gaining time so you can play longer and gain higher scores. 

## Controls

| Button | Action |
| ------ | ------ |
| Dpad | Select menu's, Move cursor during game play|
| A | Confirm in menus, Select a block |
| B | Back in menu, gametype selector and game |
| (A) + Left + Down | Show or hide the debug info |

## Credits
Game Remake Created by Willems Davy, Original game by Nick Kouvaris. The original flash game is still available on [wayback machine](https://web.archive.org/web/20090220141735/http://lightforce.freestuff.gr/znax.php)

### Graphics
All Graphics were made by Willems Davy initially for the gp2x version in jasc paint shop pro 7. The graphics have been adapted for the espboy handheld resolution using gimp.
