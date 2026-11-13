# pixelart

A small Windows console tool that turns pixel art drawn as **text** into PNG
(and ICO) files. There is no editor and no window: you edit a text file, run
the tool and commit the images it writes.

```
pixelart.exe [-o outdir] [-p palette.txt] sheet.txt [more.txt ...]
```

| Option | Meaning |
| --- | --- |
| `-o dir` | Write the images to `dir` (created if missing). Default: the current directory. |
| `-p palette.txt` | Use the colours in `palette.txt` instead of the built-in PICO-8 palette. |
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

See [`examples/`](examples) for ready-to-run sheets.

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

Any other palette is a plain text file passed with `-p`. It replaces the
PICO-8 palette completely for that run.

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
  palette can hold up to 94 colours. Keys are case-sensitive and each may be
  defined only once.
- `<colour>` is `#RRGGBB`, or `#RRGGBBAA` for a partly transparent colour. The
  `#` is optional.
- Anything after the colour (a name, a note) is ignored.
- Blank lines and lines starting with `#` are ignored. The file is UTF-8 or
  ASCII; a BOM is fine.

[`palettes/pico8.txt`](palettes/pico8.txt) is the built-in palette written out
in this format, a good starting point for your own;
[`palettes/gameboy.txt`](palettes/gameboy.txt) is a 4-colour example.

```bat
pixelart -o out examples\pico8.txt
pixelart -p palettes\gameboy.txt -o out examples\gameboy.txt
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
is linked statically, so the exe is a single self-contained file.

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
