#!/usr/bin/env python3
"""Генерирует src/font_data.h: растровые глифы (8-bit альфа) из моноширинного
TTF. Кириллица А-я, Ё, ё и ASCII. Запускается на компьютере разработчика.

    python3 tools/genfont.py > src/font_data.h

Шрифт DejaVu Sans Mono распространяется по свободной лицензии (Bitstream
Vera / public domain для изменений DejaVu).
"""
import sys
from PIL import Image, ImageDraw, ImageFont

REGULAR = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"

# (имя, файл, размер в пикселях)
FONTS = [("small", REGULAR, 15), ("big", BOLD, 22)]

codepoints = list(range(0x20, 0x7F)) + [0x401] + list(range(0x410, 0x450)) + [0x451]
# символы для оформления: многоточие, тире, стрелки не нужны (рисуем примитивами)
codepoints += [0x2026, 0x2014, 0xB7]
codepoints = sorted(set(codepoints))


def emit(name, path, size):
    font = ImageFont.truetype(path, size)
    asc, desc = font.getmetrics()
    w = int(round(font.getlength("M")))
    h = asc + desc
    print(f"#define FONT_{name.upper()}_W {w}")
    print(f"#define FONT_{name.upper()}_H {h}")
    print(f"static const unsigned char font_{name}_data[{len(codepoints)}][{w * h}] = {{")
    for cp in codepoints:
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((0, asc), chr(cp), fill=255, font=font, anchor="ls")
        data = list(img.getdata())
        print("  {" + ",".join(str(v) for v in data) + "},")
    print("};")


print("/* Сгенерировано tools/genfont.py — не править руками. */")
print("#ifndef FONT_DATA_H")
print("#define FONT_DATA_H")
print(f"#define FONT_GLYPHS {len(codepoints)}")
print("static const unsigned int font_codepoints[FONT_GLYPHS] = {")
print("  " + ",".join(str(c) for c in codepoints))
print("};")
for n, p, s in FONTS:
    emit(n, p, s)
print("#endif")
