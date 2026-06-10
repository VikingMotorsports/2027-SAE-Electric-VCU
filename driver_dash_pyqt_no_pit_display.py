#---------------------------------------------------------------------------------------#
#                           VMS Formula SAE Dashboard                                   #
#                                                                                       #
#               This is the display module that is finalized using PyQt6                #
#                                                                                       #
#  Author:     Allen Paul (2026 VMS VCU Capstone Team 13)                               #
#  Version:     1.0 (May 2026)                                                          #
#  Description: A high-performance, visually rich dashboard for the Formula SAE         #
#                                                                                       #
#---------------------------------------------------------------------------------------#
import sys
import random
import time
import math
import can
import threading
import queue
import json

try:
    from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                                 QHBoxLayout, QLabel, QFrame)
    from PyQt6.QtCore import Qt, QTimer, QRect, QPoint
    from PyQt6.QtGui import QPainter, QColor, QFont, QPen, QBrush, QLinearGradient
except ImportError:
    try:
        from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                                       QHBoxLayout, QLabel, QFrame)
        from PySide6.QtCore import Qt, QTimer, QRect, QPoint
        from PySide6.QtGui import QPainter, QColor, QFont, QPen, QBrush, QLinearGradient
    except ImportError:
        raise ImportError('Requires PyQt6 or PySide6')

    
# ─────────────────────────────────────────────────────────────────────────────
#  CAN INPUT SETUP
# ─────────────────────────────────────────────────────────────────────────────
BRAKE_MSG_ID = 0x040
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
#  SERIAL SETUP
# ─────────────────────────────────────────────────────────────────────────────
# import serial
# try:
#     port = serial.Serial('COM3', 115200, timeout=0)
# except Exception as e:
#     print(f"Serial Error: {e}")
#     port = None
port = None

# ─────────────────────────────────────────────────────────────────────────────
#  CONSTANTS & CONFIGURATION
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

C = {
    'bg':        QColor(5,   5,   5),
    'card':      QColor(13,  13,  13),
    'border':    QColor(40,  40,  40),
    'green':     QColor(57,  255, 20),
    'orange':    QColor(255, 107, 0),
    'red':       QColor(255, 34,  0),
    'blue':      QColor(0,   170, 255),
    'yellow':    QColor(232, 255, 0),
    'white':     QColor(255, 255, 255),
    'dim':       QColor(51,  51,  51),
    'very_dim':  QColor(20,  20,  20),
    'text_dim':  QColor(150, 150, 150),
    'topbar':    QColor(8,   8,   8),
}

# ─────────────────────────────────────────────────────────────────────────────
#  CUSTOM TELEMETRY WIDGET CLASSES
# ─────────────────────────────────────────────────────────────────────────────

class DashCard(QFrame):
    """The standard styling frame for metric modules."""
    def __init__(self):
        super().__init__()
        self.setStyleSheet("""
            DashCard {
                background-color: rgb(13, 13, 13);
                border: 1px solid rgb(40, 40, 40);
                border-radius: 8px;
            }
            QLabel { border: none; color: white; }
        """)

class ProgressBar(QWidget):
    """Custom horizontal telemetry bar for status tracking."""
    def __init__(self, color):
        super().__init__()
        self.color = color
        self.frac = 0.0

    def setFrac(self, frac):
        self.frac = max(0.0, min(1.0, frac))
        self.update()

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setBrush(QColor(30, 30, 30))
        p.setPen(Qt.PenStyle.NoPen)
        p.drawRoundedRect(self.rect(), 3, 3)
        p.setBrush(self.color)
        p.drawRoundedRect(0, 0, int(self.width() * self.frac), self.height(), 3, 3)

class CircularGauge(QWidget):
    """High-performance energy ring that toggles dynamically between Deploy & Regen."""
    def __init__(self):
        super().__init__()
        self.deploy_val = 0
        self.regen_val = 0

    def setValues(self, deploy, regen):
        self.deploy_val = deploy
        self.regen_val = regen
        self.update()

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        rect = self.rect().adjusted(14, 14, -14, -14)
        size = min(rect.width(), rect.height())
        center_rect = QRect(int(rect.center().x() - size//2), int(rect.center().y() - size//2), size, size)

        # Background Track
        painter.setPen(QPen(QColor(25, 25, 25), 12))
        painter.drawEllipse(center_rect)

        p_pen = QPen()
        p_pen.setWidth(12)
        p_pen.setCapStyle(Qt.PenCapStyle.RoundCap)

        if self.regen_val > 0:
            p_pen.setColor(C['yellow'])
            span = int((self.regen_val / MAX_REGEN) * 360 * 16)
            display_num = self.regen_val
            label_txt = "REGEN kW"
        else:
            p_pen.setColor(C['blue'])
            span = int((self.deploy_val / MAX_POWER) * 360 * 16)
            display_num = self.deploy_val
            label_txt = "DEPLOY kW"

        painter.setPen(p_pen)
        painter.drawArc(center_rect, 90 * 16, -span)

        # Draw Inner Core Text Metrics
        painter.setPen(C['white'])
        painter.setFont(QFont("Courier New", 24, QFont.Weight.Bold))
        painter.drawText(center_rect.adjusted(0, -10, 0, -10), Qt.AlignmentFlag.AlignCenter, f"{int(display_num)}")
        
        painter.setFont(QFont("Courier New", 10, QFont.Weight.Bold))
        painter.setPen(C['text_dim'])
        painter.drawText(center_rect.adjusted(0, 25, 0, 25), Qt.AlignmentFlag.AlignCenter, label_txt)

class PedalsIndicator(QWidget):
    """Elongated travel meters placed in the cockpit panel area."""
    def __init__(self, color, label="ACCEL"):
        super().__init__()
        self.level = 0.0
        self.color = color
        self.label = label

    def setLevel(self, val):
        self.level = max(0.0, min(1.0, val))
        self.update()

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        # Save 30px at the bottom of widget for text labels
        groove_rect = QRect(0, 0, self.width(), self.height() - 30)
        p.setBrush(QColor(20, 20, 20))
        p.setPen(QPen(C['border'], 1))
        p.drawRoundedRect(groove_rect, 8, 8)
        
        if self.level > 0:
            fill_h = int(groove_rect.height() * self.level)
            grad = QLinearGradient(0, groove_rect.height(), 0, 0)
            grad.setColorAt(0, self.color.darker(250))
            grad.setColorAt(1, self.color)
            
            p.setBrush(grad)
            p.setPen(Qt.PenStyle.NoPen)
            p.drawRoundedRect(0, groove_rect.height() - fill_h, self.width(), fill_h, 8, 8)

        p.setPen(C['text_dim'])
        p.setFont(QFont("Courier New", 10, QFont.Weight.Bold))
        p.drawText(QRect(0, self.height() - 25, self.width(), 25), Qt.AlignmentFlag.AlignCenter, self.label)

class DRSIndicator(QLabel):
    """High-visibility cockpit indicator showing the status of the DRS system."""
    def __init__(self):
        super().__init__("DRS")
        self.setFixedSize(90, 32)
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setFont(QFont("Courier New", 13, QFont.Weight.Bold))
        self.update_status(False)

    def update_status(self, active):
        if active:
            self.setStyleSheet(f"""
                background-color: {C['blue'].name()};
                color: black;
                border-radius: 4px;
                border: 2px solid white;
            """)
        else:
            self.setStyleSheet("""
                background-color: rgb(20, 20, 20);
                color: rgb(70, 70, 70);
                border-radius: 4px;
                border: 1px solid rgb(40, 40, 40);
            """)

# ─────────────────────────────────────────────────────────────────────────────
#  MAIN WINDOW MANAGEMENT
# ─────────────────────────────────────────────────────────────────────────────

class FormulaDashboard(QMainWindow):
    def __init__(self):
        super().__init__()
        # Import original data structure variables completely
        self.state = {
            'velocity': 0.0, 'accel': 0.0, 'brake': 0.0, 'bat_kwh': BAT_CAPACITY * 0.78,
            'motor_temp': 42.0, 'bat_temp': 31.0, 'inv_temp': 38.0, 'regen_harvested': 0.0,
            'energy_used': 6.1, 'dist_km': 0.0, 'lap_secs': 83.471, 'lap_num': 3,
            'gap_ahead': 0.0, 'attack_zones': 2, 'attack_timer': 0, 'mode': 'normal',
            'drs_active': False, 'frame': 0, 'deploy_kw': 0, 'regen_kw': 0,
        }
        self.keys_held = {'w': False, 's': False}
        self.initUI()
        
        # Core rendering loop running at 60 FPS
        self.timer = QTimer()
        self.timer.timeout.connect(self.app_loop)
        self.timer.start(16)
        self._last_phys = time.perf_counter()

    def create_temp_card(self, title, color):
        """Generates space-efficient combined temp modules."""
        card = DashCard()
        layout = QVBoxLayout(card)
        layout.setContentsMargins(15, 10, 15, 10)
        
        t_label = QLabel(title)
        t_label.setStyleSheet("color: #888888; font-size: 11px; font-weight: bold;")
        layout.addWidget(t_label)

        v_label = QLabel("0°C")
        v_label.setFont(QFont("Courier New", 24, QFont.Weight.Bold))
        v_label.setStyleSheet(f"color: {color.name()};")
        layout.addWidget(v_label)

        bar = ProgressBar(color)
        bar.setFixedHeight(8)
        layout.addWidget(bar)
        return card, v_label, bar

    def initUI(self):
        self.setWindowTitle("VMS Formula SAE Dashboard (PyQt)")
        self.setStyleSheet("background-color: rgb(5, 5, 5);")
        self.setFixedSize(1280, 720)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        outer_layout = QVBoxLayout(central_widget)
        outer_layout.setContentsMargins(0, 0, 0, 0)
        outer_layout.setSpacing(0)

        # ── 1. TOP STATUS BAR ──
        self.top_bar = QFrame()
        self.top_bar.setStyleSheet("background-color: rgb(8,8,8); border-bottom: 1px solid rgb(40,40,40);")
        self.top_bar.setFixedHeight(40)
        tb_lay = QHBoxLayout(self.top_bar)
        tb_lay.setContentsMargins(15, 0, 15, 0)
        
        self.lbl_lap = QLabel("LAP 3")
        self.lbl_lap_time = QLabel("0:00.000")
        self.lbl_lap_time.setStyleSheet("color: #E8FF00; font-weight: bold;")
        self.lbl_delta = QLabel("Δ -0.182")
        self.lbl_delta.setStyleSheet("color: #39FF14;")
        lbl_center_title = QLabel("PSU VMS · FORMULA SAE")
        lbl_center_title.setStyleSheet("color: #333333; font-weight: bold;")
        self.lbl_zones = QLabel("ATTACK ZONES: 2")
        self.lbl_position = QLabel("P1")
        self.lbl_gap = QLabel("GAP -0.0s")
        self.lbl_position = QLabel("POSITION: 1")
        
        for lbl in [self.lbl_lap, self.lbl_lap_time, self.lbl_delta, lbl_center_title, self.lbl_zones, self.lbl_position, self.lbl_gap]:
            lbl.setFont(QFont("Courier New", 11, QFont.Weight.Bold))
            lbl.setStyleSheet(lbl.styleSheet() + "border: none; color: white;" if "color" not in lbl.styleSheet() else lbl.styleSheet())

        tb_lay.addWidget(self.lbl_lap)
        tb_lay.addWidget(self.lbl_lap_time)
        tb_lay.addWidget(self.lbl_delta)
        tb_lay.addStretch()
        tb_lay.addWidget(lbl_center_title)
        tb_lay.addStretch()
        tb_lay.addWidget(self.lbl_zones)
        tb_lay.addWidget(self.lbl_position)
        tb_lay.addWidget(self.lbl_gap)
        outer_layout.addWidget(self.top_bar)

        # ── 2. CORE COCKPIT PANEL (3 DATA COLUMNS) ──
        cockpit_layout = QHBoxLayout()
        cockpit_layout.setContentsMargins(20, 15, 20, 15)
        cockpit_layout.setSpacing(20)

        # Left aligned column (Battery & Power Ring)
        left_col = QVBoxLayout()
        self.bat_card = DashCard()
        bl = QVBoxLayout(self.bat_card)
        lbl_b = QLabel("BATTERY SOC")
        lbl_b.setStyleSheet("color: #888888; font-size: 11px; font-weight: bold;")
        bl.addWidget(lbl_b)
        self.bat_label = QLabel("78%")
        self.bat_label.setFont(QFont("Courier New", 30, QFont.Weight.Bold))
        bl.addWidget(self.bat_label)
        self.bat_bar = ProgressBar(C['green'])
        self.bat_bar.setFixedHeight(12)
        bl.addWidget(self.bat_bar)
        left_col.addWidget(self.bat_card, 1)

        self.power_gauge = CircularGauge()
        left_col.addWidget(self.power_gauge, 2)
        cockpit_layout.addLayout(left_col, 1)

        # Center cockpit alignment column (Speedometer, DRS, and Pedals)
        center_col = QVBoxLayout()
        center_col.addStretch(1) # Top spring handles vertical symmetry

        center_h_layout = QHBoxLayout()
        center_h_layout.setSpacing(35)

        # Left telemetry travel bar (Brakes)
        self.brake_bar = PedalsIndicator(C['red'], "BRAKE")
        self.brake_bar.setFixedSize(65, 320)
        center_h_layout.addWidget(self.brake_bar)

        # Center cluster group
        speed_group = QVBoxLayout()
        speed_group.setSpacing(5)
        
        self.drs_badge = DRSIndicator()
        speed_group.addWidget(self.drs_badge, 0, Qt.AlignmentFlag.AlignHCenter)

        self.speed_label = QLabel("0")
        self.speed_label.setFont(QFont("Courier New", 145, QFont.Weight.Bold))
        self.speed_label.setStyleSheet("color: white; line-height: 100%;")
        self.speed_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        unit_label = QLabel("KM/H")
        unit_label.setFont(QFont("Courier New", 20, QFont.Weight.Bold))
        unit_label.setStyleSheet("color: #444444;")
        unit_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        speed_group.addWidget(self.speed_label)
        speed_group.addWidget(unit_label)
        center_h_layout.addLayout(speed_group)

        # Right telemetry travel bar (Throttle)
        self.throttle_bar = PedalsIndicator(C['green'], "THROTTLE")
        self.throttle_bar.setFixedSize(65, 320)
        center_h_layout.addWidget(self.throttle_bar)

        center_col.addLayout(center_h_layout)
        center_col.addStretch(1) # Bottom balancing spring
        cockpit_layout.addLayout(center_col, 2)

        # Right aligned column (Temperatures)
        right_col = QVBoxLayout()
        self.m_card, self.mt_val, self.mt_bar = self.create_temp_card("MOTOR TEMP", C['green'])
        self.b_card, self.bt_val, self.bt_bar = self.create_temp_card("BATTERY TEMP", C['orange'])
        self.i_card, self.it_val, self.it_bar = self.create_temp_card("INVERTER TEMP", C['red'])
        
        right_col.addWidget(self.m_card)
        right_col.addWidget(self.b_card)
        right_col.addWidget(self.i_card)
        cockpit_layout.addLayout(right_col, 1)

        outer_layout.addLayout(cockpit_layout, 1)

        # ── 3. BOTTOM OVERVIEW STATS ROW ──
        self.bottom_bar = QFrame()
        self.bottom_bar.setStyleSheet("background-color: rgb(8,8,8); border-top: 1px solid rgb(40,40,40);")
        self.bottom_bar.setFixedHeight(95)
        bot_layout = QHBoxLayout(self.bottom_bar)
        bot_layout.setContentsMargins(15, 10, 15, 10)
        bot_layout.setSpacing(12)

        self.stat_cards = []
        # Removed "STATE OF CHARGE" to prevent module redundancy
        labels = ["ENERGY USED", "EFFICIENCY", "PRED. RANGE", "LAPS REMAIN"]
        for label in labels:
            card = DashCard()
            cl = QVBoxLayout(card)
            cl.setContentsMargins(10, 5, 10, 5)
            cl.setSpacing(2)
            
            tl = QLabel(label)
            tl.setStyleSheet("color: #777777; font-size: 9px; font-weight: bold;")
            vl = QLabel("0")
            vl.setFont(QFont("Courier New", 16, QFont.Weight.Bold))
            
            cl.addWidget(tl, 0, Qt.AlignmentFlag.AlignHCenter)
            cl.addWidget(vl, 0, Qt.AlignmentFlag.AlignHCenter)
            bot_layout.addWidget(card)
            self.stat_cards.append(vl) # Correctly inside the labels registration loop

        outer_layout.addWidget(self.bottom_bar)

    # ─────────────────────────────────────────────────────────────────────────────
    #  VEHICLE MATH CORE SIMULATION
    # ─────────────────────────────────────────────────────────────────────────────

    def set_mode(self, m):
        if m == 'attack' and self.state['attack_zones'] > 0:
            self.state['attack_zones'] -= 1
            self.state['attack_timer'] = FPS * 3
        self.state['mode'] = m

    def keyPressEvent(self, event):
        k = event.key()
        if k in (Qt.Key.Key_W, Qt.Key.Key_Up):    self.keys_held['w'] = True
        if k in (Qt.Key.Key_S, Qt.Key.Key_Down):  self.keys_held['s'] = True
        if k == Qt.Key.Key_D: self.state['drs_active'] = not self.state['drs_active']
        if k == Qt.Key.Key_1: self.set_mode('normal')
        if k == Qt.Key.Key_2: self.set_mode('attack')
        if k == Qt.Key.Key_3: self.set_mode('regen')
        if k == Qt.Key.Key_4: self.set_mode('fan')
        if k in (Qt.Key.Key_Q, Qt.Key.Key_Escape): self.close()

    def keyReleaseEvent(self, event):
        k = event.key()
        if k in (Qt.Key.Key_W, Qt.Key.Key_Up):   self.keys_held['w'] = False
        if k in (Qt.Key.Key_S, Qt.Key.Key_Down): self.keys_held['s'] = False

    def app_loop(self):
        now = time.perf_counter()
        dt = min(now - self._last_phys, 0.05)
        self._last_phys = now
        s = self.state

        # Check Hardware Input Pipelines via Serial
        if port and port.in_waiting > 0:
            data = port.read(port.in_waiting).decode('utf-8', errors='ignore').upper()
            if 'D' in data: s['drs_active'] = not s['drs_active']
            if 'U' in data: self.set_mode('attack')
            if 'R' in data: self.set_mode('regen')
            if 'N' in data: self.set_mode('normal')
            if 'F' in data: self.set_mode('fan')
            if 'X' in data: self.close()

        #s['accel'] = 1.0 if self.keys_held.get('w') else 0.0
        #s['brake'] = 1.0 if self.keys_held.get('s') else 0.0
        while True:
            canMessage = get_can_message()

            if canMessage is None:
                break

            if canMessage.arbitration_id == ACCELERATOR_MSG_ID:
                s['accel'] = canMessage.data[0] / 100.0
                print (canMessage.data[0])

            elif canMessage.arbitration_id == BRAKE_MSG_ID:
                s['brake'] = canMessage.data[0] / 100.0
                print (canMessage.data[0])

        pm = {'normal': 1.0, 'attack': 1.0, 'fan': 1.15, 'regen': 0.75}[s['mode']]
        rm = 1.5 if s['mode'] == 'regen' else 1.0

        push = s['accel'] * ACCEL_POWER * pm * (1 + (s['velocity'] / MAX_SPEED) * 0.3)
        drag = s['velocity']**2 * DRAG + s['velocity'] * 0.001
        brake_f = s['brake'] * BRAKE_FORCE * (1 + (s['velocity'] / MAX_SPEED) * 0.5)
        s['velocity'] = max(0.0, min(MAX_SPEED, s['velocity'] + (push - drag - brake_f) * dt * 60))

        deploy_kw = int(push * MAX_POWER / ACCEL_POWER * pm) if s['accel'] > 0 else 0
        regen_kw = (int(s['brake'] * MAX_REGEN * 0.6 * rm) if s['brake'] > 0 
                    else (int(s['velocity'] * 0.4 * rm) if s['accel'] == 0 and s['velocity'] > 20 else 0))

        draw = deploy_kw * dt / 3600
        gain = regen_kw * dt / 3600
        s['bat_kwh'] = max(0.0, min(BAT_CAPACITY, s['bat_kwh'] - draw + gain))
        s['energy_used'] += draw
        s['regen_harvested'] += gain
        s['dist_km'] += s['velocity'] * dt / 3600

        s['motor_temp'] += (42 + deploy_kw * 0.18 + s['velocity'] * 0.05 - s['motor_temp']) * 0.02
        s['bat_temp'] += (31 + deploy_kw * 0.04 + regen_kw * 0.03 - s['bat_temp']) * 0.01
        s['inv_temp'] += (38 + deploy_kw * 0.12 - s['inv_temp']) * 0.02

        s['lap_secs'] += dt
        s['gap_ahead'] += (random.random() - 0.52) * 0.01
        s['gap_ahead'] = max(-2.0, min(5.0, s['gap_ahead']))
        if s['attack_timer'] > 0: s['attack_timer'] -= 1
        s['frame'] += 1
        s['deploy_kw'] = deploy_kw
        s['regen_kw'] = regen_kw

        self.update_ui_elements()

    # ─────────────────────────────────────────────────────────────────────────────
    #  UI STATE SYNC RENDER FRAME
    # ─────────────────────────────────────────────────────────────────────────────

    def update_ui_elements(self):
        s = self.state
        bat_pct = (s['bat_kwh'] / BAT_CAPACITY) * 100
        
        # 1. Update Top Bar Values
        lap_m = int(s['lap_secs'] // 60)
        lap_s_ = s['lap_secs'] % 60
        self.lbl_lap.setText(f"LAP {s['lap_num']}")
        self.lbl_lap_time.setText(f"{lap_m}:{lap_s_:06.3f}")
        self.lbl_zones.setText(f"ATTACK ZONES: {s['attack_zones']}")
        gap_str = f"{'+' if s['gap_ahead']>=0 else ''}{s['gap_ahead']:.1f}s"
        self.lbl_gap.setText(f"GAP {gap_str}")
        self.lbl_gap.setStyleSheet(f"color: {'#39FF14' if s['gap_ahead'] < 0 else '#FF6B00'}; font-weight: bold;")

        # 2. Update Center Dashboard Panel Elements
        self.speed_label.setText(str(int(s['velocity'])))
        self.throttle_bar.setLevel(s['accel'])
        self.brake_bar.setLevel(s['brake'])
        self.drs_badge.update_status(s['drs_active'])

        # 3. Update Left Battery SOC & Ring Gauge
        self.bat_label.setText(f"{int(bat_pct)}%")
        self.bat_bar.setFrac(bat_pct / 100.0)
        if bat_pct < 15:    self.bat_label.setStyleSheet("color: #FF2200;")
        elif bat_pct < 30:  self.bat_label.setStyleSheet("color: #FF6B00;")
        else:               self.bat_label.setStyleSheet("color: #39FF14;")

        self.power_gauge.setValues(s['deploy_kw'], s['regen_kw'])

        # 4. Update Right Temperature Elements
        self.mt_val.setText(f"{int(s['motor_temp'])}°C")
        self.mt_bar.setFrac(s['motor_temp'] / MOTOR_CRIT)
        
        self.bt_val.setText(f"{int(s['bat_temp'])}°C")
        self.bt_bar.setFrac(s['bat_temp'] / BAT_CRIT)
        
        self.it_val.setText(f"{int(s['inv_temp'])}°C")
        self.it_bar.setFrac(s['inv_temp'] / INV_CRIT)

        # 5. Update Bottom Overview Row Cards (Indices updated for 4 elements)
        eff = (s['dist_km'] / s['energy_used']) if s['energy_used'] > 0 else 0
        pred_range = s['bat_kwh'] * eff
        laps_est = max(0, int(pred_range / 2.5))

        self.stat_cards[0].setText(f"{s['energy_used']:.1f} kWh")
        self.stat_cards[1].setText(f"{eff:.1f} km/kWh")
        self.stat_cards[1].setStyleSheet("color: #39FF14;")
        self.stat_cards[2].setText(f"{pred_range:.0f} km")
        self.stat_cards[3].setText(str(laps_est))
        self.stat_cards[3].setStyleSheet("color: #E8FF00;")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = FormulaDashboard()
    window.show()
    sys.exit(app.exec())