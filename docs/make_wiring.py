"""Draws docs/wiring.svg: an ESP32 DevKitC (38-pin, real pin order) wired to
the 4x4 keypad, the four extra buttons and the on-board LED, plus a table of
what each pin does. Run: python docs/make_wiring.py  (no dependencies)."""
from pathlib import Path

W, H = 1500, 1120
GOLD, GOLD_L, SKY, TEAL = '#c5a059', '#f3d58c', '#7ec8e3', '#2ec4b6'
INK, MUTED, BG = '#f2f4f8', '#9aa4b5', '#0b0e14'

# ESP32 DevKitC v4, USB at the bottom; pins top → bottom.
LEFT = ['3V3', 'EN', 'VP 36', 'VN 39', '34', '35', '32', '33', '25', '26',
        '27', '14', '12', 'GND', '13', 'D2', 'D3', 'CMD', '5V']
RIGHT = ['GND', '23', '22', 'TX', 'RX', '21', 'GND', '19', '18', '5', '17',
         '16', '4', '0', '2', '15', 'D1', 'D0', 'CLK']
BX, BY, BW = 600, 150, 300         # board
PITCH = 36
PIN_Y0 = BY + 160
LX, RX = BX + 22, BX + BW - 22     # pin dots


def pin_y(i):
    return PIN_Y0 + i * PITCH


out = []
add = out.append
add(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
    'font-family="Figtree, Segoe UI, Arial, sans-serif">')
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
add(f'<text x="{W/2}" y="52" text-anchor="middle" fill="{GOLD_L}" font-size="34" font-weight="800">'
    'Karaoke Pad — wiring</text>')
add(f'<text x="{W/2}" y="86" text-anchor="middle" fill="{MUTED}" font-size="18">'
    'ESP32 DevKitC · 4×4 membrane keypad · 4 push buttons · no resistors (internal pull-ups)</text>')

# ——— Board ———
bh = PIN_Y0 + 18 * PITCH + 70 - BY
add(f'<rect x="{BX}" y="{BY}" width="{BW}" height="{bh}" rx="18" fill="#163041" stroke="#2a5570" stroke-width="3"/>')
add(f'<rect x="{BX+70}" y="{BY+18}" width="{BW-140}" height="120" rx="6" fill="#c8ccd4"/>')
add(f'<rect x="{BX+82}" y="{BY+30}" width="{BW-164}" height="96" rx="4" fill="#9ea4ae"/>')
add(f'<text x="{BX+BW/2}" y="{BY+72}" text-anchor="middle" fill="#2b3240" font-size="16" font-weight="800">ESP32-WROOM-32</text>')
add(f'<text x="{BX+BW/2}" y="{BY+96}" text-anchor="middle" fill="#3b4250" font-size="12">Bluetooth LE + Wi-Fi</text>')
add(f'<text x="{BX+BW/2}" y="{BY+300}" text-anchor="middle" fill="#9ec3d6" font-size="20" font-weight="800">ESP32 DevKitC</text>')
add(f'<text x="{BX+BW/2}" y="{BY+326}" text-anchor="middle" fill="#6f95a8" font-size="14">(38 pins, top view)</text>')
# EN / BOOT buttons and the on-board LED
by_btn = BY + bh - 70
for x, name in ((BX + 70, 'EN'), (BX + BW - 110, 'BOOT')):
    add(f'<rect x="{x}" y="{by_btn}" width="40" height="28" rx="5" fill="#2b3240"/>')
    add(f'<text x="{x+20}" y="{by_btn+48}" text-anchor="middle" fill="#9ec3d6" font-size="12">{name}</text>')
led_x, led_y = BX + BW / 2, PIN_Y0 + RIGHT.index('2') * PITCH  # level with GPIO 2
add(f'<circle cx="{led_x}" cy="{led_y}" r="9" fill="{SKY}"/><circle cx="{led_x}" cy="{led_y}" r="18" fill="{SKY}" opacity="0.25"/>')
add(f'<line x1="{led_x+20}" y1="{led_y}" x2="{RX-40}" y2="{led_y}" stroke="{SKY}" stroke-width="2" stroke-dasharray="4 4"/>')
add(f'<text x="{led_x}" y="{led_y+38}" text-anchor="middle" fill="{SKY}" font-size="13" font-weight="700">on-board LED · GPIO 2</text>')
# USB
add(f'<rect x="{BX+BW/2-40}" y="{BY+bh-18}" width="80" height="34" rx="6" fill="#aab0ba"/>')
add(f'<text x="{BX+BW/2}" y="{BY+bh+44}" text-anchor="middle" fill="{MUTED}" font-size="14">USB · power + flashing</text>')

used = {}  # pin label → colour


def pin_dot(x, y, label, side):
    c = used.get(label, '#55606f')
    add(f'<circle cx="{x}" cy="{y}" r="8" fill="#0d1b25" stroke="{c}" stroke-width="3"/>')
    anchor = 'start' if side == 'L' else 'end'
    tx = x + 16 if side == 'L' else x - 16
    weight = '800' if label in used else '400'
    fill = INK if label in used else '#6f8597'
    add(f'<text x="{tx}" y="{y+5}" text-anchor="{anchor}" fill="{fill}" font-size="14" font-weight="{weight}">{label}</text>')


# ——— Keypad (left) ———
KX, KY, KW, KH = 70, 330, 300, 360
keys = [['1', '2', '3', 'A'], ['4', '5', '6', 'B'], ['7', '8', '9', 'C'], ['*', '0', '#', 'D']]
names = {'A': 'Next', 'B': 'Stop', 'C': 'Melody', 'D': 'Mic', '*': 'Back', '#': 'OK'}
add(f'<rect x="{KX}" y="{KY}" width="{KW}" height="{KH}" rx="16" fill="#1d2330" stroke="#3a4254" stroke-width="2"/>')
add(f'<text x="{KX+KW/2}" y="{KY-40}" text-anchor="middle" fill="{INK}" font-size="18" font-weight="800">4×4 membrane keypad</text>')
add(f'<text x="{KX+KW/2}" y="{KY-16}" text-anchor="middle" fill="{MUTED}" font-size="13">'
    '8-pin ribbon, left → right: rows R1–R4, columns C1–C4</text>')
for r in range(4):
    for c in range(4):
        x, y = KX + 22 + c * 68, KY + 22 + r * 72
        k = keys[r][c]
        fill = 'url(#gold)' if k == '#' else '#2c3443'
        add(f'<rect x="{x}" y="{y}" width="56" height="56" rx="10" fill="{fill}"/>')
        add(f'<text x="{x+28}" y="{y+30}" text-anchor="middle" fill="{"#14110a" if k == "#" else INK}" '
            f'font-size="22" font-weight="800">{k}</text>')
        if k in names:
            add(f'<text x="{x+28}" y="{y+48}" text-anchor="middle" fill="{"#14110a" if k == "#" else MUTED}" '
                f'font-size="10">{names[k]}</text>')

# Keypad ribbon: 8 pins along the bottom, left → right R1 R2 R3 R4 C1 C2 C3 C4
ribbon = [('R1', '13'), ('R2', '12'), ('R3', '14'), ('R4', '27'),
          ('C1', '26'), ('C2', '25'), ('C3', '33'), ('C4', '32')]
for lab, gp in ribbon:
    used[gp] = GOLD if lab.startswith('R') else TEAL
rib_y = KY + KH + 38
for i, (lab, gp) in enumerate(ribbon):
    x = KX + 38 + i * 32
    col = used[gp]
    add(f'<rect x="{x-9}" y="{KY+KH}" width="18" height="34" fill="{col}" opacity="0.85"/>')
    add(f'<text x="{x}" y="{KY+KH-16}" text-anchor="middle" fill="{col}" font-size="13" font-weight="800">{lab}</text>')

# Wires keypad → left header (orthogonal, one lane each)
for i, (lab, gp) in enumerate(ribbon):
    x0 = KX + 38 + i * 32
    y0 = KY + KH + 34
    yi = pin_y(LEFT.index(gp))
    lane_y = y0 + 90 + i * 12          # under the keypad
    lane_x = BX - 40 - i * 18          # left of the board
    col = used[gp]
    add(f'<polyline points="{x0},{y0} {x0},{lane_y} {lane_x},{lane_y} {lane_x},{yi} {LX},{yi}" '
        f'fill="none" stroke="{col}" stroke-width="4" stroke-linejoin="round" opacity="0.9"/>')

# ——— Buttons (right) ———
buttons = [('Volume +', '23'), ('Volume −', '22'), ('Tempo +', '21'), ('Tempo −', '19')]
for _, gp in buttons:
    used[gp] = SKY
used['GND'] = '#8a93a3'
used['2'] = SKY
BTX = 1150
gnd_x = BTX + 150
BTN_Y = [260, 410, 540, 710]  # chosen so no two wires run together
for i, (name, gp) in enumerate(buttons):
    y = BTN_Y[i]
    yi = pin_y(RIGHT.index(gp))
    lane_x = BX + BW + 40 + i * 18
    add(f'<polyline points="{RX},{yi} {lane_x},{yi} {lane_x},{y} {BTX-34},{y}" fill="none" '
        f'stroke="{SKY}" stroke-width="4" stroke-linejoin="round"/>')
    # button symbol
    add(f'<rect x="{BTX-34}" y="{y-30}" width="68" height="60" rx="12" fill="#1d2330" stroke="#3a4254" stroke-width="2"/>')
    add(f'<circle cx="{BTX}" cy="{y}" r="18" fill="url(#gold)"/>')
    add(f'<line x1="{BTX+34}" y1="{y}" x2="{gnd_x}" y2="{y}" stroke="#8a93a3" stroke-width="4"/>')
    add(f'<text x="{BTX}" y="{y-42}" text-anchor="middle" fill="{INK}" font-size="16" font-weight="800">{name}</text>')
    add(f'<text x="{BTX}" y="{y+52}" text-anchor="middle" fill="{SKY}" font-size="13">GPIO {gp}</text>')
# GND bus: right GND pin → down the bus
gy = pin_y(RIGHT.index('GND'))
bus_y = 175
add(f'<polyline points="{RX},{gy} {BX+BW+20},{gy} {BX+BW+20},{bus_y} {gnd_x},{bus_y} {gnd_x},{BTN_Y[-1]}" '
    f'fill="none" stroke="#8a93a3" stroke-width="4" stroke-linejoin="round"/>')
add(f'<text x="{BX+BW+40}" y="{bus_y-12}" fill="#8a93a3" font-size="14" font-weight="800">GND — the other leg of every button</text>')

# Pins (drawn last, over the wires)
for i, lab in enumerate(LEFT):
    pin_dot(LX, pin_y(i), lab, 'L')
for i, lab in enumerate(RIGHT):
    pin_dot(RX, pin_y(i), lab, 'R')

# ——— What each pin is for ———
TX0, TY0 = 975, 830
rows = [
    (GOLD, 'GPIO 13 · 12 · 14 · 27', 'keypad rows R1–R4 (outputs, scanned)'),
    (TEAL, 'GPIO 26 · 25 · 33 · 32', 'keypad columns C1–C4 (inputs, pull-up)'),
    (SKY, 'GPIO 23 · 22', 'Volume + / − (hold to repeat)'),
    (SKY, 'GPIO 21 · 19', 'Tempo + / − (hold to repeat)'),
    (SKY, 'GPIO 2', 'status LED (on-board)'),
    ('#8a93a3', 'GND', 'other leg of the four buttons'),
]
add(f'<rect x="{TX0-20}" y="{TY0-40}" width="520" height="{len(rows)*36+60}" rx="14" fill="#141a24" stroke="#2b3342"/>')
add(f'<text x="{TX0}" y="{TY0-12}" fill="{GOLD_L}" font-size="16" font-weight="800" letter-spacing="1">WHAT EACH PIN IS FOR</text>')
for i, (col, pins, what) in enumerate(rows):
    y = TY0 + 22 + i * 36
    add(f'<rect x="{TX0}" y="{y-12}" width="14" height="14" rx="3" fill="{col}"/>')
    add(f'<text x="{TX0+24}" y="{y}" fill="{INK}" font-size="15" font-weight="700">{pins}</text>')
    add(f'<text x="{TX0+210}" y="{y}" fill="{MUTED}" font-size="15">{what}</text>')

add('<defs><linearGradient id="gold" x1="0" y1="0" x2="0" y2="1">'
    f'<stop offset="0" stop-color="{GOLD_L}"/><stop offset="1" stop-color="{GOLD}"/></linearGradient></defs>')
add('</svg>')
Path(__file__).with_name('wiring.svg').write_text('\n'.join(out), encoding='utf-8')
print('wrote docs/wiring.svg')
