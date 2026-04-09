import os
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import serial
import time
import csv
from datetime import datetime

SERIAL_PORT = 'COM6'
BAUD_RATE = 115200 #Keep increasing to reduce current measurement delay?

try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
except serial.SerialException:
    print(f"Cannot open serial port {SERIAL_PORT}")
    exit()

filename = f"ina219_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
csv_file = open(filename, mode='w', newline='')
csv_writer = csv.writer(csv_file)
csv_writer.writerow(['Timestamp', 'BusVoltage', 'ShuntVoltage', 'LoadVoltage', 'Current_mA', 'Temperature', 'Power_mW'])

window_size = 100
time_data = deque([0]*window_size, maxlen=window_size)
bus_voltage_data = deque([0]*window_size, maxlen=window_size)
shunt_voltage_data = deque([0]*window_size, maxlen=window_size)
load_voltage_data = deque([0]*window_size, maxlen=window_size)
current_data = deque([0]*window_size, maxlen=window_size)
temp_data = deque([0]*window_size, maxlen=window_size)
power_data = deque([0]*window_size, maxlen=window_size)

fig, axs = plt.subplots(3, 2, figsize=(10, 8))
lines = []
labels = ['BusVoltage (V)', 'ShuntVoltage (mV)', 'LoadVoltage (V)', 'Current (mA)', 'Temperature (C)', 'Power (mW)']
data_queues = [bus_voltage_data, shunt_voltage_data, load_voltage_data, current_data, temp_data, power_data]

for ax, label in zip(axs.flat, labels):
    line, = ax.plot([], [], label=label)
    ax.set_ylabel(label)
    ax.legend(loc='upper right')
    ax.grid(True)
    lines.append(line)

axs[-1, 0].set_xlabel('Time (s)')
axs[-1, 1].set_xlabel('Time (s)')

start_time = time.time()

def update(frame):
    try:
        line = ser.readline().decode('utf-8').strip()
        if not line:
            return lines

        print(line)
        parts = line.split('\t')
        values_dict = {}

        for part in parts:
            if ':' in part:
                label, val = part.split(':')
                values_dict[label.strip()] = float(val.strip())

        timestamp = time.time() - start_time #Converting timestamp value to seconds for better visualization
        time_data.append(timestamp) #append - add items to end of array
        bus_voltage_data.append(values_dict.get('BusVoltage', 0))
        shunt_voltage_data.append(values_dict.get('ShuntVoltage', 0))
        load_voltage_data.append(values_dict.get('LoadVoltage', 0))
        current_data.append(values_dict.get('Current_mA', 0))
        temp_data.append(values_dict.get('Temperature', 0))
        power_data.append(values_dict.get('Power_mW', 0))

        # Update plots
        for line, data in zip(lines, data_queues):
            line.set_data(time_data, data)

        for ax in axs.flat:
            ax.relim()
            ax.autoscale_view()

        # Log to CSV
        csv_writer.writerow([
            datetime.now().strftime('%H:%M:%S'), #Hour/Minutes/Seconds
            bus_voltage_data[-1],
            shunt_voltage_data[-1],
            load_voltage_data[-1],
            current_data[-1],
            temp_data[-1],
            power_data[-1]
        ])

        return lines

    except Exception as e:
        print(f"Error: {e}")
        return lines

try:
    ani = animation.FuncAnimation(fig, update, interval=5000, cache_frame_data=False)
    plt.tight_layout()
    plt.show()
except KeyboardInterrupt:
    print("\nLogging interrupted by user.")
finally:
    if ser.is_open:
        ser.close()
    if not csv_file.closed:
        csv_file.close()
    print(f"\n CSV saved to: {os.path.abspath(filename)}")
