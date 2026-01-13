#!/usr/bin/env python3
"""WiFi-aware real-time visualization for the ZPlate force plate."""
import os
import platform
import socket
import struct
import subprocess
import sys
import tempfile
from collections import deque

from PyQt5 import QtCore, QtWidgets
import pyqtgraph as pg


class TaskSignals(QtCore.QObject):
    finished = QtCore.pyqtSignal(object)
    error = QtCore.pyqtSignal(str)
    def __init__(self):
        super().__init__()


class Task(QtCore.QRunnable):
    """Wrapper to run blocking logic without freezing the GUI."""

    def __init__(self, fn, *args, **kwargs):
        super().__init__()
        self.fn = fn
        self.args = args
        self.kwargs = kwargs
        self.signals = TaskSignals()

    @QtCore.pyqtSlot()
    def run(self):
        try:
            result = self.fn(*self.args, **self.kwargs)
        except Exception as exc:  # pragma: no cover - instrumentation
            self.signals.error.emit(str(exc))
        else:
            self.signals.finished.emit(result)


class WiFiManager:
    """Wraps Windows netsh commands to scan/connect/disconnect."""

    PROFILE_TEMPLATE = """<?xml version=\"1.0\"?>
<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">
  <name>{ssid}</name>
  <SSIDConfig>
    <SSID>
      <name>{ssid}</name>
    </SSID>
  </SSIDConfig>
  <connectionType>ESS</connectionType>
  <connectionMode>manual</connectionMode>
  <MSM>
    <security>
      <authEncryption>
        <authentication>WPA2PSK</authentication>
        <encryption>AES</encryption>
        <useOneX>false</useOneX>
      </authEncryption>
      <sharedKey>
        <keyType>passPhrase</keyType>
        <protected>false</protected>
        <keyMaterial>{password}</keyMaterial>
      </sharedKey>
    </security>
  </MSM>
</WLANProfile>
"""

    def __init__(self):
        self._platform = platform.system()

    def _ensure_windows(self):
        if self._platform != "Windows":
            raise RuntimeError("Wi-Fi management requires Windows and netsh.")

    def _run_netsh(self, args):
        """Execute a netsh command and return the decoded output."""
        self._ensure_windows()
        try:
            result = subprocess.run(
                ["netsh", "wlan"] + args,
                capture_output=True,
                text=True,
                check=True,
            )
        except subprocess.CalledProcessError as exc:
            raise RuntimeError(exc.stderr.strip() or str(exc))
        return result.stdout

    def scan_networks(self):
        output = self._run_netsh(["show", "networks", "mode=bssid"])
        networks = []
        current = {}
        for line in output.splitlines():
            trimmed = line.strip()
            if trimmed.startswith("SSID ") and ":" in trimmed:
                if current:
                    networks.append(current)
                current = {"ssid": trimmed.split(":", 1)[1].strip(), "signal": "", "auth": ""}
            elif trimmed.startswith("Signal") and ":" in trimmed and current:
                current["signal"] = trimmed.split(":", 1)[1].strip()
            elif trimmed.startswith("Authentication") and ":" in trimmed and current:
                current["auth"] = trimmed.split(":", 1)[1].strip()
        if current:
            networks.append(current)
        return networks

    def current_connection(self):
        output = self._run_netsh(["show", "interfaces"])
        state = {}
        for line in output.splitlines():
            if ":" not in line:
                continue
            key, value = line.split(":", 1)
            state[key.strip()] = value.strip()
        return state

    def disconnect(self):
        self._run_netsh(["disconnect"])
        return self.current_connection()

    def connect(self, ssid, password=None):
        status = self.current_connection()
        interface = status.get("Name")

        profiles = self._run_netsh(["show", "profiles"])
        if ssid not in profiles:
            if not password:
                raise RuntimeError("Password required to create a new profile.")
            profile_xml = self.PROFILE_TEMPLATE.format(ssid=ssid, password=password)
            fd, path = tempfile.mkstemp(suffix=".xml")
            with os.fdopen(fd, "w", encoding="utf-8") as handle:
                handle.write(profile_xml)
            try:
                self._run_netsh(["add", "profile", f"filename={path}", "user=current"])
            finally:
                os.unlink(path)

        cmd = ["connect", f"name={ssid}"]
        if interface:
            cmd.append(f"interface={interface}")
        self._run_netsh(cmd)
        return self.current_connection()


def format_signal(signal_text):
    return signal_text if signal_text else "n/a"


def make_logger(widget):
    def logger(message):
        timestamp = QtCore.QDateTime.currentDateTime().toString("HH:mm:ss")
        widget.appendPlainText(f"[{timestamp}] {message}")
    return logger


class MainWindow(QtWidgets.QMainWindow):
    UDP_BUFFER_SIZE = 64

    def __init__(self):
        super().__init__()
        self.setWindowTitle("ZPlate Wi-Fi Manager + Visualization")
        pg.setConfigOptions(antialias=True)
        self.thread_pool = QtCore.QThreadPool()
        self.wifi_manager = WiFiManager()
        self.ap_ip = "192.168.4.1"
        self.udp_port = 5555
        self.plot_buffers = [deque(maxlen=512) for _ in range(4)]
        self.time_buffer = deque(maxlen=512)
        self.sample_counter = 0
        self.streaming = False
        self.setup_ui()
        self.setup_udp()
        self.plot_timer = QtCore.QTimer(self)
        self.plot_timer.timeout.connect(self.update_plot)
        self.plot_timer.start(40)
        self.read_timer = QtCore.QTimer(self)
        self.read_timer.timeout.connect(self.poll_udp)
        self.read_timer.start(10)
        self.status_timer = QtCore.QTimer(self)
        self.status_timer.timeout.connect(self.refresh_connection_status)
        self.status_timer.start(5000)

    def setup_ui(self):
        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        layout = QtWidgets.QVBoxLayout(central)
        layout.addWidget(splitter)

        wifi_panel = QtWidgets.QGroupBox("Wi-Fi Management")
        wifi_layout = QtWidgets.QVBoxLayout()
        wifi_panel.setLayout(wifi_layout)
        splitter.addWidget(wifi_panel)

        control_layout = QtWidgets.QHBoxLayout()
        self.scan_button = QtWidgets.QPushButton("Scan networks")
        self.scan_button.clicked.connect(self.scan_networks)
        self.refresh_status_button = QtWidgets.QPushButton("Refresh status")
        self.refresh_status_button.clicked.connect(self.refresh_connection_status)
        control_layout.addWidget(self.scan_button)
        control_layout.addWidget(self.refresh_status_button)
        wifi_layout.addLayout(control_layout)

        self.network_table = QtWidgets.QTableWidget(0, 3)
        self.network_table.setHorizontalHeaderLabels(["SSID", "Signal", "Authentication"])
        self.network_table.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self.network_table.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self.network_table.verticalHeader().setVisible(False)
        self.network_table.horizontalHeader().setStretchLastSection(True)
        self.network_table.itemSelectionChanged.connect(self.on_network_selected)
        wifi_layout.addWidget(self.network_table)

        form = QtWidgets.QFormLayout()
        self.ssid_input = QtWidgets.QLineEdit("ZPlate")
        self.password_input = QtWidgets.QLineEdit("zplate2026")
        self.password_input.setEchoMode(QtWidgets.QLineEdit.Password)
        form.addRow("SSID", self.ssid_input)
        form.addRow("Password", self.password_input)
        wifi_layout.addLayout(form)

        actions_layout = QtWidgets.QHBoxLayout()
        self.connect_button = QtWidgets.QPushButton("Connect")
        self.connect_button.clicked.connect(self.connect_to_ssid)
        self.disconnect_button = QtWidgets.QPushButton("Disconnect")
        self.disconnect_button.clicked.connect(self.disconnect)
        actions_layout.addWidget(self.connect_button)
        actions_layout.addWidget(self.disconnect_button)
        wifi_layout.addLayout(actions_layout)

        self.connection_label = QtWidgets.QLabel("Current network: Unknown")
        wifi_layout.addWidget(self.connection_label)

        self.devices_label = QtWidgets.QLabel("Devices connected: unknown")
        wifi_layout.addWidget(self.devices_label)

        log_header = QtWidgets.QHBoxLayout()
        log_header.addWidget(QtWidgets.QLabel("Activity Log:"))
        log_header.addStretch()
        self.clear_log_button = QtWidgets.QPushButton("Clear")
        self.clear_log_button.clicked.connect(lambda: self.log.clear())
        log_header.addWidget(self.clear_log_button)
        wifi_layout.addLayout(log_header)
        
        self.log = QtWidgets.QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(500)
        wifi_layout.addWidget(self.log)
        self.logger = make_logger(self.log)

        stream_panel = QtWidgets.QGroupBox("Streaming & Visualization")
        stream_layout = QtWidgets.QVBoxLayout()
        stream_panel.setLayout(stream_layout)
        splitter.addWidget(stream_panel)

        plot_layout = QtWidgets.QVBoxLayout()
        self.plot_widget = pg.PlotWidget(background="#111111")
        self.plot_widget.showGrid(x=True, y=True, alpha=0.3)
        self.plot_widget.setLabel("left", "ADC Value")
        self.plot_widget.setLabel("bottom", "Samples")
        self.plot_widget.addLegend()
        stream_layout.addWidget(self.plot_widget)

        self.curves = []
        colors = ["#ff5555", "#55ff55", "#5555ff", "#ff55ff"]
        for idx, color in enumerate(colors, start=1):
            pen = pg.mkPen(color=color, width=2)
            curve = self.plot_widget.plot(pen=pen, name=f"CH{idx}")
            self.curves.append(curve)

        controls = QtWidgets.QHBoxLayout()
        self.start_button = QtWidgets.QPushButton("Start streaming")
        self.stop_button = QtWidgets.QPushButton("Stop streaming")
        self.start_button.clicked.connect(self.start_streaming)
        self.stop_button.clicked.connect(self.stop_streaming)
        controls.addWidget(self.start_button)
        controls.addWidget(self.stop_button)
        stream_layout.addLayout(controls)

        self.status_label = QtWidgets.QLabel("Streaming: idle | 0 samples")
        stream_layout.addWidget(self.status_label)

    def setup_udp(self):
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("", self.udp_port))
        self.listener.setblocking(False)
        self.command_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.destination = (self.ap_ip, self.udp_port)
        self.logger(f"Listening on UDP port {self.udp_port}")

    def send_command(self, char):
        try:
            self.command_socket.sendto(char.encode("ascii"), self.destination)
        except OSError as exc:
            self.logger(f"Command send failed: {exc}")

    def start_streaming(self):
        if not self.streaming:
            self.send_command("S")
            self.streaming = True
            self.logger("Sent START command to ESP32")

    def stop_streaming(self):
        if self.streaming:
            self.send_command("E")
            self.streaming = False
            self.logger("Sent STOP command to ESP32")

    def poll_udp(self):
        try:
            while True:
                data, addr = self.listener.recvfrom(self.UDP_BUFFER_SIZE)
                if len(data) < 20:
                    continue
                # Unpack: uint32 timestamp + 4x int32 channels = 20 bytes
                timestamp, ch1, ch2, ch3, ch4 = struct.unpack("<Iiii", data[:20])
                self.sample_counter += 1
                self.time_buffer.append(timestamp)
                self.plot_buffers[0].append(ch1)
                self.plot_buffers[1].append(ch2)
                self.plot_buffers[2].append(ch3)
                self.plot_buffers[3].append(ch4)
                self.status_label.setText(f"Streaming: {'active' if self.streaming else 'waiting'} | {self.sample_counter} samples")
        except BlockingIOError:
            pass
        except struct.error as exc:
            self.logger(f"Packet unpack error: {exc}")
        except OSError as exc:
            self.logger(f"UDP receive error: {exc}")

    def update_plot(self):
        if not self.plot_buffers[0]:
            return
        try:
            x = list(range(len(self.plot_buffers[0])))
            for curve, buffer in zip(self.curves, self.plot_buffers):
                curve.setData(x, list(buffer))
        except Exception as exc:
            # Ignore plot errors to prevent GUI freeze
            pass

    def scan_networks(self):
        self.scan_button.setEnabled(False)
        self.logger("Scanning for WiFi networks...")
        worker = Task(self.wifi_manager.scan_networks)
        worker.signals.finished.connect(self.on_scan_complete)
        worker.signals.error.connect(self.on_worker_error)
        self.thread_pool.start(worker)

    def on_scan_complete(self, networks):
        self.scan_button.setEnabled(True)
        self.network_table.setRowCount(0)
        for network in networks:
            row = self.network_table.rowCount()
            self.network_table.insertRow(row)
            self.network_table.setItem(row, 0, QtWidgets.QTableWidgetItem(network["ssid"]))
            self.network_table.setItem(row, 1, QtWidgets.QTableWidgetItem(format_signal(network.get("signal"))))
            self.network_table.setItem(row, 2, QtWidgets.QTableWidgetItem(network.get("auth", "")))
        self.logger(f"Network scan found {len(networks)} entries")

    def on_worker_error(self, message):
        if not self.scan_button.isEnabled():
            self.scan_button.setEnabled(True)
        self.logger(f"Wi-Fi task failed: {message}")

    def on_network_selected(self):
        items = self.network_table.selectedItems()
        if not items:
            return
        ssid = items[0].text()
        self.ssid_input.setText(ssid)
        signal = items[1].text()
        self.devices_label.setText(f"Last scan: {ssid} ({signal})")

    def connect_to_ssid(self):
        ssid = self.ssid_input.text().strip()
        password = self.password_input.text().strip()
        if not ssid:
            self.logger("SSID cannot be empty")
            return
        self.logger(f"Connecting to {ssid}...")
        worker = Task(self.wifi_manager.connect, ssid, password)
        worker.signals.finished.connect(self.on_connect_finished)
        worker.signals.error.connect(self.on_worker_error)
        self.thread_pool.start(worker)

    def on_connect_finished(self, status):
        ssid = status.get("SSID", "Unknown")
        state = status.get("State", "Unknown")
        self.connection_label.setText(f"Current network: {ssid} ({state})")
        self.logger(f"Connected to {ssid} ({state})")

    def disconnect(self):
        worker = Task(self.wifi_manager.disconnect)
        worker.signals.finished.connect(self.on_disconnect_finished)
        worker.signals.error.connect(self.on_worker_error)
        self.thread_pool.start(worker)

    def on_disconnect_finished(self, status):
        state = status.get("State", "Unknown")
        self.connection_label.setText(f"Current network: disconnected ({state})")
        self.logger("Wi-Fi disconnected")

    def refresh_connection_status(self):
        try:
            status = self.wifi_manager.current_connection()
        except RuntimeError as exc:
            self.logger(f"Status refresh failed: {exc}")
            return
        ssid = status.get("SSID", "None")
        state = status.get("State", "Unknown")
        self.connection_label.setText(f"Current network: {ssid} ({state})")
        signal = status.get("Signal", "not reported")
        self.devices_label.setText(f"Signal: {signal}")

    def closeEvent(self, event):
        self.listener.close()
        self.command_socket.close()
        super().closeEvent(event)


def main():
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow()
    window.resize(1200, 720)
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
