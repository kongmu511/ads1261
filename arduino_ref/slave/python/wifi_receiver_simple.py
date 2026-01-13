#!/usr/bin/env python3
"""
Simple UDP receiver for ZPlate - No WiFi management needed.
Use this if your PC doesn't have WiFi or you want to test over Ethernet/USB.
"""

import sys
import struct
import socket
from collections import deque
from PyQt5 import QtWidgets, QtCore
import pyqtgraph as pg


class MainWindow(QtWidgets.QMainWindow):
    # Calibration factor for DYX-301 100kg load cell (from test_tare.ino)
    CALIB_FACTOR = 2231.19  # ADC counts per Newton
    
    def __init__(self):
        super().__init__()
        self.setWindowTitle("ZPlate Simple Receiver")
        pg.setConfigOptions(antialias=True)
        
        # ESP32 connection (modify if needed)
        self.esp32_ip = "192.168.4.1"
        self.udp_port = 5555
        
        # Data buffers
        self.plot_buffers = [deque(maxlen=1000) for _ in range(4)]
        self.time_buffer = deque(maxlen=1000)
        self.sample_counter = 0
        self.streaming = False
        
        self.setup_ui()
        self.setup_udp()
        
        # Timers
        self.plot_timer = QtCore.QTimer(self)
        self.plot_timer.timeout.connect(self.update_plot)
        self.plot_timer.start(40)  # 25 FPS
        
        self.read_timer = QtCore.QTimer(self)
        self.read_timer.timeout.connect(self.poll_udp)
        self.read_timer.start(10)  # 100 Hz polling
    
    def setup_ui(self):
        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        layout = QtWidgets.QVBoxLayout(central)
        
        # Connection info
        info_layout = QtWidgets.QHBoxLayout()
        info_layout.addWidget(QtWidgets.QLabel(f"ESP32 IP: {self.esp32_ip}"))
        info_layout.addWidget(QtWidgets.QLabel(f"UDP Port: {self.udp_port}"))
        layout.addLayout(info_layout)
        
        # Plot widget
        self.plot_widget = pg.PlotWidget(background="#111111")
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)
        self.plot_widget.setLabel("left", "Force (N)")
        self.plot_widget.setLabel("bottom", "Samples")
        self.plot_widget.addLegend()
        layout.addWidget(self.plot_widget)
        
        # Create curves
        self.curves = []
        colors = ["#ff5555", "#55ff55", "#5555ff", "#ff55ff"]
        for idx, color in enumerate(colors, start=1):
            pen = pg.mkPen(color=color, width=2)
            curve = self.plot_widget.plot(pen=pen, name=f"CH{idx}")
            self.curves.append(curve)
        
        # Controls
        controls = QtWidgets.QHBoxLayout()
        self.start_button = QtWidgets.QPushButton("Start Streaming")
        self.stop_button = QtWidgets.QPushButton("Stop Streaming")
        self.tare_button = QtWidgets.QPushButton("Tare")
        self.clear_button = QtWidgets.QPushButton("Clear Log")
        
        self.start_button.clicked.connect(self.start_streaming)
        self.stop_button.clicked.connect(self.stop_streaming)
        self.tare_button.clicked.connect(self.send_tare)
        self.clear_button.clicked.connect(lambda: self.log.clear())
        
        controls.addWidget(self.start_button)
        controls.addWidget(self.stop_button)
        controls.addWidget(self.tare_button)
        controls.addWidget(self.clear_button)
        layout.addLayout(controls)
        
        # Status
        self.status_label = QtWidgets.QLabel("Streaming: idle | 0 samples | 0 Hz")
        layout.addWidget(self.status_label)
        
        # Log
        log_label = QtWidgets.QLabel("Activity Log:")
        layout.addWidget(log_label)
        self.log = QtWidgets.QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumHeight(150)
        self.log.setMaximumBlockCount(100)
        layout.addWidget(self.log)
        
        self.log_message(f"Listening on UDP port {self.udp_port}")
        self.log_message(f"Sending commands to {self.esp32_ip}:{self.udp_port}")
    
    def setup_udp(self):
        # Listener socket - bind to 192.168.4.100 specifically
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("192.168.4.100", self.udp_port))
        self.listener.setblocking(False)
        self.log_message("Bound to 192.168.4.100:5555 for receiving data")
        
        # Command socket
        self.command_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.destination = (self.esp32_ip, self.udp_port)
        
        # Rate calculation
        self.last_rate_check = QtCore.QTime.currentTime()
        self.samples_since_check = 0
    
    def send_command(self, char):
        try:
            self.command_socket.sendto(char.encode("ascii"), self.destination)
            self.log_message(f"Sent command: '{char}'")
        except OSError as exc:
            self.log_message(f"Command send failed: {exc}")
    
    def start_streaming(self):
        if not self.streaming:
            self.send_command("S")
            self.streaming = True
    
    def stop_streaming(self):
        if self.streaming:
            self.send_command("E")
            self.streaming = False
    
    def send_tare(self):
        self.send_command("T")
    
    def poll_udp(self):
        try:
            while True:
                data, addr = self.listener.recvfrom(64)
                if len(data) < 20:
                    continue
                
                # Unpack: 1 uint32 timestamp + 4 int32 channels = 20 bytes
                timestamp, ch1, ch2, ch3, ch4 = struct.unpack("<Iiiii", data[:20])
                
                # Convert raw ADC to Newtons
                ch1_N = ch1 / self.CALIB_FACTOR
                ch2_N = ch2 / self.CALIB_FACTOR
                ch3_N = ch3 / self.CALIB_FACTOR
                ch4_N = ch4 / self.CALIB_FACTOR
                
                self.sample_counter += 1
                self.samples_since_check += 1
                self.time_buffer.append(timestamp)
                self.plot_buffers[0].append(ch1_N)
                self.plot_buffers[1].append(ch2_N)
                self.plot_buffers[2].append(ch3_N)
                self.plot_buffers[3].append(ch4_N)
                
                # Calculate rate every second
                elapsed = self.last_rate_check.msecsTo(QtCore.QTime.currentTime())
                if elapsed >= 1000:
                    rate = (self.samples_since_check * 1000) / elapsed
                    self.status_label.setText(
                        f"Streaming: {'active' if self.streaming else 'waiting'} | "
                        f"{self.sample_counter} samples | {rate:.1f} Hz"
                    )
                    self.last_rate_check = QtCore.QTime.currentTime()
                    self.samples_since_check = 0
        except BlockingIOError:
            pass
        except struct.error as exc:
            self.log_message(f"Packet unpack error: {exc}")
        except OSError as exc:
            self.log_message(f"UDP error: {exc}")
    
    def update_plot(self):
        if not self.plot_buffers[0]:
            return
        try:
            x = list(range(len(self.plot_buffers[0])))
            for curve, buffer in zip(self.curves, self.plot_buffers):
                curve.setData(x, list(buffer))
        except Exception:
            # Ignore plot errors to prevent GUI freeze
            pass
    
    def log_message(self, message):
        timestamp = QtCore.QTime.currentTime().toString("HH:mm:ss")
        self.log.appendPlainText(f"[{timestamp}] {message}")
    
    def closeEvent(self, event):
        self.listener.close()
        self.command_socket.close()
        super().closeEvent(event)


def main():
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow()
    window.resize(1000, 700)
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
