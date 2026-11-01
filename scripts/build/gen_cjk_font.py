#!/usr/bin/env python3
"""Generate a 16x16 CJK bitmap font for the Chinese characters used in the
Ziphu.OS UI. Source: GNU Unifont (a 16x16 monochrome bitmap font with full
CJK coverage). Output: src/font_cjk.c with a lookup table {codepoint, glyph}.

Each glyph is 16 rows x 16 cols = 32 bytes (2 bytes per row, big-endian bit
order: bit 15 = leftmost pixel). This matches the 8x16 ASCII font's bit7=
leftmost convention, doubled in width."""
import sys
from PIL import Image, ImageDraw, ImageFont

# Collect every CJK / CJK-punct character that appears in gui.c source.
CHARS = (
    "中主于件体关内动启器回处存崩已序应志拟文日机浅深溃理用程简管系统置色虚言设语返重题驱"
    "，。：；！？、（）【】"
    # also keep the language label glyphs even if not in the scanned set
    "文泉驿点阵正黑等宽"
)
# de-dup, keep CJK only (drop punctuation that has no Unifont mono glyph issues)
seen = set()
uniq = []
for c in CHARS:
    if c not in seen:
        seen.add(c)
        uniq.append(c)
print("chars:", "".join(uniq), "count:", len(uniq), file=sys.stderr)

FONT = "/usr/share/fonts/opentype/unifont/unifont.otf"
f = ImageFont.truetype(FONT, 16)

def render_glyph(ch):
    """Render one char to a 16x16 1bpp bitmap; return 32 bytes."""
    img = Image.new("1", (16, 16), 0)
    d = ImageDraw.Draw(img)
    # Unifont is exactly 16px; use anchor 'la' (left-ascender) so the glyph
    # sits in the cell. Some chars need a 1px nudge.
    d.text((0, 0), ch, font=f, fill=1)
    px = img.load()
    out = bytearray(32)
    for row in range(16):
        lo = 0
        hi = 0
        for col in range(16):
            if px[col, row]:
                if col < 8:
                    hi |= (0x80 >> col)
                else:
                    lo |= (0x80 >> (col - 8))
        out[row * 2]     = hi
        out[row * 2 + 1] = lo
    return bytes(out)

# Sanity: confirm glyphs are non-empty
table = []
for c in uniq:
    g = render_glyph(c)
    n = sum(bin(b).count("1") for b in g)
    if n == 0:
        print("WARN: empty glyph for", repr(c), c, file=sys.stderr)
    table.append((ord(c), g))

# Emit C
with open("/home/z/my-project/kernel/src/font_cjk.c", "w") as out:
    out.write("/* ==========================================================================\n")
    out.write(" *  Ziphu.OS 1.0  -  OpenZK  -  src/font_cjk.c\n")
    out.write(" *  16x16 CJK bitmap glyphs for the Simplified-Chinese UI strings.\n")
    out.write(" *  Generated from GNU Unifont by scripts/build/gen_cjk_font.py.\n")
    out.write(" *  Each glyph = 32 bytes: 16 rows x 2 bytes (bit15=leftmost pixel).\n")
    out.write(" * ========================================================================== */\n")
    out.write('#include "zk_types.h"\n\n')
    out.write("typedef struct { u32 cp; u8 g[32]; } cjk_entry_t;\n\n")
    out.write("const cjk_entry_t zk_font_cjk[] = {\n")
    for cp, g in table:
        h = ", ".join("0x%02X" % b for b in g)
        out.write("    { 0x%04XU, { %s } },\n" % (cp, h))
    out.write("};\n")
    out.write("const u32 zk_font_cjk_count = sizeof(zk_font_cjk)/sizeof(zk_font_cjk[0]);\n")
print("wrote src/font_cjk.c with", len(table), "glyphs", file=sys.stderr)
