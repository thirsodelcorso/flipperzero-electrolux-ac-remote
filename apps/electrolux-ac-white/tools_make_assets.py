"""Prepara os icones (assets/) a partir dos icones do Midea AC Remote (MIT, (c) 2025 Vladimir Stoliarov,
https://github.com/xakep666/flipperzero-midea-ac-remote) e deriva as variantes de que o Electrolux precisa.
Uso: python3 -I tools_make_assets.py <pasta com os PNG originais> <pasta de saida>"""
import sys, shutil, os
sys.path.append("/home/claude/.local/lib/python3.12/site-packages")
from PIL import Image
SRC, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
USED = ['on_19x20','on_hover_19x20','off_19x20','off_hover_19x20','cold_19x20','cold_hover_19x20','fan_19x20','fan_hover_19x20',
        'tempup_24x21','tempup_hover_24x21','tempdown_24x21','tempdown_hover_24x21','frame_30x39',
        'power_text_21x5','mode_text_17x5','fan_text_12x5','swing_text_20x5']
for n in USED: shutil.copy(f'{SRC}/{n}.png', f'{OUT}/{n}.png')

def load(n):
    im = Image.open(f'{SRC}/{n}.png').convert('RGB').convert('L')
    return [[1 if im.getpixel((x, y)) < 128 else 0 for x in range(im.width)] for y in range(im.height)]
def save(n, g):
    h, w = len(g), len(g[0]); im = Image.new('RGB', (w, h), (255, 255, 255))
    for y in range(h):
        for x in range(w):
            if g[y][x]: im.putpixel((x, y), (0, 0, 0))
    im.save(f'{OUT}/{n}.png')
def clone(g): return [r[:] for r in g]
def fill(g, x0, x1, y0, y1, v):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1): g[y][x] = v

# ---- 19x20: molduras ----
off, off_h, swing, swing_h = load('off_19x20'), load('off_hover_19x20'), load('swing_19x20'), load('swing_hover_19x20')
blank = clone(off); fill(blank, 2, 16, 1, 16, 0); fill(blank, 1, 17, 2, 16, 0)        # moldura sem triangulo
blank_h = clone(off_h); fill(blank_h, 3, 15, 3, 15, 1)                                  # hover sem triangulo
tri = clone(swing); fill(tri, 1, 16, 4, 14, 0); fill(tri, 1, 16, 15, 16, 0)             # moldura COM triangulo (swing sem o glifo)
tri_h = clone(swing_h); fill(tri_h, 3, 16, 3, 15, 1)
# FAN: moldura com triangulo (segurar OK diminui) + helice do "Samsung AC Remote" (dappermint, MIT, (c) 2026 Lia Yoffe,
# https://github.com/dappermint/samsung-ac-remote-flipper-app), icone fan_speed_2, subida 3 linhas p/ abrir espaco as barras
def shift_fan(name, bg, dy):
    g = load(name); blk = [r[3:16] for r in g[4:15]]
    fill(g, 3, 15, 4, 14, bg)
    for yy, row in enumerate(blk):
        for xx, v in enumerate(row): g[4 + yy - dy][xx + 3] = v
    orig = load(name)
    for y in range(0, 5):                                  # recoloca o triangulo (canto superior direito)
        for x in range(14, 19): g[y][x] = orig[y][x]
    return g
save('fanbtn_19x20', shift_fan('fan_speed_2_19x20', 0, 2)); save('fanbtn_hover_19x20', shift_fan('fan_speed_2_hover_19x20', 1, 2))
glyph = [[swing[y][x] if (4 <= y <= 14 and 1 <= x <= 16) else 0 for x in range(19)] for y in range(20)]
sw = clone(blank); sw_h = clone(blank_h)
for y in range(20):
    for x in range(19):
        if glyph[y][x]: sw[y][x] = 1; sw_h[y][x] = 0
save('swingnt_19x20', sw); save('swingnt_hover_19x20', sw_h)                           # SWING sem triangulo

# ---- 19x11: molduras ----
turbo, turbo_h, led, led_h = load('turbo_19x11'), load('turbo_hover_19x11'), load('led_19x11'), load('led_hover_19x11')
b11 = clone(turbo); fill(b11, 1, 17, 1, 7, 0)
b11_h = clone(turbo_h); fill(b11_h, 1, 17, 1, 7, 1)
def compose(name, pixels):
    n, nh = clone(b11), clone(b11_h)
    for (x, y) in pixels: n[y][x] = 1; nh[y][x] = 0
    save(name + '_19x11', n); save(name + '_hover_19x11', nh)
led_px = [(x, y) for y in range(3, 8) for x in range(3, 16) if led[y][x]]
compose('lednt', led_px)
E = ["###", "#..", "###", "#..", "###"]; C = ["###", "#..", "#..", "#..", "###"]; O = ["###", "#.#", "#.#", "#.#", "###"]
eco_px = []
for gi, g in enumerate((E, C, O)):
    for ry, row in enumerate(g):
        for rx, ch in enumerate(row):
            if ch == '#': eco_px.append((4 + gi * 4 + rx, 3 + ry))
compose('eco', eco_px)
more_px = [(x + dx, y) for x in (4, 8, 12) for dx in (0, 1) for y in (5, 6)]
compose('more', more_px)

# ---- folha de conferencia ----
names = ['fanbtn_19x20','fanbtn_hover_19x20','swingnt_19x20','swingnt_hover_19x20','lednt_19x11','lednt_hover_19x11','eco_19x11','eco_hover_19x11','more_19x11','more_hover_19x11','on_19x20','off_19x20','cold_19x20','fan_19x20']
S = 10; sheet = Image.new('RGB', (len(names) * (19 * S + 16) + 16, 20 * S + 32), (255, 138, 40)); x = 16
for n in names:
    im = Image.open(f'{OUT}/{n}.png').convert('L'); big = Image.new('RGB', im.size, (255, 138, 40))
    for yy in range(im.height):
        for xx in range(im.width):
            if im.getpixel((xx, yy)) < 128: big.putpixel((xx, yy), (0, 0, 0))
    sheet.paste(big.resize((im.width * S, im.height * S), Image.NEAREST), (x, 16)); x += 19 * S + 16
sheet.save('/tmp/assets_check.png'); print('ok', len(os.listdir(OUT)), 'icones')
