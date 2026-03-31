'''
                Gear Shift Test
    This code simulates a simple 6-speed gearbox 
    with a rev limiter and shift lights.
    
@author: Allen Paul
@date: 03/30/2026
@title: gear_shift_test.py

'''

import matplotlib.pyplot as plt
import keyboard
import time
import numpy as np

# --- 1. Mock Engine ---
# 6-Speed Gearbox
gear_ratios = [0, 3.4, 2.4, 1.9, 1.5, 1.25, 1.05] 
rev_limit = 12500
idle_rpm = 2500

# PHYSICS TUNING
drag_coefficient = 0.00006  
braking_force = 2.5
accel_power = 12.0          

# --- 2. INITIAL STATE ---
current_gear = 1
velocity = 0.0
rpm = idle_rpm

# --- 3. DISPLAY SETUP ---
plt.style.use('dark_background')
fig, ax = plt.subplots(figsize=(8, 5))
plt.ion()

print("W: Gas | B: Brake | U: Up | Space: Down | ESC: Quit")

try:
    while True:
        # --- A. INPUT ("Driver Controls") HANDLING ---
        accel_active = 1.0 if keyboard.is_pressed('w') else 0.0
        brake_active = braking_force if keyboard.is_pressed('b') else 0.0
        
        if keyboard.is_pressed('u'):
            if current_gear < 6:
                current_gear += 1
                time.sleep(0.18) 
        if keyboard.is_pressed('space'):
            if current_gear > 1:
                current_gear -= 1
                time.sleep(0.18)

        # --- B. Math for the engine ---
        # 1. RPM Calculation
        rpm = idle_rpm + (velocity * gear_ratios[current_gear] * 36.5)

        # 2. Hard Rev Limiter
        power_cut = 0.0 if rpm >= (rev_limit - 50) else 1.0
        
        # 3. Engine Force with Turbo Curve
        turbo_boost = (rpm / rev_limit) ** 1.1
        # Modified gear division to make 5th and 6th gear much more efficient
        engine_push = (accel_active * accel_power * turbo_boost * power_cut) / (gear_ratios[current_gear] ** 0.4)
        
        # 4. Aero Resistance (Drag)
        resistance = (velocity ** 2) * drag_coefficient
        
        # 5. Velocity Update
        velocity = max(0, velocity + (engine_push - resistance - brake_active))
        
        # 6. Display Speed
        kmh = int(velocity)

        # Rev Limiter Bounce
        if rpm >= rev_limit:
            rpm = rev_limit - np.random.randint(300, 600)

        # --- C. Driver Dashboard ---
        ax.clear()

        # Gear and Speed Labels
        ax.text(0.5, 0.45, str(current_gear), fontsize=110, ha='center', va='center', color='white', fontweight='bold')
        ax.text(0.5, 0.2, "GEAR", fontsize=12, ha='center', color='gray')
        
        ax.text(0.5, -0.05, f"{kmh}", fontsize=60, ha='center', color='#FF4500', fontweight='bold')
        ax.text(0.5, -0.18, "KM/H", fontsize=15, ha='center', color='cyan')

        # Shift Lights
        num_dots = 12
        rpm_step = rev_limit / num_dots
        for i in range(num_dots):
            dot_x = 0.17 + (i * 0.06) 
            dot_rpm = (i + 1) * rpm_step
            dot_color = '#222222'
            if rpm >= dot_rpm:
                if dot_rpm < 10500:
                    dot_color = 'green'
                else:
                    dot_color = 'red' if (time.time() * 20) % 2 > 1 else '#111111'
            ax.plot(dot_x, 0.85, 'o', markersize=14, color=dot_color)

        # Telemetry
        ax.bar(0.9, accel_active, color='lime', width=0.06)
        ax.text(0.9, -0.25, "GAS", fontsize=10, ha='center', color='lime', fontweight='bold')
        
        ax.bar(0.1, min(1.0, brake_active/braking_force), color='red', width=0.06)
        ax.text(0.1, -0.25, "BRAKE", fontsize=10, ha='center', color='red', fontweight='bold')

        ax.set_xlim(0, 1)
        ax.set_ylim(-0.4, 1.0)
        ax.axis('off')
        plt.draw()
        plt.pause(0.001)

        if keyboard.is_pressed('esc'): break

except KeyboardInterrupt: pass
plt.close()