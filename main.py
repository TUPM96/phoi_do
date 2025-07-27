import sys
import serial
import serial.tools.list_ports
import json
from PyQt5.QtWidgets import (
    QApplication, QWidget, QVBoxLayout, QPushButton, QListWidget,
    QLabel, QMessageBox, QTextEdit, QHBoxLayout, QGridLayout
)
from PyQt5.QtCore import QTimer

class SerialControl(QWidget):
    def __init__(self):
        super().__init__()
        self.ser = None
        self.current_mode = "AUTO"
        self.current_action = "PHOI"

        self.setWindowTitle("Điều Khiển Arduino qua Bluetooth (JSON HC-06)")

        self.layout = QVBoxLayout()
        self.label = QLabel("Chọn cổng COM của HC-06 sau khi đã ghép nối Bluetooth")
        self.layout.addWidget(self.label)

        self.com_list = QListWidget()
        self.layout.addWidget(self.com_list)

        self.refresh_button = QPushButton("Quét cổng COM")
        self.refresh_button.clicked.connect(self.scan_ports)
        self.layout.addWidget(self.refresh_button)

        self.connect_button = QPushButton("Kết nối")
        self.connect_button.clicked.connect(self.connect_port)
        self.layout.addWidget(self.connect_button)

        # Tạo layout cho nút điều khiển
        self.button_layout = QHBoxLayout()
        self.mode_button = QPushButton("Chế độ: AUTO")
        self.mode_button.clicked.connect(self.toggle_mode)
        self.mode_button.setEnabled(False)
        self.button_layout.addWidget(self.mode_button)

        self.action_button = QPushButton("PHƠI")
        self.action_button.clicked.connect(self.toggle_action)
        self.action_button.setEnabled(False)
        self.button_layout.addWidget(self.action_button)

        self.layout.addLayout(self.button_layout)

        # Hiện trạng thái cảm biến, trạng thái máy
        self.status_grid = QGridLayout()
        self.labels = {}
        fields = ['Nhiệt độ', 'Độ ẩm', 'Ánh sáng', 'Mưa', 'Trạng thái', 'Chế độ']
        for i, f in enumerate(fields):
            lbl_name = QLabel(f"{f}:")
            lbl_val = QLabel("-")
            self.status_grid.addWidget(lbl_name, i, 0)
            self.status_grid.addWidget(lbl_val, i, 1)
            self.labels[f] = lbl_val
        self.layout.addLayout(self.status_grid)

        # Log nhận dữ liệu gốc
        self.log_box = QTextEdit()
        self.log_box.setReadOnly(True)
        self.layout.addWidget(self.log_box)

        self.setLayout(self.layout)
        self.scan_ports()

        # Timer để đọc dữ liệu từ serial
        self.timer = QTimer()
        self.timer.timeout.connect(self.read_serial_data)
        self.timer.start(200)

    def scan_ports(self):
        self.com_list.clear()
        ports = serial.tools.list_ports.comports()
        for port in ports:
            self.com_list.addItem(f"{port.device} - {port.description}")
        if not ports:
            self.label.setText("Không tìm thấy cổng COM nào.")
        else:
            self.label.setText("Chọn cổng COM của HC-06 (thường là tên có 'Bluetooth' hoặc 'Serial')")

    def connect_port(self):
        selected = self.com_list.currentItem()
        if not selected:
            QMessageBox.warning(self, "Lỗi", "Hãy chọn cổng COM để kết nối.")
            return
        port_name = selected.text().split(' ')[0]
        try:
            self.ser = serial.Serial(port_name, 9600, timeout=1)
            self.label.setText(f"Đã kết nối với {port_name}")
            self.mode_button.setEnabled(True)
            self.action_button.setEnabled(True)
        except Exception as e:
            self.label.setText("Kết nối thất bại!")
            QMessageBox.critical(self, "Lỗi", str(e))

    def send_json_command(self, obj):
        if self.ser and self.ser.is_open:
            try:
                cmd = json.dumps(obj)
                self.ser.write((cmd + "\n").encode())
                self.label.setText(f"Đã gửi lệnh: {cmd}")
            except Exception as e:
                self.label.setText("Gửi lệnh thất bại!")
                QMessageBox.critical(self, "Lỗi", str(e))
        else:
            QMessageBox.warning(self, "Lỗi", "Chưa kết nối cổng COM.")

    def toggle_mode(self):
        # Đảo trạng thái mode
        if self.current_mode == "AUTO":
            self.current_mode = "MANUAL"
        else:
            self.current_mode = "AUTO"
        self.mode_button.setText(f"Chế độ: {self.current_mode}")
        self.send_json_command({"mode": self.current_mode.lower()})

    def toggle_action(self):
        # Đảo trạng thái thu/phơi (chỉ gửi khi ở MANUAL)
        if self.current_action == "PHOI":
            self.current_action = "THU"
        else:
            self.current_action = "PHOI"
        self.action_button.setText(self.current_action)
        if self.current_mode == "MANUAL":
            self.send_json_command({"action": self.current_action.lower()})

    def read_serial_data(self):
        if self.ser and self.ser.is_open and self.ser.in_waiting:
            try:
                data = self.ser.readline().decode(errors='ignore').strip()
                if data:
                    self.log_box.append(f"Nhận: {data}")
                    try:
                        j = json.loads(data)
                        # Hiển thị lên các trường
                        self.labels['Nhiệt độ'].setText(
                            "-" if j.get("temp") is None else f"{j.get('temp')} °C"
                        )
                        self.labels['Độ ẩm'].setText(
                            "-" if j.get("humi") is None else f"{j.get('humi')} %"
                        )
                        self.labels['Ánh sáng'].setText(j.get("light", "-"))
                        self.labels['Mưa'].setText("Có" if j.get("rain") else "Không")
                        self.labels['Trạng thái'].setText(j.get("action", "-"))
                        self.labels['Chế độ'].setText(j.get("mode", "-"))
                        # Đồng bộ nút nếu trạng thái thay đổi từ Arduino
                        if j.get("mode") and j.get("mode").upper() != self.current_mode:
                            self.current_mode = j.get("mode").upper()
                            self.mode_button.setText(f"Chế độ: {self.current_mode}")
                        if j.get("action") and j.get("action").upper() != self.current_action:
                            self.current_action = j.get("action").upper()
                            self.action_button.setText(self.current_action)
                    except Exception as e:
                        # Không phải json, chỉ log thô
                        pass
            except Exception as e:
                pass

    def closeEvent(self, event):
        if self.ser and self.ser.is_open:
            self.ser.close()
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = SerialControl()
    window.show()
    sys.exit(app.exec_())