/*
 * pixelart: turns text sprite sheets into PNG (and ICO) assets.
 * No editor, no window: edit the text, run the tool, commit the PNGs.
 *
 *   pixelart.exe [-o outdir] [-p palette] sheet.txt [more.txt ...]
 *
 * Sheet format:
 *
 *   # comment
 *   sprite md-icon x1 x2 x4      <- name, then the scales to write
 *   .1111.                        <- one row per line, all rows equal width
 *   11cc11                           one palette key per pixel, . = transparent
 *   ...
 *                                 <- a blank line (or the next sprite) ends it
 *
 * Each scale writes one PNG: x1 -> name.png, x4 -> name@4x.png. Pixels are
 * scaled by plain repetition, so they stay hard-edged. The extra word `ico`
 * also writes name.ico (Windows icon) holding every scale up to 256x256.
 *
 * The palette is PICO-8 unless -p names a palette file or one of the
 * palettes compiled into the exe from palettes/ (see README.md).
 *
 * Windows only, MSVC. The single dependency, stb_image_write, is vendored in
 * third_party/stb.
 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* stb_image_write: only the in-memory PNG encoder is used, so the stdio
   writers are compiled out. Static linkage leaves the BMP/TGA/JPG/HDR
   encoders unreferenced, which MSVC reports as C4505; that is expected. */
#ifdef _MSC_VER
#pragma warning(push, 3)
#pragma warning(disable : 4505)
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/* EMBEDDED_PALETTES: the .txt files in palettes/, generated at build time */
#include "palettes.h"

#ifndef PIXELART_VERSION
#define PIXELART_VERSION "dev"
#endif

#define MAX_DIM    256
#define MAX_SCALES 8
#define MAX_COLORS 92 /* printable ASCII (94) minus '.' and '#' */

/* ------------------------------------------------------------ palette ---- */

typedef struct {
    int8_t index[128];         /* key character -> colour index, -1 = none */
    uint8_t rgba[MAX_COLORS][4];
    char keys[MAX_COLORS + 1]; /* in definition order, for messages */
    int count;
    char source[MAX_PATH * 3]; /* "PICO-8" or the palette file */
} Palette;

static Palette pal;

static void palette_reset(void)
{
    memset(&pal, 0, sizeof pal);
    memset(pal.index, -1, sizeof pal.index);
}

static bool palette_add(char key, uint32_t rgba)
{
    if (pal.count == MAX_COLORS || pal.index[(unsigned char)key] >= 0) return false;
    pal.index[(unsigned char)key] = (int8_t)pal.count;
    pal.rgba[pal.count][0] = (uint8_t)(rgba >> 24);
    pal.rgba[pal.count][1] = (uint8_t)(rgba >> 16);
    pal.rgba[pal.count][2] = (uint8_t)(rgba >> 8);
    pal.rgba[pal.count][3] = (uint8_t)rgba;
    pal.keys[pal.count++] = key;
    return true;
}

/* The default: the PICO-8 palette, keys 0-9 a-f (A-F accepted too). */
static void palette_pico8(void)
{
    static const uint32_t PICO8[16] = {
        0x000000, 0x1D2B53, 0x7E2553, 0x008751, 0xAB5236, 0x5F574F, 0xC2C3C7, 0xFFF1E8,
        0xFF004D, 0xFFA300, 0xFFEC27, 0x00E436, 0x29ADFF, 0x83769C, 0xFF77A8, 0xFFCCAA,
    };
    palette_reset();
    for (int i = 0; i < 16; i++)
        palette_add("0123456789abcdef"[i], (PICO8[i] << 8) | 0xFF);
    for (int i = 0; i < 6; i++) pal.index['A' + i] = (int8_t)(10 + i);
    strcpy(pal.source, "PICO-8");
}

static bool valid_key(char c)
{
    return c > ' ' && c < 127 && c != '.' && c != '#';
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* "#RRGGBB" / "#RRGGBBAA" (the '#' is optional) -> 0xRRGGBBAA */
static bool parse_color(const char *s, size_t n, uint32_t *out)
{
    if (n && *s == '#') s++, n--;
    if (n != 6 && n != 8) return false;
    uint32_t v = 0;
    for (size_t i = 0; i < n; i++) {
        int h = hexval(s[i]);
        if (h < 0) return false;
        v = (v << 4) | (uint32_t)h;
    }
    *out = n == 6 ? (v << 8) | 0xFF : v;
    return true;
}

/* ------------------------------------------------------- diagnostics ---- */

static int written, errors;

static void fail(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    if (line > 0) fprintf(stderr, "%s:%d: error: ", file, line);
    else fprintf(stderr, "%s: error: ", file);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    errors++;
}

/* ------------------------------------------------- UTF-8 file system ---- */

static wchar_t *widen(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = n > 0 ? malloc((size_t)n * sizeof *w) : NULL;
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

static char *narrow(const wchar_t *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = n > 0 ? malloc((size_t)n) : NULL;
    if (s) WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}

static const char *last_error(void)
{
    static char msg[512];
    DWORD err = GetLastError();
    wchar_t wmsg[256];
    DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                             err, 0, wmsg, 256, NULL);
    while (n && (wmsg[n - 1] == L'\r' || wmsg[n - 1] == L'\n' || wmsg[n - 1] == L'.')) wmsg[--n] = 0;
    if (!n || !WideCharToMultiByte(CP_UTF8, 0, wmsg, -1, msg, sizeof msg, NULL, NULL))
        snprintf(msg, sizeof msg, "Windows error %lu", (unsigned long)err);
    return msg;
}

/* Whole file, NUL-terminated. */
static char *load_file(const char *path)
{
    wchar_t *wpath = widen(path);
    HANDLE h = wpath ? CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, NULL)
                     : INVALID_HANDLE_VALUE;
    free(wpath);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    char *text = NULL;
    LARGE_INTEGER size;
    if (GetFileSizeEx(h, &size) && size.QuadPart < 64 * 1024 * 1024) {
        DWORD len = (DWORD)size.QuadPart, got = 0;
        text = malloc((size_t)len + 1);
        if (text && ReadFile(h, text, len, &got, NULL) && got == len) {
            text[len] = 0;
        } else {
            free(text);
            text = NULL;
        }
    }
    CloseHandle(h);
    return text;
}

static bool save_file(const char *path, const void *a, size_t alen, const void *b, size_t blen)
{
    wchar_t *wpath = widen(path);
    HANDLE h = wpath ? CreateFileW(wpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL)
                     : INVALID_HANDLE_VALUE;
    free(wpath);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD n;
    bool ok = WriteFile(h, a, (DWORD)alen, &n, NULL) && n == alen;
    if (ok && blen) ok = WriteFile(h, b, (DWORD)blen, &n, NULL) && n == blen;
    if (!CloseHandle(h)) ok = false;
    return ok;
}

/* mkdir -p */
static bool make_dirs(const char *path)
{
    wchar_t *w = widen(path);
    if (!w) return false;
    for (wchar_t *p = w;; p++) {
        wchar_t c = *p;
        if (c != 0 && c != L'\\' && c != L'/') continue;
        /* skip "C:", the leading "\" and "\\server" prefixes */
        if (p > w && p[-1] != L':' && p[-1] != L'\\' && p[-1] != L'/') {
            *p = 0;
            CreateDirectoryW(w, NULL); /* failures show up in the check below */
            *p = c;
        }
        if (!c) break;
    }
    DWORD attr = GetFileAttributesW(w);
    free(w);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

/* ----------------------------------------------------- palette files ---- */

/*
 * One colour per line:   <key> <colour> [name ...]
 *   key     one printable character except '.', '#' and space
 *   colour  #RRGGBB or #RRGGBBAA ('#' optional)
 * '#' at the start of a line is a comment; blank lines are ignored.
 */
/* Parses palette text (modified in place); `path` names it in messages. */
static bool parse_palette(char *text, const char *path)
{
    int before = errors;
    palette_reset();
    snprintf(pal.source, sizeof pal.source, "%s", path);
    int line = 0;
    for (char *p = text; p && *p;) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        line++;
        char *l = p;
        p = nl ? nl + 1 : NULL;

        size_t n = strlen(l);
        while (n && (l[n - 1] == '\r' || l[n - 1] == ' ' || l[n - 1] == '\t')) l[--n] = 0;
        while (*l == ' ' || *l == '\t') l++;
        if (line == 1 && (unsigned char)l[0] == 0xEF && (unsigned char)l[1] == 0xBB &&
            (unsigned char)l[2] == 0xBF)
            l += 3; /* UTF-8 BOM */
        if (*l == 0 || *l == '#') continue;

        char key = l[0];
        if (!valid_key(key) || (l[1] != ' ' && l[1] != '\t')) {
            fail(path, line, "expected '<key> #RRGGBB [name]' where <key> is one character "
                             "other than '.', '#' or space");
            continue;
        }
        char *c = l + 1;
        while (*c == ' ' || *c == '\t') c++;
        size_t clen = strcspn(c, " \t");
        uint32_t rgba;
        if (!parse_color(c, clen, &rgba)) {
            fail(path, line, "'%.*s' is not a colour (use #RRGGBB or #RRGGBBAA)", (int)clen, c);
            continue;
        }
        if (pal.index[(unsigned char)key] >= 0) {
            fail(path, line, "key '%c' is defined twice", key);
            continue;
        }
        if (!palette_add(key, rgba)) {
            fail(path, line, "a palette holds at most %d colours", MAX_COLORS);
            break;
        }
    }
    if (errors == before && pal.count == 0) fail(path, 0, "the palette defines no colours");
    return errors == before;
}

/* A bundled palette by name ("db32", "DB32" or "db32.txt"), or -1. */
static int find_embedded(const char *name)
{
    size_t n = strlen(name);
    if (n > 4 && _stricmp(name + n - 4, ".txt") == 0) n -= 4;
    for (int i = 0; i < EMBEDDED_PALETTE_COUNT; i++)
        if (strlen(EMBEDDED_PALETTES[i].name) == n && _strnicmp(EMBEDDED_PALETTES[i].name, name, n) == 0)
            return i;
    return -1;
}

static bool is_file(const char *path)
{
    wchar_t *w = widen(path);
    DWORD attr = w ? GetFileAttributesW(w) : INVALID_FILE_ATTRIBUTES;
    free(w);
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

/* -p: a palette file if one exists at that path, else a bundled palette. */
static bool load_palette(const char *arg)
{
    char *text;
    char source[MAX_PATH * 3];
    if (is_file(arg)) {
        text = load_file(arg);
        if (!text) {
            fail(arg, 0, "can't read palette: %s", last_error());
            return false;
        }
        snprintf(source, sizeof source, "%s", arg);
    } else {
        int i = find_embedded(arg);
        if (i < 0) {
            fprintf(stderr, "error: '%s' is neither a palette file nor a bundled palette "
                            "(pixelart --list-palettes shows them)\n", arg);
            errors++;
            return false;
        }
        text = _strdup(EMBEDDED_PALETTES[i].text);
        if (!text) return false;
        snprintf(source, sizeof source, "%s", EMBEDDED_PALETTES[i].name);
    }
    bool ok = parse_palette(text, source);
    free(text);
    return ok;
}

/* --list-palettes: name and the title from the palette's first comment. */
static void list_palettes(void)
{
    for (int i = 0; i < EMBEDDED_PALETTE_COUNT; i++) {
        const char *t = EMBEDDED_PALETTES[i].text;
        int title_len = 0;
        const char *title = "";
        for (const char *l = t; *l;) {
            const char *e = strchr(l, '\n');
            size_t len = e ? (size_t)(e - l) : strlen(l);
            while (len && (*l == ' ' || *l == '\t')) l++, len--;
            if (len && *l == '#') {
                if (!*title) {
                    title = l + 1;
                    title_len = (int)len - 1;
                    while (title_len && *title == ' ') title++, title_len--;
                    while (title_len && (title[title_len - 1] == '\r' || title[title_len - 1] == ' '))
                        title_len--;
                }
            }
            if (!e) break;
            l = e + 1;
        }
        printf("%-20s %.*s\n", EMBEDDED_PALETTES[i].name, title_len, title);
    }
}

/* ------------------------------------------------------------ sprites ---- */

typedef struct {
    char name[128];
    int scales[MAX_SCALES], nscales;
    bool ico;
    int w, h;
    int8_t px[MAX_DIM * MAX_DIM]; /* colour index, -1 = transparent */
    const char *file;
    int line;
} Sprite;

static const char *outdir = ".";

static unsigned char *render(const Sprite *s, int scale)
{
    int w = s->w * scale, h = s->h * scale;
    unsigned char *img = malloc((size_t)w * (size_t)h * 4);
    if (!img) return NULL;
    for (int y = 0; y < h; y++) {
        unsigned char *row = img + (size_t)y * (size_t)w * 4;
        for (int x = 0; x < w; x++) {
            int c = s->px[(y / scale) * s->w + x / scale];
            unsigned char *p = row + x * 4;
            if (c < 0) memset(p, 0, 4);
            else memcpy(p, pal.rgba[c], 4);
        }
    }
    return img;
}

/* PNG bytes for one scale; free() the result. */
static unsigned char *encode_png(const Sprite *s, int scale, int *len)
{
    unsigned char *img = render(s, scale);
    if (!img) return NULL;
    int w = s->w * scale;
    unsigned char *png = stbi_write_png_to_mem(img, w * 4, w, s->h * scale, 4, len);
    free(img);
    return png;
}

static bool write_png(const Sprite *s, int scale)
{
    char path[1024];
    if (scale == 1) snprintf(path, sizeof path, "%s/%s.png", outdir, s->name);
    else snprintf(path, sizeof path, "%s/%s@%dx.png", outdir, s->name, scale);

    int len = 0;
    unsigned char *png = encode_png(s, scale, &len);
    if (!png) {
        fail(s->file, s->line, "'%s': out of memory", s->name);
        return false;
    }
    bool ok = save_file(path, png, (size_t)len, NULL, 0);
    free(png);
    if (!ok) {
        fail(s->file, s->line, "can't write %s: %s", path, last_error());
        return false;
    }
    printf("%s (%dx%d)\n", path, s->w * scale, s->h * scale);
    written++;
    return true;
}

static void put_le16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void put_le32(unsigned char *p, uint32_t v)
{
    put_le16(p, v & 0xffff);
    put_le16(p + 2, v >> 16);
}

/* A .ico whose entries are PNG-compressed images (Windows Vista and later),
   one per scale that fits the 256x256 icon limit. */
static bool write_ico(const Sprite *s)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.ico", outdir, s->name);
    if (s->w != s->h) {
        fail(s->file, s->line, "'%s': icons must be square, this is %dx%d", s->name, s->w, s->h);
        return false;
    }
    unsigned char *png[MAX_SCALES];
    int len[MAX_SCALES], dim[MAX_SCALES], n = 0;
    for (int i = 0; i < s->nscales; i++) {
        int d = s->w * s->scales[i];
        if (d > 256) continue;
        png[n] = encode_png(s, s->scales[i], &len[n]);
        if (!png[n]) {
            for (int j = 0; j < n; j++) free(png[j]);
            fail(s->file, s->line, "'%s': out of memory", s->name);
            return false;
        }
        dim[n++] = d;
    }
    if (n == 0) {
        fail(s->file, s->line, "'%s': no scale fits in a 256x256 icon", s->name);
        return false;
    }

    /* header + directory, then the PNGs back to back */
    size_t dirlen = 6 + 16 * (size_t)n, total = 0;
    for (int i = 0; i < n; i++) total += (size_t)len[i];
    unsigned char *buf = malloc(dirlen + total);
    bool ok = buf != NULL;
    if (ok) {
        memset(buf, 0, dirlen);
        put_le16(buf + 2, 1); /* type: icon */
        put_le16(buf + 4, (unsigned)n);
        uint32_t offset = (uint32_t)dirlen;
        for (int i = 0; i < n; i++) {
            unsigned char *ent = buf + 6 + 16 * i;
            ent[0] = ent[1] = (unsigned char)(dim[i] == 256 ? 0 : dim[i]); /* 0 means 256 */
            put_le16(ent + 4, 1);  /* colour planes */
            put_le16(ent + 6, 32); /* bits per pixel */
            put_le32(ent + 8, (uint32_t)len[i]);
            put_le32(ent + 12, offset);
            memcpy(buf + offset, png[i], (size_t)len[i]);
            offset += (uint32_t)len[i];
        }
        ok = save_file(path, buf, dirlen + total, NULL, 0);
        if (!ok) fail(s->file, s->line, "can't write %s: %s", path, last_error());
    } else {
        fail(s->file, s->line, "'%s': out of memory", s->name);
    }
    free(buf);
    for (int i = 0; i < n; i++) free(png[i]);
    if (!ok) return false;
    printf("%s (%d sizes)\n", path, n);
    written++;
    return true;
}

static void finish(Sprite *s)
{
    if (!s->name[0]) return;
    if (s->h == 0) {
        fail(s->file, s->line, "sprite '%s' has no rows", s->name);
    } else {
        for (int i = 0; i < s->nscales; i++) write_png(s, s->scales[i]);
        if (s->ico) write_ico(s);
    }
    s->name[0] = 0;
}

static bool valid_name(const char *n)
{
    if (!*n) return false;
    for (; *n; n++)
        if (!((*n >= 'a' && *n <= 'z') || (*n >= 'A' && *n <= 'Z') ||
              (*n >= '0' && *n <= '9') || *n == '-' || *n == '_'))
            return false;
    return true;
}

static void start(Sprite *s, char *args, const char *file, int line)
{
    char *save = NULL, *tok = strtok_s(args, " \t", &save);
    memset(s, 0, sizeof *s);
    s->file = file;
    s->line = line;
    if (!tok || !valid_name(tok)) {
        fail(file, line, "expected 'sprite <name> [x1 x2 ...] [ico]' (name: letters, digits, - _)");
        return;
    }
    snprintf(s->name, sizeof s->name, "%s", tok);
    while ((tok = strtok_s(NULL, " \t", &save)) != NULL) {
        if (strcmp(tok, "ico") == 0) {
            s->ico = true;
            continue;
        }
        char *end = tok;
        long k = (tok[0] == 'x' || tok[0] == 'X') ? strtol(tok + 1, &end, 10) : 0;
        if (k < 1 || k > 64 || *end) {
            fail(file, line, "bad option '%s' (use x1 .. x64 or ico)", tok);
            s->name[0] = 0;
            return;
        }
        if (s->nscales == MAX_SCALES) {
            fail(file, line, "at most %d scales per sprite", MAX_SCALES);
            s->name[0] = 0;
            return;
        }
        s->scales[s->nscales++] = (int)k;
    }
    if (s->nscales == 0) s->scales[s->nscales++] = 1;
}

static void add_row(Sprite *s, const char *row, const char *file, int line)
{
    int w = (int)strlen(row);
    if (s->h == 0) {
        if (w > MAX_DIM) {
            fail(file, line, "rows are limited to %d pixels", MAX_DIM);
            s->name[0] = 0;
            return;
        }
        s->w = w;
    } else if (w != s->w) {
        fail(file, line, "row is %d pixels wide, '%s' started with %d", w, s->name, s->w);
        s->name[0] = 0;
        return;
    }
    if (s->h == MAX_DIM) {
        fail(file, line, "sprites are limited to %d rows", MAX_DIM);
        s->name[0] = 0;
        return;
    }
    for (int x = 0; x < w; x++) {
        unsigned char ch = (unsigned char)row[x];
        int c = ch == '.' ? -1 : ch < 128 ? pal.index[ch] : -2;
        if (c == -2 || (c == -1 && ch != '.')) {
            fail(file, line, "column %d: '%c' is not a colour in %s (%s) or '.'",
                 x + 1, ch >= ' ' && ch < 127 ? ch : '?', pal.source, pal.keys);
            s->name[0] = 0;
            return;
        }
        s->px[s->h * MAX_DIM + x] = (int8_t)c;
    }
    s->h++;
}

/* Rows are stored with a MAX_DIM stride while parsing; pack them. */
static void pack(Sprite *s)
{
    for (int y = 1; y < s->h; y++) memmove(s->px + y * s->w, s->px + y * MAX_DIM, (size_t)s->w);
}

static void process(const char *file)
{
    char *text = load_file(file);
    if (!text) {
        fail(file, 0, "%s", last_error());
        return;
    }
    static Sprite s;
    s.name[0] = 0;
    bool skipping = false; /* rows of a sprite whose header failed */
    int line = 0;
    /* strtok would merge blank lines, so split by hand */
    for (char *p = text; p && *p;) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = 0;
        line++;
        char *l = p;
        p = nl ? nl + 1 : NULL;

        size_t n = strlen(l);
        while (n && (l[n - 1] == '\r' || l[n - 1] == ' ' || l[n - 1] == '\t')) l[--n] = 0;
        while (*l == ' ' || *l == '\t') l++;
        if (line == 1 && (unsigned char)l[0] == 0xEF && (unsigned char)l[1] == 0xBB &&
            (unsigned char)l[2] == 0xBF)
            l += 3; /* UTF-8 BOM */

        if (*l == '#') continue;
        if (*l == 0) {
            if (s.name[0]) { pack(&s); finish(&s); }
            skipping = false;
            continue;
        }
        if (strncmp(l, "sprite", 6) == 0 && (l[6] == ' ' || l[6] == '\t' || l[6] == 0)) {
            if (s.name[0]) { pack(&s); finish(&s); }
            start(&s, l + 6, file, line);
            skipping = !s.name[0];
            continue;
        }
        if (s.name[0]) add_row(&s, l, file, line);
        else if (!skipping)
            fail(file, line, "pixel row outside a sprite (start one with 'sprite <name>')");
        /* after an error, skip the rest of this block quietly */
        skipping = !s.name[0];
    }
    if (s.name[0]) { pack(&s); finish(&s); }
    free(text);
}

/* --------------------------------------------------------------- main ---- */

static void usage(FILE *f)
{
    fputs("usage: pixelart [-o outdir] [-p palette] sheet.txt [more.txt ...]\n"
          "\n"
          "  -o dir           write the images to dir (created if missing; default: .)\n"
          "  -p palette       use another palette instead of PICO-8: a palette file,\n"
          "                   or the name of a bundled one (e.g. -p db32)\n"
          "  --list-palettes  list the bundled palettes\n"
          "  --show-palette name\n"
          "                   print a bundled palette, e.g. to start your own\n"
          "  -h, --help       show this help\n"
          "  --version        print the version\n",
          f);
}

int wmain(int argc, wchar_t **wargv)
{
    SetConsoleOutputCP(CP_UTF8);

    char **argv = calloc((size_t)argc + 1, sizeof *argv);
    if (!argv) return 1;
    for (int i = 0; i < argc; i++) {
        argv[i] = narrow(wargv[i]);
        if (!argv[i]) return 1;
    }

    const char *palette_file = NULL;
    int files = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((strcmp(a, "-o") == 0 || strcmp(a, "-p") == 0) && i + 1 < argc) {
            if (a[1] == 'o') outdir = argv[++i];
            else palette_file = argv[++i];
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0 || strcmp(a, "/?") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(a, "--list-palettes") == 0) {
            list_palettes();
            return 0;
        } else if (strcmp(a, "--show-palette") == 0 && i + 1 < argc) {
            int k = find_embedded(argv[i + 1]);
            if (k < 0) {
                fprintf(stderr, "error: no bundled palette '%s' (see --list-palettes)\n", argv[i + 1]);
                return 1;
            }
            fputs(EMBEDDED_PALETTES[k].text, stdout);
            return 0;
        } else if (strcmp(a, "--version") == 0) {
            printf("pixelart %s\n", PIXELART_VERSION);
            return 0;
        } else if (a[0] == '-') {
            usage(stderr);
            return 2;
        } else {
            files++;
        }
    }
    if (files == 0) {
        usage(stderr);
        return 2;
    }

    if (palette_file) {
        if (!load_palette(palette_file)) return 1;
    } else {
        palette_pico8();
    }
    if (!make_dirs(outdir)) {
        fprintf(stderr, "error: can't create %s: %s\n", outdir, last_error());
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "-p") == 0) {
            i++;
            continue;
        }
        process(argv[i]);
    }
    printf("%d file%s written%s\n", written, written == 1 ? "" : "s",
           errors ? ", with errors" : "");
    return errors ? 1 : 0;
}
