"""
                Formula E Driver Dashboard  —  pygame edition
    Smooth 60 fps rendering using hardware-accelerated surfaces.
    Drop-in replacement for the matplotlib version.

    Install:   pip install pygame
    Run:       driver_dash_pygame.py

    Controls:
        W / Up Arrow    – Throttle
        S / Down Arrow  – Brake
        D               – Toggle DRS
        1               – Normal mode
        2               – Attack mode  (uses one zone token)
        3               – Regen+ mode
        4               – Fan Boost mode
        Q / Escape      – Quit

    Serial (hardware):
        In order to use with actual hardware, uncomment the SERIAL SETUP section and set your COM port.
        The dashboard expects simple ASCII commands sent over serial, for example:
            'W' to indicate throttle is pressed
            'B' to indicate brake is pressed
            'D' to toggle DRS
            'U' to activate Attack mode
            'R' to activate Regen+ mode
            'N' to return to Normal mode
            'F' to activate Fan Boost mode
            'X' to quit the application
    @author : Allen Paul
    @date   : 2026
    @file   : driver_dash_pygame.py
"""

import pygame
import pygame.gfxdraw
import math
import time
import random
import sys
import can
import threading
import queue

# ─────────────────────────────────────────────────────────────────────────────
#  CAN INPUT SETUP
# ─────────────────────────────────────────────────────────────────────────────
BRAKE_MSG_ID = 0x008
ACCELERATOR_MSG_ID = 0x080


#can_bus queue for receiving CAN messages from a separate thread
can_buffer = queue.Queue(maxsize=1000)

# Initialize CAN bus (Linux SocketCAN example, adjust for your platform and CAN interface)
bus = can.interface.Bus(bustype='socketcan', channel='can0', bitrate=500000)
# Below is for testing
#bus = can.interface.Bus(interface='virtual', channel='test', bitrate=500000)


def can_listener():
    while True:  # Added loop to keep thread alive
        try:
            msg = bus.recv(timeout=1.0) # Added timeout to allow clean exit if needed
            if msg:
#                print(f"Listener_Thread: Received CAN message: {msg}")
                can_buffer.put(msg, block=False)
        except queue.Full:
            continue 
        except Exception as e:
            print(f"CAN Error: {e}")
            break

def get_can_message():
    try:
        return can_buffer.get_nowait() # Do not block the UI thread
    except queue.Empty:
        return None

#start CAN listener thread
listener_thread = threading.Thread(target=can_listener, daemon=True)
listener_thread.start()

# ─────────────────────────────────────────────────────────────────────────────
#  SERIAL SETUP  (uncomment to use hardware)
# ─────────────────────────────────────────────────────────────────────────────
# import serial
# try:
#     port = serial.Serial('COM3', 115200, timeout=0)
# except Exception as e:
#     print(f"Serial Error: {e}")
#     port = None
port = None

# ─────────────────────────────────────────────────────────────────────────────
#  CONSTANTS
# ─────────────────────────────────────────────────────────────────────────────
FPS          = 60
MAX_POWER    = 350
MAX_REGEN    = 250
MAX_SPEED    = 290
BAT_CAPACITY = 25.0
DRAG         = 0.00005
BRAKE_FORCE  = 3.0
ACCEL_POWER  = 14.0
MOTOR_WARN, MOTOR_CRIT = 90,  120
BAT_WARN,   BAT_CRIT   = 42,   55
INV_WARN,   INV_CRIT   = 70,   90

# ─────────────────────────────────────────────────────────────────────────────
#  COLORS  (RGB)
# ─────────────────────────────────────────────────────────────────────────────
C = {
    'bg':        (5,   5,   5),
    'card':      (13,  13,  13),
    'border':    (26,  26,  26),
    'green':     (57,  255, 20),
    'orange':    (255, 107, 0),
    'red':       (255, 34,  0),
    'blue':      (0,   170, 255),
    'yellow':    (232, 255, 0),
    'white':     (255, 255, 255),
    'dim':       (51,  51,  51),
    'very_dim':  (20,  20,  20),
    'text_dim':  (250, 250,  250),
    'topbar':    (8,   8,   8),
}

MODE_COLORS = {
    'normal': ((51,  51,  51),  (255, 255, 255)),
    'attack': ((255, 107, 0),   (0,   0,   0)),
    'regen':  ((0,   170, 255), (0,   0,   0)),
    'fan':    ((232, 255, 0),   (0,   0,   0)),
}

def temp_color(val, warn, crit):
    if val > crit * 0.9: return C['red']
    if val > warn:        return C['orange']
    return C['green']

def bat_color(pct):
    if pct < 15: return C['red']
    if pct < 30: return C['orange']
    return C['green']

# ─────────────────────────────────────────────────────────────────────────────
#  VEHICLE STATE
# ─────────────────────────────────────────────────────────────────────────────
state = {
    'velocity':        0.0,
    'accel':           0.0,
    'brake':           0.0,
    'bat_kwh':         BAT_CAPACITY * 0.78,
    'motor_temp':      42.0,
    'bat_temp':        31.0,
    'inv_temp':        38.0,
    'regen_harvested': 0.0,
    'energy_used':     6.1,
    'dist_km':         0.0,
    'lap_secs':        83.471,
    'lap_num':         3,
    'gap_ahead':       1.2,
    'attack_zones':    2,
    'attack_timer':    0,
    'mode':            'normal',
    'drs_active':      False,
    'frame':           0,
    'deploy_kw':       0,
    'regen_kw':        0,
}

def set_mode(m):
    s = state
    if m == 'attack' and s['attack_zones'] > 0:
        s['attack_zones'] -= 1
        s['attack_timer']  = FPS * 3   # 3 seconds of attack mode flash
    s['mode'] = m

# ─────────────────────────────────────────────────────────────────────────────
#  PHYSICS
# ─────────────────────────────────────────────────────────────────────────────
_last_phys = time.perf_counter()

def update_physics():
    global _last_phys
    now = time.perf_counter()
    dt  = min(now - _last_phys, 0.05)
    _last_phys = now
    s = state

    # --- NEW CAN PROCESSING BLOCK ---
    BRAKE_ACCEL_ID = 0x008
    SPEED_MSG_ID   = 0x081 # Example ID for speed
    MODE_MSG_ID    = 0x082 # Example ID for modes
    while not can_buffer.empty():
        try:
            msg = can_buffer.get_nowait() # Non-blocking fetch
            
            if msg.arbitration_id == ACCELERATOR_MSG_ID:
                state['accel'] = msg.data[0] / 15.0
                state['brake'] = msg.data[1] / 255.0
            
            elif msg.arbitration_id == SPEED_MSG_ID:
                # Assuming speed is 16-bit
                state['velocity'] = int.from_bytes(msg.data[0:2], "big")
                
            elif msg.arbitration_id == MODE_MSG_ID:
                modes = {0: 'normal', 1: 'attack', 2: 'regen', 3: 'fan'}
                val = msg.data[0]
                if val in modes:
                    set_mode(modes[val])
        except queue.Empty:
            break
    # ---------------------------------

    pm = {'normal':1.0,'attack':1.0,'fan':1.15,'regen':0.75}[s['mode']]
    rm = 1.5 if s['mode'] == 'regen' else 1.0

    push     = s['accel'] * ACCEL_POWER * pm * (1 + (s['velocity'] / MAX_SPEED) * 0.3)
    drag     = s['velocity']**2 * DRAG + s['velocity'] * 0.001
    brake_f  = s['brake'] * BRAKE_FORCE * (1 + (s['velocity'] / MAX_SPEED) * 0.5)
    s['velocity'] = max(0.0, min(MAX_SPEED, s['velocity'] + (push - drag - brake_f) * dt * 60))

    deploy_kw = int(push * MAX_POWER / ACCEL_POWER * pm) if s['accel'] > 0 else 0
    regen_kw  = (int(s['brake'] * MAX_REGEN * 0.6 * rm)
                 if s['brake'] > 0
                 else (int(s['velocity'] * 0.4 * rm) if s['accel'] == 0 and s['velocity'] > 20 else 0))

    draw = deploy_kw * dt / 3600
    gain = regen_kw  * dt / 3600
    s['bat_kwh']          = max(0.0, min(BAT_CAPACITY, s['bat_kwh'] - draw + gain))
    s['energy_used']     += draw
    s['regen_harvested'] += gain
    s['dist_km']          += s['velocity'] * dt / 3600

    s['motor_temp'] += (42 + deploy_kw*0.18 + s['velocity']*0.05 - s['motor_temp']) * 0.02
    s['bat_temp']   += (31 + deploy_kw*0.04 + regen_kw*0.03      - s['bat_temp'])   * 0.01
    s['inv_temp']   += (38 + deploy_kw*0.12 - s['inv_temp'])                         * 0.02

    s['lap_secs']   += dt
    s['gap_ahead']  += (random.random() - 0.52) * 0.01
    s['gap_ahead']   = max(-2.0, min(5.0, s['gap_ahead']))
    if s['attack_timer'] > 0: s['attack_timer'] -= 1
    s['frame']      += 1
    s['deploy_kw']   = deploy_kw
    s['regen_kw']    = regen_kw
    return True

# ─────────────────────────────────────────────────────────────────────────────
#  DRAWING HELPERS
# ─────────────────────────────────────────────────────────────────────────────
def draw_rect(surf, x, y, w, h, color, radius=6, border=None, border_w=1):
    #"""Rounded rectangle, optionally with a border."""
    rect = pygame.Rect(x, y, w, h)
    pygame.draw.rect(surf, color, rect, border_radius=radius)
    if border:
        pygame.draw.rect(surf, border, rect, border_w, border_radius=radius)

def draw_hbar(surf, x, y, w, h, frac, color, bg=(20,20,20), radius=3):
    draw_rect(surf, x, y, w, h, bg, radius)
    fw = max(0, int(w * min(1.0, frac)))
    if fw > 0:
        draw_rect(surf, x, y, fw, h, color, radius)

def draw_arc(surf, cx, cy, r, frac, color, width=8, bg_color=(30,30,30)):
    """Draw a circular progress arc."""
    # Background circle
    pygame.gfxdraw.aacircle(surf, cx, cy, r, bg_color)
    for i in range(width):
        pygame.gfxdraw.aacircle(surf, cx, cy, r - i, bg_color)

    if frac <= 0.001:
        return
    start_angle = math.pi / 2          # 12 o'clock
    sweep       = frac * 2 * math.pi
    steps       = max(1, int(sweep * r))
    prev = None
    for i in range(steps + 1):
        a  = start_angle - (i / steps) * sweep
        px = int(cx + r * math.cos(a))
        py = int(cy - r * math.sin(a))
        if prev:
            for t in range(width):
                ra = start_angle - (i / steps) * sweep + 0.02
                ox = int(math.cos(ra + math.pi/2) * t)
                oy = int(-math.sin(ra + math.pi/2) * t)
                pygame.gfxdraw.pixel(surf, px + ox, py + oy, color + (220,))
        prev = (px, py)

def draw_arc_clean(surf, cx, cy, r, frac, color, width=10):
    #"""Cleaner arc using filled circles along a path."""
    if frac <= 0:
        return
    start = -math.pi / 2
    sweep = frac * 2 * math.pi
    steps = max(2, int(abs(sweep) * r / 2))
    for i in range(steps + 1):
        a  = start + (i / steps) * sweep
        px = int(cx + r * math.cos(a))
        py = int(cy + r * math.sin(a))
        pygame.gfxdraw.filled_circle(surf, px, py, width // 2, color + (255,))
        pygame.gfxdraw.aacircle(surf, px, py, width // 2, color + (255,))

def render_text(surf, font, text, x, y, color, anchor='topleft'):
    img  = font.render(str(text), True, color)
    rect = img.get_rect(**{anchor: (x, y)})
    surf.blit(img, rect)
    return rect

# ─────────────────────────────────────────────────────────────────────────────
#  MAIN
# ─────────────────────────────────────────────────────────────────────────────
def main():
    pygame.init()
    pygame.display.set_caption("VMS Formula SAE Dashboard")

    info = pygame.display.Info()
    W, H = 1280, 720
    screen = pygame.display.set_mode((W, H), pygame.RESIZABLE | pygame.DOUBLEBUF | pygame.HWSURFACE)
    clock = pygame.time.Clock()

    # Fonts
    mono = pygame.font.SysFont('Arial', 13)

    def font(size):
        return pygame.font.SysFont('Arial', size, bold=False)
    def bold_font(size):
        return pygame.font.SysFont('Arial', size, bold=True)

    # Pre-create fonts at needed sizes
    F = {s: font(s)      for s in [9, 11, 13, 15, 18, 22, 28, 36]}
    B = {s: bold_font(s) for s in [11, 13, 15, 18, 22, 28, 36, 56, 90]}

    keys_held = {'w': False, 's': False}

    running = True
    while running:
        msg = get_can_message()
        print(msg)
        # ── EVENTS ──────────────────────────────────────────────────────────
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                k = event.key
                if k in (pygame.K_w, pygame.K_UP):    keys_held['w'] = True
                if k in (pygame.K_s, pygame.K_DOWN):  keys_held['s'] = True
                if k == pygame.K_d:   state['drs_active'] = not state['drs_active']
                if k == pygame.K_1:   set_mode('normal')
                if k == pygame.K_2:   set_mode('attack')
                if k == pygame.K_3:   set_mode('regen')
                if k == pygame.K_4:   set_mode('fan')
                if k in (pygame.K_q, pygame.K_ESCAPE): running = False
            elif event.type == pygame.KEYUP:
                k = event.key
                if k in (pygame.K_w, pygame.K_UP):   keys_held['w'] = False
                if k in (pygame.K_s, pygame.K_DOWN): keys_held['s'] = False
            elif event.type == pygame.VIDEORESIZE:
                W, H = event.w, event.h

        if not update_physics():
            running = False
            break

        s = state
        bat_pct = s['bat_kwh'] / BAT_CAPACITY * 100
        kmh = int(s['velocity'])
        speed_frac = s['velocity'] / MAX_SPEED
        eff = (s['dist_km'] / s['energy_used']) if s['energy_used'] > 0 else 0
        pred_range = s['bat_kwh'] * eff
        laps_est = max(0, int(pred_range / 2.5))
        deploy_kw = s['deploy_kw']
        regen_kw = s['regen_kw']
        bc = bat_color(bat_pct)
        frame = s['frame']
        blink = (frame // 15) % 2 == 0   # ~2 Hz blink

        # Scale factor for responsive layout
        sx = W / 1280
        sy = H / 720
        sc = min(sx, sy)

        def px(x): return int(x * sx)
        def py(y): return int(y * sy)
        def ps(s_): return max(8, int(s_ * sc))

        # ── DRAW ────────────────────────────────────────────────────────────
        screen.fill(C['bg'])
        # ── TOP BAR ─────────────────────────────────────────────────────────
        pygame.draw.rect(screen, C['topbar'], (0, 0, W, py(38)))
        pygame.draw.line(screen, C['border'], (0, py(38)), (W, py(38)), 1)

        lap_m   = int(s['lap_secs'] // 60)
        lap_s_  = s['lap_secs'] % 60
        lap_str = f"{lap_m}:{lap_s_:06.3f}"

        render_text(screen, bold_font(ps(11)), f"LAP {s['lap_num']}",  px(12),  py(10), C['text_dim'])
        render_text(screen, bold_font(ps(13)), lap_str,                 px(80),  py(9),  C['yellow'])
        render_text(screen, bold_font(ps(11)), "Δ -0.182",              px(220), py(10), C['green'])
        render_text(screen, bold_font(ps(11)), "VMS FORMULA SAE",      px(W//2),py(12), C['dim'],    anchor='midtop')
        gap_str = f"{'+' if s['gap_ahead']>=0 else ''}{s['gap_ahead']:.1f}s"
        gap_col = C['green'] if s['gap_ahead'] < 0 else C['orange']
        render_text(screen, bold_font(ps(11)), f"ATTACK ZONES: {s['attack_zones']}", px(780), py(10), C['text_dim'])
        render_text(screen, bold_font(ps(11)), "P1",                    px(1060), py(10), C['white'])
        render_text(screen, bold_font(ps(11)), f"GAP {gap_str}",        px(1100), py(10), gap_col)

        # ── LEFT PANEL  x: 10..250 ──────────────────────────────────────────
        lx, lw = px(10), px(235)
        # Battery
        draw_rect(screen, lx, py(48), lw, py(105), C['card'], border=C['border'])
        render_text(screen, font(ps(9)),  "BATTERY",              lx+px(12), py(56),  C['text_dim'])
        render_text(screen, bold_font(ps(28)), f"{bat_pct:.0f}%", lx+px(12), py(72),  bc)
        draw_hbar(screen, lx+px(12), py(112), lw-px(24), py(10), bat_pct/100, bc)
        render_text(screen, font(ps(9)), f"{s['bat_kwh']:.1f} kWh remaining",
                    lx+px(12), py(128), C['text_dim'])

        # Deploy ring
        draw_rect(screen, lx, py(162), lw, py(125), C['card'], border=C['border'])
        render_text(screen, font(ps(9)), "POWER DEPLOY",          lx+px(12), py(170), C['text_dim'])
        ring_cx, ring_cy, ring_r = lx + lw//2, py(230), py(40)
        # Background ring
        for i in range(py(12)):
            pygame.gfxdraw.aacircle(screen, ring_cx, ring_cy, ring_r - i, (20,20,20))
        # Foreground arc
        draw_arc_clean(screen, ring_cx, ring_cy, ring_r - py(6),
                       deploy_kw / MAX_POWER, C['blue'], width=py(10))
        render_text(screen, bold_font(ps(18)), str(deploy_kw), ring_cx, ring_cy-py(6),
                    C['white'], anchor='center')
        render_text(screen, font(ps(9)),  "kW",                   ring_cx, ring_cy+py(12),
                    C['text_dim'], anchor='center')
        render_text(screen, font(ps(9)),  "MAX 350 kW",           ring_cx, py(277),
                    C['text_dim'], anchor='center')

        # Regen
        draw_rect(screen, lx, py(296), lw, py(75), C['card'], border=C['border'])
        render_text(screen, font(ps(9)),  "REGEN RECOVERY",       lx+px(12), py(304), C['text_dim'])
        arrow = " ↑" if regen_kw > 0 else ""
        render_text(screen, bold_font(ps(22)), f"{regen_kw} kW{arrow}",
                    lx+px(12), py(318), C['blue'])
        draw_hbar(screen, lx+px(12), py(348), lw-px(24), py(8), regen_kw/MAX_REGEN, C['blue'])
        render_text(screen, font(ps(9)),  f"HARVESTED: {s['regen_harvested']:.2f} kWh",
                    lx+px(12), py(360), C['text_dim'])

        # Mode buttons
        draw_rect(screen, lx, py(380), lw, py(80), C['card'], border=C['border'])
        render_text(screen, font(ps(9)), "POWER MODE",            lx+px(12), py(388), C['text_dim'])
        mode_labels = [('1:NORMAL','normal'), ('2:ATTACK','attack'),
                       ('3:REGEN+','regen'),  ('4:FAN',   'fan')]
        for i, (lbl, mname) in enumerate(mode_labels):
            bx = lx + px(12) + (i % 2) * px(110)
            by = py(402) + (i // 2) * py(28)
            active = s['mode'] == mname
            bg_c, fg_c = (MODE_COLORS[mname] if active else (C['card'], C['dim']))
            bdr = MODE_COLORS[mname][0] if active else C['border']
            draw_rect(screen, bx, by, px(100), py(22), bg_c, radius=4, border=bdr)
            render_text(screen, bold_font(ps(10)), lbl, bx+px(50), by+py(11), fg_c, anchor='center')

        # ── CENTER PANEL  x: 260..1020 ──────────────────────────────────────
        cx_start = px(260)
        # Shift lights
        for i in range(15):
            dot_x  = cx_start + px(20) + i * px(48)
            dot_y  = py(58)
            filled = i < int(speed_frac * 15)
            if filled:
                if i < 5:   dc = C['green']
                elif i < 10: dc = C['orange']
                else:        dc = C['red'] if blink else C['very_dim']
            else:
                dc = (18, 18, 18)
            pygame.gfxdraw.filled_circle(screen, dot_x, dot_y, py(10), dc)
            pygame.gfxdraw.aacircle(screen,     dot_x, dot_y, py(10), dc)

        # Big Speedometer
        render_text(screen, bold_font(ps(90)), str(kmh),
                    cx_start + px(380), py(200), C['white'], anchor='center')
        render_text(screen, font(ps(13)), "km/h",
                    cx_start + px(380), py(310), C['dim'], anchor='center')

        # Pedals
        ped_y = py(380)
        draw_rect(screen, cx_start,        ped_y, px(360), py(100), C['card'], border=C['border'])
        draw_rect(screen, cx_start+px(370),ped_y, px(360), py(100), C['card'], border=C['border'])

        render_text(screen, font(ps(9)),      "ACCELERATOR",    cx_start+px(12),        ped_y+py(10), C['text_dim'])
        render_text(screen, bold_font(ps(22)),f"{int(s['accel']*100)}%",cx_start+px(12),ped_y+py(24), C['green'])
        draw_hbar(screen, cx_start+px(12), ped_y+py(60), px(336), py(10), s['accel'], C['green'])

        render_text(screen, font(ps(9)),      "BRAKE PRESSURE", cx_start+px(382),       ped_y+py(10), C['text_dim'])
        render_text(screen, bold_font(ps(22)),f"{int(s['brake']*100)}%",cx_start+px(382),ped_y+py(24), C['red'])
        draw_hbar(screen, cx_start+px(382), ped_y+py(60), px(336), py(10), s['brake'], C['red'])

        # ── RIGHT PANEL  x: 1030..1270 ──────────────────────────────────────
        rx = px(1030)
        rw = px(240)
        # Motor output
        draw_rect(screen, rx, py(48), rw, py(105), C['card'], border=C['border'])
        render_text(screen, font(ps(9)),      "MOTOR OUTPUT",  rx+px(12), py(56),  C['text_dim'])
        render_text(screen, bold_font(ps(36)),str(deploy_kw),  rx+px(12), py(70),  C['yellow'])
        render_text(screen, font(ps(9)),      "kW",            rx+px(12), py(122), C['text_dim'])

        # Temperatures
        temp_rows = [
            ("MOTOR TEMP",   s['motor_temp'], MOTOR_WARN, MOTOR_CRIT, "120°C", py(162)),
            ("BATTERY TEMP", s['bat_temp'],   BAT_WARN,   BAT_CRIT,   "55°C",  py(237))
        ]
        for (lbl, val, warn, crit, limit_str, ty) in temp_rows:
            tc = temp_color(val, warn, crit)
            draw_rect(screen, rx, ty, rw, py(68), C['card'], border=C['border'])
            render_text(screen, font(ps(9)),      lbl,              rx+px(12), ty+py(8),  C['text_dim'])
            render_text(screen, bold_font(ps(18)),f"{val:.0f}°C",   rx+px(12), ty+py(22), tc)
            draw_hbar(screen, rx+px(12), ty+py(50), rw-px(24), py(7), val/crit, tc)
            render_text(screen, font(ps(9)),      f"LIMIT {limit_str}", rx+px(12), ty+py(57), C['text_dim'])

        # ERS status
        draw_rect(screen, rx, py(390), rw, py(90), C['card'], border=C['border'])
        render_text(screen, font(ps(9)), "ERS STATUS",       rx+px(12), py(398), C['text_dim'])
        ers_dots = [
            (C['blue']   if regen_kw  > 0 else (30,30,30), "HARVEST"),
            (C['yellow'] if deploy_kw > 0 else (30,30,30), "DEPLOY"),
            (C['green']  if s['drs_active'] else (30,30,30), "DRS"),
        ]
        for i, (dc, dlbl) in enumerate(ers_dots):
            dx = rx + px(18) + i * px(74)
            pygame.gfxdraw.filled_circle(screen, dx, py(424), py(6), dc)
            pygame.gfxdraw.aacircle(screen,     dx, py(424), py(6), dc)
            render_text(screen, font(ps(8)), dlbl, dx+px(12), py(419), C['text_dim'])
        ers_state = ('REGEN' if regen_kw > 0 else 'DEPLOY' if deploy_kw > 0 else 'IDLE')
        ers_col   = (C['blue'] if regen_kw > 0 else C['yellow'] if deploy_kw > 0 else C['dim'])
        render_text(screen, font(ps(9)), f"STATE: {ers_state}", rx+px(12), py(444), ers_col)

        # ── BOTTOM BAR ───────────────────────────────────────────────────────
        bar_y = py(496)
        bar_h = H - bar_y - py(8)
        pygame.draw.rect(screen, C['topbar'], (0, bar_y - py(4), W, H))
        pygame.draw.line(screen, C['border'], (0, bar_y - py(4)), (W, bar_y - py(4)), 1)

        bstats = [
            ("ENERGY USED",     f"{s['energy_used']:.1f} kWh", C['white']),
            ("EFFICIENCY",      f"{eff:.1f} km/kWh",            C['green']),
            ("LAPS REMAIN",     str(laps_est),                  C['yellow']),
        ]
        bw = (W - px(20)) // 5
        for i, (lbl_t, val_t, col) in enumerate(bstats):
            bx2 = px(10) + i * bw
            draw_rect(screen, bx2, bar_y, bw - px(6), bar_h, C['card'], border=C['border'])
            render_text(screen, font(ps(9)),      lbl_t, bx2 + bw//2 - px(3), bar_y + py(8),
                        C['text_dim'], anchor='midtop')
            render_text(screen, bold_font(ps(15)),val_t, bx2 + bw//2 - px(3), bar_y + py(30),
                        col, anchor='midtop')

        # Controls (Test)
        render_text(screen, font(ps(8)),
                    "W/↑=THROTTLE  S/↓=BRAKE  D=DRS  1-4=MODE  Q=QUIT",
                    W - px(10), H - py(6), (30,30,30), anchor='bottomright')

        # ── FPS ──────────────────────────────────────────────────────────────
        fps_val = clock.get_fps()
        render_text(screen, font(ps(9)), f"{fps_val:.0f} fps",
                    px(10), H - py(6), (30,30,30), anchor='bottomleft')

        pygame.display.flip()
        clock.tick(FPS)

    pygame.quit()
    if port:
        port.close()
    sys.exit()


if __name__ == '__main__':
    main()
