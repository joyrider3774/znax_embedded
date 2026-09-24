# Znax Embedded
![DownloadCountTotal](https://img.shields.io/github/downloads/joyrider3774/znax_embedded/total?label=total%20downloads&style=plastic) ![DownloadCountLatest](https://img.shields.io/github/downloads/joyrider3774/znax_embedded/latest/total?style=plastic) ![LatestVersion](https://img.shields.io/github/v/tag/joyrider3774/znax_embedded?label=Latest%20version&style=plastic) ![License](https://img.shields.io/github/license/joyrider3774/znax_embedded?style=plastic)

Znax is a remake of a game by Nick Kouvaris. It is a sort of puzzle / arcade game where you as the player need to select 4 blocks of the same color as the corners of rectangles as big as you can. 

## Screenshots
The browser build, at twice the game's own 128x128:

| Title screen | In game |
| --- | --- |
| ![Znax title screen](metadata/screenshots/title.png) | ![Znax in game](metadata/screenshots/ingame.png) |

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
| Nintendo DS | d-pad | A | B |
| Nintendo 3DS | d-pad or circle pad | A | B |
| Nintendo 64 | d-pad | A | B |
| PlayStation | d-pad | Cross | Circle |
| PlayStation Portable | d-pad or the analog stick | Cross | Circle |
| PlayStation Vita | d-pad or the left stick | Cross | Circle |
| Windows | arrow keys | X | C |
| MS-DOS | arrow keys | X | C |
| Browser | arrow keys | X | C |

On the Tufty 2350 a tap of HOME is B when it is let go.

On the Gamebuino META holding HOME for a second goes back to its loader.

## Devices
Every [release](https://github.com/joyrider3774/znax_embedded/releases) has a build for every device. `releases/` is where a build of your own puts them, it is not part of the repository:

| Device | File | How to install |
| ------ | ---- | -------------- |
| [ESPboy](https://www.espboy.com/) | ESPboy_Znax.bin | flash it, the board is a LOLIN(WEMOS) D1 mini |
| [Gamebuino META](https://gamebuino.com/gamebuino-meta) | GamebuinoMeta_Znax.zip | unzip it onto the SD card, it holds a Znax_embedded folder with the game, its save and the loader's images, the .hex in it is for flashing the game directly |
| [Adafruit PyBadge](https://www.adafruit.com/product/4200) | PyBadge_Znax.uf2 | double press reset and copy it onto the drive that appears |
| [Adafruit PyGamer](https://www.adafruit.com/product/4242) | PyGamer_Znax.uf2 | same as the PyBadge |
| [Pimoroni PicoSystem](https://shop.pimoroni.com/products/picosystem) | PicoSystem_Znax.uf2 | hold X while switching on and copy it onto the drive that appears |
| [Pimoroni Explorer](https://shop.pimoroni.com/products/explorer?variant=42092697845843) | Explorer_Znax.uf2 | hold BOOT while pressing RESET and copy it onto the drive that appears |
| [Pimoroni Tufty 2350](https://shop.pimoroni.com/products/tufty-2350?variant=55811986227579) | Tufty_Znax.uf2 | hold HOME while pressing RESET and copy it onto the drive that appears |
| [TinyCircuits Thumby Color](https://tinycircuits.com/products/thumby-color) | ThumbyColor_Znax.uf2 | put it into bootloader mode and copy it onto the RPI-RP2 drive that appears |
| [Playdate](https://play.date/) | Playdate_Znax.pdx.zip | unzip it and sideload Znax.pdx, the same pdx runs in the Playdate simulator |
| [Libretro / RetroArch](https://www.retroarch.com/) | Libretro_Znax.zip | copy znax_libretro.dll into RetroArch's cores folder and znax_libretro.info into its info folder, then Load Core and Start Core |
| [Game Boy Advance](https://en.wikipedia.org/wiki/Game_Boy_Advance) | GBA_Znax.gba | put it on a flash cart or open it in an emulator, the high scores are saved in the cartridge's SRAM |
| [Nintendo DS](https://en.wikipedia.org/wiki/Nintendo_DS) | NDS_Znax.nds | put it on a flash card or open it in an emulator, the high scores are saved next to it in Znax.sav |
| [Nintendo 3DS](https://en.wikipedia.org/wiki/Nintendo_3DS) | 3DS_Znax.3dsx | copy it into /3ds/ on the SD card and start it from the Homebrew Launcher, or open it in an emulator, the high scores are saved in sdmc:/3ds/Znax/ |
| [Nintendo 64](https://en.wikipedia.org/wiki/Nintendo_64) | N64_Znax.z64 | put it on a flash cart or open it in an emulator, the high scores are saved in the cartridge EEPROM |
| [PlayStation](https://en.wikipedia.org/wiki/PlayStation_(console)) | PSX_Znax.exe | open it in an emulator or send it to a console that runs unsigned code, the progress is not saved yet |
| [PlayStation Portable](https://en.wikipedia.org/wiki/PlayStation_Portable) | PSP_Znax.PBP | rename it to EBOOT.PBP and put it in ms0:/PSP/GAME/Znax/ on the memory stick, or open it in PPSSPP |
| [PlayStation Vita](https://en.wikipedia.org/wiki/PlayStation_Vita) | Vita_Znax.vpk | install it with VitaShell on a Vita with homebrew enabled, or open it in Vita3K |
| Windows | Windows_Znax.exe | runs on its own, the high scores are saved next to it in Znax.sav |
| MS-DOS | DOS_Znax.zip | unzip ZNAX.EXE onto a DOS machine or into DOSBox and run it, the high scores are saved next to it in ZNAX.SAV |
| MS-DOS, not dithered | DOS_Znax_ND.zip | the same program with `DITHERING` 0, unzip ZNAX_ND.EXE and run it the same way. On a 256 colour screen a shade the palette has no colour for is the nearer one it does have, instead of a pattern of the two |
| Browser | Web_Znax.zip | upload it to an itch.io HTML project, or unzip it and open index.html from a web server, the high scores are saved in the browser |

The Tufty 2350 has no speaker, the game is silent there. Holding RESET until the rear LEDs are dark puts it to sleep, a front button wakes it up again, with UP and DOWN held as well it goes into shipping mode instead.

The Thumby Color's display is 128x128, the game's own size, so it is shown 1:1 over the whole screen. That build has not been tried on the device itself yet.

The Playdate shows the black & white skin, scaled up in the middle of its display.

The Game Boy Advance shows the game scaled to 160x160 in the middle of its screen, with black bars at the sides.

On the Nintendo DS the game is scaled to 192x192 in the middle of the top screen, with black bars at the sides, and the bottom screen stays dark. What the game saves goes into Znax.sav on the card it was started from, so a card that libfat can not write to (or an emulator without one) plays the game but forgets it afterwards. Its tones are square waves played as a sample: the DS's own tone channels count their frequency in a 16 bit timer and can not go below about 256 Hz.

On the Nintendo 3DS the game is scaled to 240x240 in the middle of the top screen, with black bars at the sides, and the bottom screen stays dark. What the game saves goes into sdmc:/3ds/Znax/Znax.sav. Its tones play through the console's DSP when the DSP firmware has been dumped to the SD card (sdmc:/3ds/dspfirm.cdc), and through CSND when it has not: on hardware either one plays, in an emulator only the DSP one does.

On MS-DOS the game runs in VGA mode X, 320x240 in 256 colours, blown up to 240x240 in the middle of the screen with black bars at the sides. That mode rather than the usual 320x200 one because its pixels are square, where 320x200 is stretched over the same screen and would show the game a fifth too tall. The 256 colours are set to the RGB332 cube, which is exactly what the game's 8 bpp screen buffer holds, so a frame reaches the card without a colour being worked out. Its tones are a square wave on the PC speaker, the high scores are saved next to the program in ZNAX.SAV, and Escape quits. The program is 32 bit and carries the CWSDPMI host inside it, so it needs nothing beside it on the disk.

In a browser the game is drawn into a canvas of its own 128x128 pixels, which the page stretches to whatever room it is given while keeping it square and keeping the pixels sharp. The high scores are saved in the browser's localStorage under the game's name, so a private window plays it but forgets it afterwards. The zip holds index.html, index.js and index.wasm and is what an itch.io HTML project takes as it is.

On the Nintendo 64 the game is drawn into memory in the colours the RDP takes and the RDP shows it scaled to 240x240 in the middle of its 320x240 screen, with black bars at the sides. Its tones are a square wave written into the buffers the sound hardware plays from. The high scores are saved in the cartridge EEPROM, which the ROM says it has, so a cartridge or an emulator without one plays the game but forgets it afterwards.

On the PlayStation the game is drawn into memory in the colours the GPU takes, handed to it as a texture and shown scaled to 240x240 in the middle of its 320x240 screen, with black bars at the sides. Its tones are a square wave the SPU plays from a single looping block. The memory card is not written yet, so what the game saves is gone when the console is switched off.

On the PlayStation Portable the game is doubled to 256x256 in the middle of the display, and the high scores are saved next to the EBOOT.PBP in Znax.sav.

On the PlayStation Vita the game is blown up four times to 512x512 in the middle of the display, and the high scores are saved in ux0:data/Znax/Znax.sav.

## Building
`python tools/build_releases.py` builds a release for every device  
`python tools/convert_skins.py` turns the images in `assets/skins` into the headers the game includes

### Where the tools are
The script looks for everything in the place it is installed in here. A tool somewhere else is passed on the command line, or set as the environment variable in the last column and left off the command line:

| Option | What it points at | Default, or environment variable |
| ------ | ----------------- | -------------------------------- |
| `--arduino-cli PATH` | arduino-cli, which builds the Arduino devices | `ARDUINO_CLI` |
| `--arduino DIR` | the Arduino IDE 1.8 folder, used when there is no arduino-cli | `C:/arduino`, `ARDUINO_DIR` |
| `--lovyangfx DIR` | LovyanGFX for the Windows build, when it is not the one in the sketchbook | `LOVYANGFX_DIR` |
| `--msys2 DIR` | MSYS2's mingw64 bin folder, for cmake and ninja | `C:/msys64/mingw64/bin`, `MSYS2_BIN` |
| `--playdate-sdk DIR` | the Playdate SDK | `C:/playdate/PlaydateSDK`, `PLAYDATE_SDK_PATH` |
| `--playdate-arm DIR` | the bin folder of the ARM gcc the Playdate needs | `PLAYDATE_ARM_BIN` |
| `--libretro-common DIR` | libretro-common | `C:/github/libretro-common`, `LIBRETRO_COMMON_DIR` |
| `--devkitpro DIR` | devkitARM with libgba, libnds, calico, libctru and tools | `C:/devkitarm`, `DEVKITPRO` |
| `--psn00bsdk DIR` | PSn00bSDK | `C:/psn00bsdk`, `PSN00BSDK_PREFIX` |
| `--n64 DIR` | the mips64-elf toolchain with libdragon | `C:/n64_dev`, `N64_INST` |
| `--emsdk DIR` | the Emscripten SDK | `C:/github/emsdk`, `EMSDK` |
| `--dosdev DIR` | DJGPP with CWSDPMI | `C:/dos_dev`, `DOSDEV` |
| `--pspdev DIR` | the pspdev toolchain | `C:/psp_dev`, `PSPDEV_DIR` |
| `--vitasdk DIR` | VitaSDK | `C:/psvita_dev`, `VITASDK` |
| `--sdl2-mingw DIR` | SDL2's mingw package, its x86_64-w64-mingw32 folder | `SDL2_MINGW` |

Only the devices being built need their tool, so one missing toolchain does not stop the rest:

```
python tools/build_releases.py --only N64 DOS --n64 D:/n64_dev --dosdev D:/dos_dev
python tools/build_releases.py --list          shows what would be built
python tools/build_releases.py --only Web      one device only
```

### Build settings
Every device is built with its own settings. These change them for all of the devices at once, and `--list` shows what the defines would be without building anything. They are the same defines the device headers and the `platforms/*/CMakeLists.txt` files take, so a single device can be built with `-D<name>=<value>` from cmake instead:

| Option | What it sets | Values |
| ------ | ------------ | ------ |
| `--forceskin N` | `FORCESKIN`, the skin built in | `-1`, or `0` to `1` |
| `--forcescreenbuffer N` | `SCREENBUFFER`, where drawing goes | `0`, `1`, `8` or `16` bits per pixel |
| `--forcescale N` | `SCALESCREEN`, how the game fills the display | `1` blown up, `0` 1:1 in the middle |
| `--forcewindowscale N` | `WINDOW_SCALE`, how big the Windows window opens | `1` to `8` times the game's size |
| `--forcedithering N` | `DITHERING`, whether an 8 or 1 bpp buffer spreads its colours | `1` spread, `0` the nearest colour |
| `--forcedebug` | `FORCEDEBUG 1`, the debug header is always shown | no value, on when it is given |

`-1` is the default skin, or the black and white one with a 1 bpp buffer. There is no skin option in the game, so only the skin that is used is built in and `--forceskin` is what picks it.

Not every device takes every buffer mode, `platforms/<device>/CMakeLists.txt` says which, and one it does not take stops that build with a message. A 1 bpp buffer can only show the one skin that is black and white, so it forces that skin whatever `--forceskin` says, see `FORCESKIN` in `defines.h`.

```
python tools/build_releases.py --forceskin 1                only skin 1, on every device
python tools/build_releases.py --only Windows --forcescreenbuffer 1
python tools/build_releases.py --list --forcedebug          what the defines would be
```

### Board packages and libraries
The Arduino devices are built with arduino-cli 1.5.1 and the versions below. They are the ones every release is built with, `.github/workflows/build-releases.yml` pins them:

| Device | Board package | Libraries |
| ------ | ------------- | --------- |
| ESPboy | esp8266:esp8266 3.1.2 | LovyanGFX 1.1.9, TFT_eSPI 2.4.72 |
| Gamebuino META | gamebuino:samd 1.2.2 | Gamebuino META 1.3.3 |
| Adafruit PyBadge, PyGamer | adafruit:samd 1.7.16 | Adafruit GFX Library 1.12.6, Adafruit ST7735 and ST7789 Library 1.5.15, Adafruit BusIO 1.17.4, Adafruit NeoPixel 1.15.5, Adafruit SPIFlash 5.1.1 |
| PicoSystem, Explorer, Tufty 2350, Thumby Color | rp2040:rp2040 5.5.0 | none, everything they use comes with the core |

The ESPboy draws through LovyanGFX and only includes TFT_eSPI's header, so the exact TFT_eSPI does not matter much.  
The Gamebuino's core needs Arduino's own arduino:samd 1.8.14 beside it for sam.h, without it the build stops at "sam.h: No such file or directory".  
The Windows build draws through the same LovyanGFX 1.1.9, see `platforms/windows/CMakeLists.txt`.

### Toolchains
The Playdate build also needs the Playdate SDK, see `platforms/playdate/CMakeLists.txt`  
The libretro core needs libretro-common, see `platforms/libretro/CMakeLists.txt`  
The Game Boy Advance build needs devkitARM and libgba, see `platforms/gba/CMakeLists.txt`  
The Nintendo DS build needs devkitARM, libnds and calico, see `platforms/nds/CMakeLists.txt`  
The Nintendo 3DS build needs devkitARM and libctru, see `platforms/3ds/CMakeLists.txt`  
The PlayStation build needs PSn00bSDK, see `platforms/psx/CMakeLists.txt`  
The Nintendo 64 build needs the mips64-elf toolchain and libdragon, see `platforms/n64/CMakeLists.txt`  
The PSP build needs the pspdev toolchain, see `platforms/psp/CMakeLists.txt` (pspdev has no Windows build, so on Windows it is built from WSL)  
The Vita build needs VitaSDK, see `platforms/vita/CMakeLists.txt`  
The browser build needs Emscripten, see `platforms/web/CMakeLists.txt`  
The MS-DOS build needs DJGPP, see `platforms/dos/CMakeLists.txt`

## Credits
Game Remake Created by Willems Davy, Original game by Nick Kouvaris. The original flash game is still available on [wayback machine](https://web.archive.org/web/20090220141735/http://lightforce.freestuff.gr/znax.php)

### Graphics
All Graphics were made by Willems Davy initially for the gp2x version in jasc paint shop pro 7. The graphics have been adapted for the espboy handheld resolution using gimp.
