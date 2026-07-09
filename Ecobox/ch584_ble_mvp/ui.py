from __future__ import annotations

import html
from typing import Any, Optional

from PyQt5.QtCore import QDateTime, Qt
from PyQt5.QtWidgets import (
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QPlainTextEdit,
    QPushButton,
    QSizePolicy,
    QTextBrowser,
    QVBoxLayout,
    QWidget,
)


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("CH584 蓝牙智能控制终端")
        self.resize(1260, 820)
        self.setMinimumSize(1080, 720)

        self.scan_button = QPushButton("扫描设备")
        self.connect_button = QPushButton("连接设备")
        self.disconnect_button = QPushButton("断开连接")
        self.send_button = QPushButton("发送原始指令")
        self.smart_send_button = QPushButton("智能分析并发送")

        self.device_list = QListWidget()
        self.user_input = QPlainTextEdit()
        self.command_input = QLineEdit()
        self.status_label = QLabel("未连接")
        self.log_output = QTextBrowser()
        self.device_count_label = QLabel("0 台设备")
        self.system_hint_label = QLabel("等待开始扫描蓝牙设备")
        self.protocol_hint_label = QLabel("BLE写入：UTF-8 文本，自动追加换行")
        self.timeline_hint_label = QLabel("等待输入控制需求")

        self._connected = False
        self._scan_busy = False
        self._connect_busy = False
        self._send_busy = False
        self._ai_busy = False
        self._execute_busy = False

        self._configure_button(self.scan_button, "secondary")
        self._configure_button(self.connect_button, "primary")
        self._configure_button(self.disconnect_button, "ghost")
        self._configure_button(self.send_button, "secondary")
        self._configure_button(self.smart_send_button, "accent")

        self._build_ui()
        self._set_defaults()

    def _build_ui(self) -> None:
        central_widget = QWidget()
        central_widget.setObjectName("appRoot")
        self.setCentralWidget(central_widget)

        root_layout = QVBoxLayout(central_widget)
        root_layout.setContentsMargins(22, 20, 22, 20)
        root_layout.setSpacing(16)

        root_layout.addWidget(self._build_header_card())

        content_layout = QGridLayout()
        content_layout.setHorizontalSpacing(16)
        content_layout.setVerticalSpacing(16)
        content_layout.setColumnStretch(0, 3)
        content_layout.setColumnStretch(1, 7)

        content_layout.addWidget(self._build_device_card(), 0, 0)
        content_layout.addWidget(self._build_ai_card(), 0, 1)
        content_layout.addWidget(self._build_log_card(), 1, 0, 1, 2)

        root_layout.addLayout(content_layout, 1)

    def _build_header_card(self) -> QFrame:
        card = self._create_card("heroCard")
        layout = QHBoxLayout(card)
        layout.setContentsMargins(24, 20, 24, 20)
        layout.setSpacing(18)

        title_wrap = QVBoxLayout()
        title_wrap.setSpacing(6)

        title = QLabel("智能龟缸 AIoT 控制台")
        title.setObjectName("heroTitle")
        subtitle = QLabel(
            "输入自然语言后，系统将自动完成 AI 解析、蓝牙发送与过程反馈。"
        )
        subtitle.setObjectName("heroSubtitle")

        title_wrap.addWidget(title)
        title_wrap.addWidget(subtitle)

        status_wrap = QVBoxLayout()
        status_wrap.setSpacing(8)
        status_wrap.setAlignment(Qt.AlignRight | Qt.AlignVCenter)

        status_caption = QLabel("连接状态")
        status_caption.setObjectName("metricCaption")
        self.status_label.setObjectName("statusChip")
        self.status_label.setAlignment(Qt.AlignCenter)
        self.status_label.setMinimumWidth(240)
        self.status_label.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Fixed)
        self.system_hint_label.setObjectName("metricValue")
        self.system_hint_label.setAlignment(Qt.AlignRight)

        status_wrap.addWidget(status_caption, 0, Qt.AlignRight)
        status_wrap.addWidget(self.status_label, 0, Qt.AlignRight)
        status_wrap.addWidget(self.system_hint_label, 0, Qt.AlignRight)

        layout.addLayout(title_wrap, 1)
        layout.addLayout(status_wrap)
        return card

    def _build_device_card(self) -> QFrame:
        card = self._create_card()
        layout = QVBoxLayout(card)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(14)

        layout.addWidget(self._section_title("蓝牙设备中心", "扫描附近设备，并与目标模块保持稳定连接。"))

        metrics_row = QHBoxLayout()
        metrics_row.setSpacing(12)
        metrics_row.addWidget(self._build_metric_card("匹配设备", self.device_count_label))
        metrics_row.addWidget(self._build_metric_card("通信方式", self.protocol_hint_label))
        layout.addLayout(metrics_row)

        button_row = QHBoxLayout()
        button_row.setSpacing(10)
        button_row.addWidget(self.scan_button)
        button_row.addWidget(self.connect_button)
        button_row.addWidget(self.disconnect_button)
        layout.addLayout(button_row)

        devices_label = QLabel("已发现的蓝牙设备")
        devices_label.setObjectName("fieldTitle")
        layout.addWidget(devices_label)

        self.device_list.setObjectName("deviceList")
        self.device_list.setMinimumHeight(280)
        self.device_list.setAlternatingRowColors(True)
        layout.addWidget(self.device_list, 1)
        return card

    def _build_ai_card(self) -> QFrame:
        card = self._create_card()
        layout = QVBoxLayout(card)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(14)

        layout.addWidget(
            self._section_title("智能控制区", "一句话输入需求，一键分析并下发控制指令。")
        )

        user_label = QLabel("自然语言输入")
        user_label.setObjectName("fieldTitle")
        layout.addWidget(user_label)

        self.user_input.setObjectName("inputEditor")
        self.user_input.setPlaceholderText(
            "例如：东边有点暗，模拟一下日出；或者：帮我打开风扇，并告诉我执行进度。"
        )
        self.user_input.setFixedHeight(150)
        layout.addWidget(self.user_input)

        helper_text = QLabel(
            "点击下方主按钮后，系统会自动完成 AI 分析、生成控制命令，并通过 BLE 发送到设备。"
        )
        helper_text.setObjectName("sectionSubtitle")
        helper_text.setWordWrap(True)
        layout.addWidget(helper_text)

        layout.addWidget(self.smart_send_button)

        raw_label = QLabel("原始 BLE 指令")
        raw_label.setObjectName("fieldTitle")
        layout.addWidget(raw_label)

        command_row = QHBoxLayout()
        command_row.setSpacing(10)
        self.command_input.setObjectName("rawInput")
        self.command_input.setPlaceholderText(
            "例如：le:on   lw:60   fan:off   feeder:feed   pump:on"
        )
        command_row.addWidget(self.command_input, 1)
        command_row.addWidget(self.send_button)
        layout.addLayout(command_row)
        return card

    def _build_log_card(self) -> QFrame:
        card = self._create_card()
        layout = QVBoxLayout(card)
        layout.setContentsMargins(20, 20, 20, 20)
        layout.setSpacing(12)

        layout.addWidget(
            self._section_title(
                "过程对话与进度",
                "系统会把每一步处理过程显示成气泡，便于实时查看 AI 解析与蓝牙发送状态。",
            )
        )

        self.timeline_hint_label.setObjectName("timelineChip")
        self.timeline_hint_label.setAlignment(Qt.AlignCenter)
        self.timeline_hint_label.setMinimumHeight(34)
        layout.addWidget(self.timeline_hint_label, 0, Qt.AlignLeft)

        self.log_output.setObjectName("logOutput")
        self.log_output.setReadOnly(True)
        self.log_output.setOpenExternalLinks(False)
        self.log_output.setOpenLinks(False)
        self.log_output.setFrameShape(QFrame.NoFrame)
        self.log_output.document().setDocumentMargin(10)
        layout.addWidget(self.log_output, 1)
        return card

    def _set_defaults(self) -> None:
        self._apply_styles()
        self.command_input.returnPressed.connect(self.send_button.click)
        self._set_status_visual(False)
        self._update_button_states()

    def _apply_styles(self) -> None:
        self.setStyleSheet(
            """
            QMainWindow {
                background: #edf3f6;
            }
            QWidget#appRoot {
                background: qlineargradient(
                    x1: 0, y1: 0, x2: 1, y2: 1,
                    stop: 0 #edf3f6,
                    stop: 0.55 #f8faf7,
                    stop: 1 #eef5ef
                );
            }
            QFrame[card="true"] {
                background: rgba(255, 255, 255, 0.96);
                border: 1px solid #d7e2e7;
                border-radius: 18px;
            }
            QFrame#heroCard {
                background: qlineargradient(
                    x1: 0, y1: 0, x2: 1, y2: 0,
                    stop: 0 #12364a,
                    stop: 1 #1f6072
                );
                border: 0;
                border-radius: 22px;
            }
            QLabel#heroTitle {
                color: #f4fbfd;
                font-size: 34px;
                font-weight: 700;
                letter-spacing: 0.4px;
            }
            QLabel#heroSubtitle {
                color: rgba(244, 251, 253, 0.82);
                font-size: 16px;
            }
            QLabel#statusChip {
                background: rgba(255, 255, 255, 0.12);
                color: #fdfefe;
                border: 1px solid rgba(255, 255, 255, 0.18);
                border-radius: 18px;
                padding: 12px 18px;
                font-size: 15px;
                font-weight: 700;
            }
            QLabel#sectionTitle {
                color: #163544;
                font-size: 22px;
                font-weight: 700;
            }
            QLabel#sectionSubtitle {
                color: #5c7180;
                font-size: 14px;
            }
            QLabel#fieldTitle {
                color: #294555;
                font-size: 16px;
                font-weight: 600;
                padding-bottom: 2px;
            }
            QLabel#metricCaption {
                color: #64808f;
                font-size: 12px;
                font-weight: 600;
                text-transform: uppercase;
            }
            QLabel#metricValue {
                color: #173747;
                font-size: 15px;
                font-weight: 700;
            }
            QLabel#timelineChip {
                background: #eef5da;
                color: #5b6505;
                border: 1px solid #d9e2a8;
                border-radius: 16px;
                padding: 8px 14px;
                font-size: 14px;
                font-weight: 700;
            }
            QFrame#metricCard {
                background: #f6fafb;
                border: 1px solid #dce7ec;
                border-radius: 14px;
            }
            QListWidget#deviceList,
            QPlainTextEdit#inputEditor,
            QTextBrowser#logOutput,
            QLineEdit#rawInput {
                background: #fbfdfe;
                color: #173747;
                border: 1px solid #cfdee6;
                border-radius: 14px;
                padding: 12px;
                selection-background-color: #2a7f8f;
                selection-color: white;
                font-size: 15px;
            }
            QPlainTextEdit#inputEditor:focus,
            QTextBrowser#logOutput:focus,
            QLineEdit#rawInput:focus,
            QListWidget#deviceList:focus {
                border: 1px solid #2f8190;
            }
            QListWidget#deviceList::item {
                padding: 10px 8px;
                border-radius: 10px;
                margin: 2px 0;
            }
            QListWidget#deviceList::item:selected {
                background: #d8eef2;
                color: #133847;
            }
            QPushButton {
                min-height: 54px;
                border-radius: 16px;
                padding: 0 22px;
                font-size: 16px;
                font-weight: 700;
                border: 1px solid transparent;
            }
            QPushButton[variant="primary"] {
                background: #1a6675;
                color: white;
                border-color: #1a6675;
            }
            QPushButton[variant="primary"]:hover {
                background: #165a67;
                border-color: #165a67;
            }
            QPushButton[variant="primary"]:pressed {
                background: #114955;
                border-color: #114955;
                padding-top: 1px;
            }
            QPushButton[variant="accent"] {
                background: #e49b32;
                color: #1e1f20;
                border-color: #e49b32;
            }
            QPushButton[variant="accent"]:hover {
                background: #d98b1f;
                border-color: #d98b1f;
            }
            QPushButton[variant="accent"]:pressed {
                background: #c47a13;
                border-color: #c47a13;
                padding-top: 1px;
            }
            QPushButton[variant="secondary"] {
                background: #eef6f8;
                color: #173747;
                border-color: #c6d8df;
            }
            QPushButton[variant="secondary"]:hover {
                background: #e2eff3;
            }
            QPushButton[variant="secondary"]:pressed {
                background: #d5e8ee;
                border-color: #9bbbc5;
                padding-top: 1px;
            }
            QPushButton[variant="ghost"] {
                background: transparent;
                color: #173747;
                border-color: #c6d8df;
            }
            QPushButton[variant="ghost"]:hover {
                background: #f6fafb;
            }
            QPushButton[variant="ghost"]:pressed {
                background: #eaf3f6;
                border-color: #a7c1cb;
                padding-top: 1px;
            }
            QPushButton[busy="true"] {
                background: #143c4c;
                color: #f7fcff;
                border-color: #143c4c;
            }
            QPushButton[busy="true"]:hover,
            QPushButton[busy="true"]:pressed {
                background: #143c4c;
                border-color: #143c4c;
                padding-top: 0;
            }
            QPushButton:disabled {
                background: #d7dee2;
                color: #7b8c95;
                border-color: #d7dee2;
            }
            """
        )

    def _create_card(self, object_name: str = "") -> QFrame:
        card = QFrame()
        card.setProperty("card", True)
        if object_name:
            card.setObjectName(object_name)
        return card

    def _build_metric_card(self, caption: str, value_label: QLabel) -> QFrame:
        card = QFrame()
        card.setObjectName("metricCard")
        layout = QVBoxLayout(card)
        layout.setContentsMargins(14, 12, 14, 12)
        layout.setSpacing(6)

        caption_label = QLabel(caption)
        caption_label.setObjectName("metricCaption")
        layout.addWidget(caption_label)
        layout.addWidget(value_label)
        return card

    def _section_title(self, title_text: str, subtitle_text: str) -> QWidget:
        wrapper = QWidget()
        layout = QVBoxLayout(wrapper)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)

        title = QLabel(title_text)
        title.setObjectName("sectionTitle")
        subtitle = QLabel(subtitle_text)
        subtitle.setObjectName("sectionSubtitle")
        subtitle.setWordWrap(True)

        layout.addWidget(title)
        layout.addWidget(subtitle)
        return wrapper

    def _configure_button(self, button: QPushButton, variant: str) -> None:
        button.setProperty("variant", variant)
        button.setProperty("busy", False)

    def _refresh_button_style(self, button: QPushButton) -> None:
        button.style().unpolish(button)
        button.style().polish(button)
        button.update()

    def _set_button_busy(self, button: QPushButton, busy: bool, idle_text: str, busy_text: str) -> None:
        button.setText(busy_text if busy else idle_text)
        button.setProperty("busy", busy)
        self._refresh_button_style(button)

    def populate_devices(self, devices: list[dict[str, Any]]) -> None:
        self.device_list.clear()

        for index, device in enumerate(devices):
            title = f"{device['name']}  [{device['address']}]"
            if device.get("matched"):
                title += "  <CH58x Match>"

            item = QListWidgetItem(title)
            item.setData(Qt.UserRole, device)
            self.device_list.addItem(item)

            if index == 0:
                self.device_list.setCurrentItem(item)

        matched_count = sum(1 for item in devices if item.get("matched"))
        self.device_count_label.setText(f"{matched_count} matched / {len(devices)} total")
        self._update_button_states()

    def selected_device(self) -> Optional[dict[str, Any]]:
        item = self.device_list.currentItem()
        if item is None:
            return None
        return item.data(Qt.UserRole)

    def current_user_text(self) -> str:
        return self.user_input.toPlainText()

    def current_command(self) -> str:
        return self.command_input.text()

    def clear_user_text(self) -> None:
        self.user_input.clear()

    def clear_command(self) -> None:
        self.command_input.clear()

    def append_log(self, message: str) -> None:
        timestamp = QDateTime.currentDateTime().toString("HH:mm:ss")
        kind = "system"
        title = "系统"

        lowered = message.lower()
        if message.startswith("Step "):
            kind = "progress"
            title = "进度"
        elif "failed" in lowered or "error" in lowered:
            kind = "error"
            title = "错误"
        elif "connected" in lowered or "disconnected" in lowered or "scanning" in lowered:
            kind = "status"
            title = "蓝牙状态"

        self._append_bubble(title, message, timestamp, kind=kind)

    def append_tagged_log(self, tag: str, message: str) -> None:
        timestamp = QDateTime.currentDateTime().toString("HH:mm:ss")
        upper_tag = tag.upper()
        if upper_tag == "USER":
            self._append_bubble("用户输入", message, timestamp, kind="user", align="right")
            return
        if upper_tag == "AI":
            self._append_bubble("AI 结果", message, timestamp, kind="ai", monospace=True)
            return
        if upper_tag == "BLE":
            self._append_bubble("BLE 发送", message, timestamp, kind="ble", monospace=True)
            return

        self._append_bubble(upper_tag, message, timestamp, kind="system")

    def _append_bubble(
        self,
        title: str,
        message: str,
        timestamp: str,
        *,
        kind: str,
        align: str = "left",
        monospace: bool = False,
    ) -> None:
        palette = {
            "system": {
                "fill": "#f4f7fb",
                "border": "#d8e0ea",
                "title": "#587181",
                "text": "#193847",
            },
            "progress": {
                "fill": "#e8f4f8",
                "border": "#bed9e4",
                "title": "#2f6f83",
                "text": "#103744",
            },
            "status": {
                "fill": "#e9f4ea",
                "border": "#cde2d0",
                "title": "#41704c",
                "text": "#204128",
            },
            "error": {
                "fill": "#fbecec",
                "border": "#efc4c4",
                "title": "#a74747",
                "text": "#672e2e",
            },
            "user": {
                "fill": "#1d6071",
                "border": "#1d6071",
                "title": "#c4eaf2",
                "text": "#f5fcfe",
            },
            "ai": {
                "fill": "#fff6e7",
                "border": "#efd49d",
                "title": "#9b6a10",
                "text": "#4f3908",
            },
            "ble": {
                "fill": "#eef7f9",
                "border": "#c8dce2",
                "title": "#3a6874",
                "text": "#143743",
            },
        }
        colors = palette[kind]
        safe_title = html.escape(title)
        safe_timestamp = html.escape(timestamp)

        if monospace:
            safe_message = (
                html.escape(message)
                .replace(" ", "&nbsp;")
                .replace("\n", "<br>")
            )
            body_html = (
                f"<div style=\"font-family:'Consolas','Courier New',monospace;"
                f" font-size:13px; line-height:1.55; color:{colors['text']};\">{safe_message}</div>"
            )
        else:
            safe_message = html.escape(message).replace("\n", "<br>")
            body_html = (
                f"<div style=\"font-size:13px; line-height:1.62; color:{colors['text']};\">{safe_message}</div>"
            )

        if align == "right":
            row_html = (
                "<table width='100%' cellspacing='0' cellpadding='0' style='margin: 8px 0 12px 0;'>"
                "<tr>"
                "<td width='24%'></td>"
                "<td width='76%' align='right'>"
            )
        else:
            row_html = (
                "<table width='100%' cellspacing='0' cellpadding='0' style='margin: 8px 0 12px 0;'>"
                "<tr>"
                "<td width='76%' align='left'>"
            )

        bubble_html = (
            f"{row_html}"
            f"<div style=\"background:{colors['fill']}; border:1px solid {colors['border']};"
            " border-radius:18px; padding:12px 14px;\">"
            f"<div style=\"font-size:10px; font-weight:700; letter-spacing:0.8px;"
            f" text-transform:uppercase; color:{colors['title']}; margin-bottom:6px;\">"
            f"{safe_title} | {safe_timestamp}</div>"
            f"{body_html}"
            "</div>"
        )

        if align == "right":
            bubble_html += "</td></tr></table>"
        else:
            bubble_html += "</td><td width='24%'></td></tr></table>"

        self.log_output.append(bubble_html)
        self.log_output.verticalScrollBar().setValue(self.log_output.verticalScrollBar().maximum())

    def set_connected(self, connected: bool, text: str) -> None:
        self._connected = connected
        self.status_label.setText(text)
        self._set_status_visual(connected)
        self._update_button_states()

    def set_scan_in_progress(self, active: bool) -> None:
        self._scan_busy = active
        self._update_button_states()

    def set_connect_in_progress(self, active: bool) -> None:
        self._connect_busy = active
        self._update_button_states()

    def set_send_in_progress(self, active: bool) -> None:
        self._send_busy = active
        self._update_button_states()

    def set_ai_in_progress(self, active: bool) -> None:
        self._ai_busy = active
        self._update_button_states()

    def set_execute_in_progress(self, active: bool) -> None:
        self._execute_busy = active
        self._update_button_states()

    def _set_status_visual(self, connected: bool) -> None:
        if connected:
            style = (
                "background: rgba(37, 182, 112, 0.18);"
                "color: #effcf5;"
                "border: 1px solid rgba(199, 247, 221, 0.28);"
                "border-radius: 18px;"
                "padding: 12px 18px;"
                "font-size: 15px;"
                "font-weight: 700;"
            )
        else:
            style = (
                "background: rgba(181, 43, 59, 0.20);"
                "color: #fff3f4;"
                "border: 1px solid rgba(255, 215, 220, 0.25);"
                "border-radius: 18px;"
                "padding: 12px 18px;"
                "font-size: 15px;"
                "font-weight: 700;"
            )
        self.status_label.setStyleSheet(style)

    def _update_button_states(self) -> None:
        has_devices = self.device_list.count() > 0

        self.scan_button.setEnabled(not self._scan_busy and not self._connect_busy)
        self.connect_button.setEnabled(
            has_devices and not self._connected and not self._scan_busy and not self._connect_busy
        )
        self.disconnect_button.setEnabled(self._connected and not self._connect_busy)
        self.send_button.setEnabled(self._connected and not self._send_busy and not self._execute_busy)
        self.smart_send_button.setEnabled(
            self._connected and not self._ai_busy and not self._execute_busy and not self._send_busy
        )
        self._set_button_busy(self.scan_button, self._scan_busy, "扫描设备", "正在扫描...")
        self._set_button_busy(self.connect_button, self._connect_busy, "连接设备", "正在连接...")
        self._set_button_busy(self.disconnect_button, False, "断开连接", "断开连接")
        self._set_button_busy(self.send_button, self._send_busy, "发送原始指令", "正在发送...")
        self._set_button_busy(
            self.smart_send_button,
            self._ai_busy or self._execute_busy,
            "智能分析并发送",
            "AI 正在分析..." if self._ai_busy else "正在蓝牙发送...",
        )
        self._update_system_hint()

    def _update_system_hint(self) -> None:
        if self._execute_busy:
            self.system_hint_label.setText("正在通过 BLE 发送 AI 生成的控制指令...")
            self.timeline_hint_label.setText("第 3 步 / 共 3 步：蓝牙载荷发送中")
            return

        if self._send_busy:
            self.system_hint_label.setText("正在发送原始 BLE 指令...")
            self.timeline_hint_label.setText("原始 BLE 指令发送中")
            return

        if self._ai_busy:
            self.system_hint_label.setText("AI 正在分析输入语句...")
            self.timeline_hint_label.setText("第 1 步 / 共 3 步：自然语言转控制 JSON")
            return

        if self._connect_busy:
            self.system_hint_label.setText("正在建立 BLE 连接...")
            self.timeline_hint_label.setText("连接所选蓝牙设备中")
            return

        if self._scan_busy:
            self.system_hint_label.setText("正在扫描附近蓝牙设备...")
            self.timeline_hint_label.setText("搜索 CH584 及其他 BLE 外设中")
            return

        if self._connected:
            self.system_hint_label.setText("BLE 连接已建立")
            self.timeline_hint_label.setText("已连接，可开始自然语言控制")
            return

        if self.device_list.count() > 0:
            self.system_hint_label.setText("扫描完成")
            self.timeline_hint_label.setText("请选择设备并开始控制流程")
            return

        self.system_hint_label.setText("等待开始扫描蓝牙设备")
        self.timeline_hint_label.setText("等待输入控制需求")
