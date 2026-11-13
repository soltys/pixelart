# pixelart

A small Windows console tool that turns pixel art drawn as **text** into PNG
(and ICO) files. There is no editor and no window: you edit a text file, run
the tool and commit the images it writes.

```
pixelart.exe [-o outdir] [-p palette] sheet.txt [more.txt ...]
```

| Option | Meaning |
| --- | --- |
| `-o dir` | Write the images to `dir` (created if missing). Default: the current directory. |
| `-p palette` | Use another palette instead of PICO-8: a palette file, or the name of a [bundled palette](#bundled-palettes) such as `db32`. |
| `--list-palettes` | List the bundled palettes. |
| `--show-palette name` | Print a bundled palette, e.g. `pixelart --show-palette db32 > my.txt` to start your own. |
| `-h`, `--help` | Show the usage. |
| `--version` | Print the version. |

Mistakes (unknown colour keys, ragged rows, bad scales, bad palette lines) are
reported as `file:line: error: ...`, the remaining sprites are still written
and the tool exits with status 1. Usage errors exit with status 2.

## Sprite sheets

A sheet holds any number of sprites. Every pixel is one character.

```
# comment
sprite ok x1 x2 x4
........
.......b
......bb
b....bb.
bb..bb..
.bbbb...
..bb....
........
```

- `sprite <name> [x1 x2 ...] [ico]` starts a sprite. The name may use letters,
  digits, `-` and `_`.
- `x1 x2 x4` are the sizes to write: `ok.png`, `ok@2x.png`, `ok@4x.png`
  (`x1` to `x64`, up to 8 of them; `x1` when none is given). Pixels are scaled
  by repetition, so edges stay sharp.
- `ico` also writes `<name>.ico` (PNG-compressed Windows icon) holding every
  size up to 256x256. The sprite must be square.
- Each following line is one row; all rows must be the same width (at most
  256x256 pixels). `.` is transparent, any other character is a key from the
  palette.
- A blank line, or the next `sprite` line, ends the sprite. Lines starting
  with `#` are comments.

See [`examples/`](examples) for ready-to-run sheets; the `examples` build
target renders them all (`cmake --build --preset release --target examples`).

## Palettes

Without `-p`, pixelart uses the 16-colour
[PICO-8 palette](https://pico-8.fandom.com/wiki/Palette): keys `0`-`9` and
`a`-`f` (`A`-`F` work too).

| Key | Colour | Key | Colour | Key | Colour | Key | Colour |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `0` | black `#000000` | `4` | brown `#AB5236` | `8` | red `#FF004D` | `c` | blue `#29ADFF` |
| `1` | dark blue `#1D2B53` | `5` | dark grey `#5F574F` | `9` | orange `#FFA300` | `d` | lavender `#83769C` |
| `2` | dark purple `#7E2553` | `6` | light grey `#C2C3C7` | `a` | yellow `#FFEC27` | `e` | pink `#FF77A8` |
| `3` | dark green `#008751` | `7` | white `#FFF1E8` | `b` | green `#00E436` | `f` | light peach `#FFCCAA` |

### palette.txt format

A palette is a plain text file. Pass its path with `-p`; it replaces the
PICO-8 palette completely for that run. (If no file exists at that path,
`-p` looks for a bundled palette of that name instead.)

```
# Game Boy greens
0 #0F380F darkest
1 #306230 dark
2 #8BAC0F light
3 #9BBC0F lightest
```

- One colour per line: `<key> <colour> [name]`.
- `<key>` is the single character used for that colour in sheets. Any
  printable ASCII character except `.` (transparent), `#` and space, so a
  palette can hold up to 92 colours. Keys are case-sensitive and each may be
  defined only once.
- `<colour>` is `#RRGGBB`, or `#RRGGBBAA` for a partly transparent colour. The
  `#` is optional.
- Anything after the colour (a name, a note) is ignored.
- Blank lines and lines starting with `#` are ignored. The file is UTF-8 or
  ASCII; a BOM is fine.

[`palettes/pico8.txt`](palettes/pico8.txt) is the built-in palette written out
in this format, a good starting point for your own.

### Bundled palettes

Every `.txt` file in `palettes/` is compiled into `pixelart.exe`, so these
work with nothing but the exe, by name (case-insensitive, `.txt` optional):

```bat
pixelart -p db32 -o out sheet.txt
pixelart --list-palettes
```

Besides `pico8`, they are the palettes that ship with
[Aseprite](https://github.com/aseprite/aseprite) (`data/extensions/*-palettes`),
converted from GIMP `.gpl` to this format. Keys follow Aseprite's colour
order: `0`-`9`, then `a`-`z`, then `A`-`Z`, then punctuation, so key `3` is
the colour at index 3 in Aseprite. Each file's header names the palette's
author, as credited by Aseprite.

| Name | Palette | Colours | Author |
| --- | --- | --- | --- |
| [`pico8`](palettes/pico8.txt) | PICO-8 (the built-in default) | 16 | Joseph White |
| [`arq4`](palettes/arq4.txt) | ARQ4 | 4 | ENDESGA Studios |
| [`cga0`](palettes/cga0.txt) | CGA0 | 4 | — |
| [`cga0hi`](palettes/cga0hi.txt) | CGA0 High | 4 | — |
| [`cga1`](palettes/cga1.txt) | CGA1 | 4 | — |
| [`cga1hi`](palettes/cga1hi.txt) | CGA1 High | 4 | — |
| [`cga3rd`](palettes/cga3rd.txt) | CGA3rd | 4 | — |
| [`cga3rdhi`](palettes/cga3rdhi.txt) | CGA3rd High | 4 | — |
| [`en4`](palettes/en4.txt) | EN4 | 4 | ENDESGA Studios |
| [`gameboy`](palettes/gameboy.txt) | Game Boy | 4 | — |
| [`virtualboy`](palettes/virtualboy.txt) | Virtual Boy | 4 | — |
| [`edg8`](palettes/edg8.txt) | EDG8 | 8 | ENDESGA Studios |
| [`matriax8c`](palettes/matriax8c.txt) | Matriax8c | 8 | Davit Masia |
| [`nyx8`](palettes/nyx8.txt) | nyx8 | 8 | Javier Guerrero |
| [`teletext`](palettes/teletext.txt) | Teletext | 8 | — |
| [`aap-micro12`](palettes/aap-micro12.txt) | AAP-Micro12 | 12 | Adigun A. Polack |
| [`monokai`](palettes/monokai.txt) | Monokai | 12 | — |
| [`aap-radiantxv`](palettes/aap-radiantxv.txt) | AAP-RadiantXV | 15 | Adigun A. Polack |
| [`a64`](palettes/a64.txt) | A64 | 16 | Arne Niklas Jansson |
| [`apple-ii`](palettes/apple-ii.txt) | Apple II | 16 | — |
| [`arne16`](palettes/arne16.txt) | ARNE16 | 16 | Arne Niklas Jansson |
| [`arq16`](palettes/arq16.txt) | ARQ16 | 16 | ENDESGA Studios |
| [`bubblegum-16`](palettes/bubblegum-16.txt) | BubbleGum16 | 16 | PineTreePizza |
| [`cg-arne`](palettes/cg-arne.txt) | CGArne | 16 | Arne Niklas Jansson |
| [`cga`](palettes/cga.txt) | CGA | 16 | — |
| [`commodore-vic20`](palettes/commodore-vic20.txt) | Commodore VIC-20 | 16 | — |
| [`commodore64`](palettes/commodore64.txt) | Commodore 64 | 16 | — |
| [`copper-tech`](palettes/copper-tech.txt) | Copper Tech | 16 | Arne Niklas Jansson |
| [`db16`](palettes/db16.txt) | DB16 | 16 | Richard Fhager |
| [`edg16`](palettes/edg16.txt) | EDG16 | 16 | ENDESGA Studios |
| [`enos16`](palettes/enos16.txt) | ENOS16 | 16 | ENDESGA Studios |
| [`eroge-copper`](palettes/eroge-copper.txt) | Eroge Copper | 16 | Arne Niklas Jansson |
| [`jmp`](palettes/jmp.txt) | JMP | 16 | Arne Niklas Jansson |
| [`msx1`](palettes/msx1.txt) | MSX1 | 16 | — |
| [`msx2`](palettes/msx2.txt) | MSX2 | 16 | — |
| [`psygnosia`](palettes/psygnosia.txt) | Psygnosia | 16 | Arne Niklas Jansson |
| [`simplejpc-16`](palettes/simplejpc-16.txt) | SimpleJPC-16 | 16 | Adigun A. Polack |
| [`solarized`](palettes/solarized.txt) | Solarized | 16 | — |
| [`win16`](palettes/win16.txt) | Win16 | 16 | — |
| [`zx-spectrum`](palettes/zx-spectrum.txt) | ZX Spectrum | 16 | — |
| [`smile-basic`](palettes/smile-basic.txt) | SmileBASIC | 18 | — |
| [`mail24`](palettes/mail24.txt) | mail24 | 24 | Hyohnoo |
| [`cpc`](palettes/cpc.txt) | CPC | 27 | — |
| [`arne32`](palettes/arne32.txt) | ARNE32 | 32 | Arne Niklas Jansson |
| [`cpc-boy`](palettes/cpc-boy.txt) | CPC Boy | 32 | Arne Niklas Jansson |
| [`db32`](palettes/db32.txt) | DB32 | 32 | Richard Fhager |
| [`edg32`](palettes/edg32.txt) | EDG32 | 32 | ENDESGA Studios |
| [`hept32`](palettes/hept32.txt) | HEPT32 | 32 | ENDESGA Studios |
| [`zughy-32`](palettes/zughy-32.txt) | Zughy-32 | 32 | Zughy |
| [`gameboy-color-type1`](palettes/gameboy-color-type1.txt) | Game Boy Color Type1 | 38 | — |
| [`rosy-42`](palettes/rosy-42.txt) | Rosy-42 | 42 | PineTreePizza |
| [`minecraft`](palettes/minecraft.txt) | Minecraft | 48 | — |
| [`nes`](palettes/nes.txt) | NES | 56 | — |
| [`nes-ntsc`](palettes/nes-ntsc.txt) | NES NTSC | 56 | — |
| [`aap-64`](palettes/aap-64.txt) | AAP-64 | 64 | Adigun A. Polack |
| [`master-system`](palettes/master-system.txt) | Master System | 64 | — |

Not included, because they have more colours than the 92 keys available:
AAP-Splendor128 (128), Atari 2600 NTSC (256), Atari 2600 PAL (128), Commodore Plus/4 (128), VGA 13h (256), Google UI (154), Web Safe Colors (216), X11 (138).

```bat
pixelart -o out examples\pico8.txt
pixelart -p gameboy -o out examples\gameboy.txt
```

## Building

Windows only, MSVC, CMake 4.0 or newer. Everything needed is in this
repository: the single dependency,
[stb_image_write](https://github.com/nothings/stb) (v1.18, public domain), is
vendored in `third_party/stb`.

```bat
cmake --workflow --preset default
```

builds `build\vs2026\Release\pixelart.exe` with Visual Studio 2026. The
`ninja-release` preset does the same from a Developer Command Prompt. The CRT
is linked statically and the palettes are compiled in, so the exe is a
single self-contained file. Adding a `.txt` file to `palettes/` bundles it on
the next build (`cmake/EmbedPalettes.cmake` turns them into
`build\...\generated\palettes.h`).

### Using it from another CMake project

```cmake
include(FetchContent)
FetchContent_Declare(pixelart
    GIT_REPOSITORY https://github.com/soltys/pixelart.git
    GIT_TAG        <commit>)
FetchContent_MakeAvailable(pixelart)

add_custom_target(art
    COMMAND pixelart -o ${CMAKE_SOURCE_DIR}/art/png ${CMAKE_SOURCE_DIR}/art/sprites.txt
    VERBATIM)
```

The `pixelart` target is then available; no install rules are added when it
is not the top-level project.
