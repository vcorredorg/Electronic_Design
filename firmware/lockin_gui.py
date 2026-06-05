import csv
import sys
import time
from collections import deque

import serial
import serial.tools.list_ports
from PyQt5 import QtCore, QtWidgets
import pyqtgraph as pg


FIELD_NAMES = [
    "error", "output_scale", "gain", "sync_filter", "external_reference",
    "samples_per_period", "sample_rate", "reference_frequency", "time_constant",
    "undersampling", "r", "phase", "noise", "x1", "y1",
    "xh0", "xh1", "xh2", "yh0", "yh1", "yh2", "first_harmonic",
]


class SerialWorker(QtCore.QObject):
    sample_received = QtCore.pyqtSignal(dict)
    status_changed = QtCore.pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.port_name = None
        self.baudrate = 2_000_000
        self.serial_port = None
        self.running = False

    @QtCore.pyqtSlot(str)
    def connect_port(self, port_name):
        self.disconnect_port()
        self.port_name = port_name
        try:
            self.serial_port = serial.Serial(port_name, self.baudrate, timeout=0.05)
            self.running = True
            self.status_changed.emit(f"Connected to {port_name}")
        except Exception as exc:
            self.serial_port = None
            self.running = False
            self.status_changed.emit(f"Connection failed: {exc}")

    @QtCore.pyqtSlot()
    def disconnect_port(self):
        self.running = False
        if self.serial_port is not None:
            try:
                self.serial_port.close()
            except Exception:
                pass
        self.serial_port = None
        self.status_changed.emit("Disconnected")

    @QtCore.pyqtSlot(str)
    def send_command(self, command):
        if self.serial_port is None or not self.serial_port.is_open:
            self.status_changed.emit("Serial port not open")
            return
        try:
            self.serial_port.write((command.strip() + "\n").encode("ascii"))
        except Exception as exc:
            self.status_changed.emit(f"Write failed: {exc}")

    @QtCore.pyqtSlot()
    def poll(self):
        if not self.running or self.serial_port is None:
            return
        try:
            while self.serial_port.in_waiting:
                line = self.serial_port.readline().decode("ascii", errors="ignore").strip()
                if not line:
                    continue
                values = line.split()
                if len(values) < len(FIELD_NAMES):
                    continue
                data = {}
                for name, value in zip(FIELD_NAMES, values):
                    try:
                        data[name] = float(value)
                    except ValueError:
                        data[name] = 0.0
                data["timestamp"] = time.time()
                self.sample_received.emit(data)
        except Exception as exc:
            self.status_changed.emit(f"Read failed: {exc}")
            self.disconnect_port()


class MainWindow(QtWidgets.QMainWindow):
    send_command_signal = QtCore.pyqtSignal(str)
    connect_signal = QtCore.pyqtSignal(str)
    disconnect_signal = QtCore.pyqtSignal()

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Lock-in control")
        self.resize(1100, 720)

        self.t0 = None
        self.times = deque(maxlen=1000)
        self.r_values = deque(maxlen=1000)
        self.phase_values = deque(maxlen=1000)
        self.x_values = deque(maxlen=1000)
        self.y_values = deque(maxlen=1000)
        self.rows = []

        self.worker = SerialWorker()
        self.thread = QtCore.QThread(self)
        self.worker.moveToThread(self.thread)
        self.connect_signal.connect(self.worker.connect_port)
        self.disconnect_signal.connect(self.worker.disconnect_port)
        self.send_command_signal.connect(self.worker.send_command)
        self.worker.sample_received.connect(self.on_sample)
        self.worker.status_changed.connect(self.statusBar().showMessage)
        self.thread.start()

        self.poll_timer = QtCore.QTimer(self)
        self.poll_timer.timeout.connect(self.worker.poll)
        self.poll_timer.start(20)

        self._build_ui()
        self.refresh_ports()

    def _build_ui(self):
        central = QtWidgets.QWidget()
        self.setCentralWidget(central)
        layout = QtWidgets.QVBoxLayout(central)

        top = QtWidgets.QHBoxLayout()
        layout.addLayout(top)

        self.port_box = QtWidgets.QComboBox()
        refresh_button = QtWidgets.QPushButton("Refresh")
        connect_button = QtWidgets.QPushButton("Connect")
        disconnect_button = QtWidgets.QPushButton("Disconnect")
        refresh_button.clicked.connect(self.refresh_ports)
        connect_button.clicked.connect(lambda: self.connect_signal.emit(self.port_box.currentText()))
        disconnect_button.clicked.connect(self.disconnect_signal.emit)

        top.addWidget(QtWidgets.QLabel("Port"))
        top.addWidget(self.port_box, 1)
        top.addWidget(refresh_button)
        top.addWidget(connect_button)
        top.addWidget(disconnect_button)

        controls = QtWidgets.QGridLayout()
        layout.addLayout(controls)

        self.freq_edit = QtWidgets.QLineEdit("1000")
        self.gain_box = QtWidgets.QComboBox()
        self.gain_box.addItems(["0", "1", "2", "4", "8", "16", "32", "64"])
        self.gain_box.setCurrentText("1")
        self.tau_edit = QtWidgets.QLineEdit("0.6")
        self.scale_edit = QtWidgets.QLineEdit("10")
        self.harm_edit = QtWidgets.QLineEdit("2")

        set_freq = QtWidgets.QPushButton("Set frequency")
        set_gain = QtWidgets.QPushButton("Set gain")
        set_tau = QtWidgets.QPushButton("Set tau")
        set_scale = QtWidgets.QPushButton("Set scale")
        set_harm = QtWidgets.QPushButton("Set harmonics")
        toggle_filter = QtWidgets.QPushButton("Toggle sync filter")
        toggle_ref = QtWidgets.QPushButton("Toggle external ref")
        recalc_ref = QtWidgets.QPushButton("Recalculate ref")
        save_csv = QtWidgets.QPushButton("Save CSV")

        set_freq.clicked.connect(lambda: self.send_command_signal.emit(self.freq_edit.text()))
        set_gain.clicked.connect(lambda: self.send_command_signal.emit("g" + self.gain_box.currentText()))
        set_tau.clicked.connect(lambda: self.send_command_signal.emit("e" + self.tau_edit.text()))
        set_scale.clicked.connect(lambda: self.send_command_signal.emit("s" + self.scale_edit.text()))
        set_harm.clicked.connect(lambda: self.send_command_signal.emit("h" + self.harm_edit.text()))
        toggle_filter.clicked.connect(lambda: self.send_command_signal.emit("t"))
        toggle_ref.clicked.connect(lambda: self.send_command_signal.emit("r"))
        recalc_ref.clicked.connect(lambda: self.send_command_signal.emit("c"))
        save_csv.clicked.connect(self.save_csv)

        controls.addWidget(QtWidgets.QLabel("Frequency Hz"), 0, 0)
        controls.addWidget(self.freq_edit, 0, 1)
        controls.addWidget(set_freq, 0, 2)
        controls.addWidget(QtWidgets.QLabel("Gain"), 0, 3)
        controls.addWidget(self.gain_box, 0, 4)
        controls.addWidget(set_gain, 0, 5)
        controls.addWidget(QtWidgets.QLabel("Tau s"), 1, 0)
        controls.addWidget(self.tau_edit, 1, 1)
        controls.addWidget(set_tau, 1, 2)
        controls.addWidget(QtWidgets.QLabel("Output scale"), 1, 3)
        controls.addWidget(self.scale_edit, 1, 4)
        controls.addWidget(set_scale, 1, 5)
        controls.addWidget(QtWidgets.QLabel("First harmonic"), 2, 0)
        controls.addWidget(self.harm_edit, 2, 1)
        controls.addWidget(set_harm, 2, 2)
        controls.addWidget(toggle_filter, 2, 3)
        controls.addWidget(toggle_ref, 2, 4)
        controls.addWidget(recalc_ref, 2, 5)
        controls.addWidget(save_csv, 2, 6)

        self.plot_r = pg.PlotWidget(title="Amplitude R")
        self.plot_phase = pg.PlotWidget(title="Phase")
        self.plot_xy = pg.PlotWidget(title="X/Y")
        self.curve_r = self.plot_r.plot([], [])
        self.curve_phase = self.plot_phase.plot([], [])
        self.curve_x = self.plot_xy.plot([], [], name="X")
        self.curve_y = self.plot_xy.plot([], [], name="Y")

        plots = QtWidgets.QSplitter(QtCore.Qt.Vertical)
        plots.addWidget(self.plot_r)
        plots.addWidget(self.plot_phase)
        plots.addWidget(self.plot_xy)
        layout.addWidget(plots, 1)

        self.status_label = QtWidgets.QLabel("No data")
        layout.addWidget(self.status_label)

    def refresh_ports(self):
        self.port_box.clear()
        for port in serial.tools.list_ports.comports():
            self.port_box.addItem(port.device)

    @QtCore.pyqtSlot(dict)
    def on_sample(self, data):
        if self.t0 is None:
            self.t0 = data["timestamp"]
        t = data["timestamp"] - self.t0
        self.times.append(t)
        self.r_values.append(data["r"])
        self.phase_values.append(data["phase"])
        self.x_values.append(data["x1"])
        self.y_values.append(data["y1"])
        self.rows.append(data)

        xs = list(self.times)
        self.curve_r.setData(xs, list(self.r_values))
        self.curve_phase.setData(xs, list(self.phase_values))
        self.curve_x.setData(xs, list(self.x_values))
        self.curve_y.setData(xs, list(self.y_values))

        self.status_label.setText(
            f"R={data['r']:.5f} phase={data['phase']:.5f} Hz={data['reference_frequency']:.2f} "
            f"gain={int(data['gain'])} error={int(data['error'])}"
        )

    def save_csv(self):
        filename, _ = QtWidgets.QFileDialog.getSaveFileName(self, "Save CSV", "lockin_data.csv", "CSV (*.csv)")
        if not filename:
            return
        with open(filename, "w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=["timestamp"] + FIELD_NAMES)
            writer.writeheader()
            writer.writerows(self.rows)
        self.statusBar().showMessage(f"Saved {filename}")

    def closeEvent(self, event):
        self.disconnect_signal.emit()
        self.thread.quit()
        self.thread.wait(1000)
        super().closeEvent(event)


if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
