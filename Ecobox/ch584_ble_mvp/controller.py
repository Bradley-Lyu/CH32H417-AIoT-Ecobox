from __future__ import annotations

import asyncio
import concurrent.futures
import json
import threading
from typing import Any

from PyQt5.QtCore import QObject, pyqtSignal

from ai_engine import AIEngine
from ble_client import BLEClient
from prompt import normalize_command
from ui import MainWindow


class WorkerSignals(QObject):
    devices_ready = pyqtSignal(object)
    connected = pyqtSignal(object)
    disconnected = pyqtSignal(str)
    sent = pyqtSignal(object)
    smart_done = pyqtSignal(object)
    log = pyqtSignal(str)
    error = pyqtSignal(str)
    scan_state = pyqtSignal(bool)
    connect_state = pyqtSignal(bool)
    send_state = pyqtSignal(bool)
    ai_state = pyqtSignal(bool)
    execute_state = pyqtSignal(bool)


class BleCommandBridge:
    """Converts normalized JSON control commands into legacy BLE text payloads."""

    def to_ble_payload(self, command: dict[str, Any]) -> str:
        normalized = normalize_command(command)
        device = normalized["device"]
        action = normalized["action"]
        value = normalized["value"]

        if device == "none" or action == "none":
            raise ValueError("AI result is uncertain. Please refine the instruction first.")

        if device == "lighteast":
            if action == "turn_on":
                return "le:on"
            if action == "turn_off":
                return "le:off"
            if action == "set_brightness":
                return f"le:{value}"

        if device == "lightwest":
            if action == "turn_on":
                return "lw:on"
            if action == "turn_off":
                return "lw:off"
            if action == "set_brightness":
                return f"lw:{value}"

        if device == "uvb":
            return "uvb:on" if action == "turn_on" else "uvb:off"

        if device == "feeder" and action == "feed":
            return f"feeder:feed:{value}" if value > 0 else "feeder:feed"

        if device == "fan":
            return "fan:on" if action == "turn_on" else "fan:off"

        if device == "pump":
            return "pump:on" if action == "turn_on" else "pump:off"

        raise ValueError(f"Unsupported command: {normalized}")


class AsyncLoopThread(threading.Thread):
    def __init__(self) -> None:
        super().__init__(daemon=True)
        self._ready = threading.Event()
        self._loop: asyncio.AbstractEventLoop | None = None

    def run(self) -> None:
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._ready.set()
        self._loop.run_forever()

        pending = asyncio.all_tasks(self._loop)
        for task in pending:
            task.cancel()
        if pending:
            self._loop.run_until_complete(asyncio.gather(*pending, return_exceptions=True))
        self._loop.close()

    def submit(self, coro: Any) -> concurrent.futures.Future[Any]:
        self._ready.wait()
        if self._loop is None:
            raise RuntimeError("Async event loop is not running.")
        return asyncio.run_coroutine_threadsafe(coro, self._loop)

    def stop(self) -> None:
        if self._loop is not None and self._loop.is_running():
            self._loop.call_soon_threadsafe(self._loop.stop)


class AppController:
    SEND_TIMEOUT_SECONDS = 8.0

    def __init__(self) -> None:
        self.ui = MainWindow()
        self.signals = WorkerSignals()
        self.worker = AsyncLoopThread()
        self.ble_client = BLEClient(disconnect_callback=self._handle_remote_disconnect)
        self.ai_engine = AIEngine()
        self.command_bridge = BleCommandBridge()

        self.worker.start()
        self._bind_signals()
        self._bind_events()

        self.ui.append_log("程序已启动，请先扫描 CH584 或其他 CH58x 蓝牙设备。")
        self.ui.append_log("智能模式会自动完成 AI 分析、指令生成和 BLE 发送。")
        self.ui.append_log("AI 模块使用本地 Ollama，地址为 http://localhost:11434，模型为 qwen2.5:3b。")

    def show(self) -> None:
        self.ui.show()

    def shutdown(self) -> None:
        try:
            future = self.worker.submit(self.ble_client.disconnect())
            future.result(timeout=5)
        except Exception:
            pass
        self.worker.stop()
        self.worker.join(timeout=2)

    def _bind_signals(self) -> None:
        self.signals.devices_ready.connect(self._on_devices_ready)
        self.signals.connected.connect(self._on_connected)
        self.signals.disconnected.connect(self._on_disconnected)
        self.signals.sent.connect(self._on_sent)
        self.signals.smart_done.connect(self._on_smart_done)
        self.signals.log.connect(self.ui.append_log)
        self.signals.error.connect(self._on_error)
        self.signals.scan_state.connect(self.ui.set_scan_in_progress)
        self.signals.connect_state.connect(self.ui.set_connect_in_progress)
        self.signals.send_state.connect(self.ui.set_send_in_progress)
        self.signals.ai_state.connect(self.ui.set_ai_in_progress)
        self.signals.execute_state.connect(self.ui.set_execute_in_progress)

    def _bind_events(self) -> None:
        self.ui.scan_button.clicked.connect(self.scan_devices)
        self.ui.connect_button.clicked.connect(self.connect_device)
        self.ui.disconnect_button.clicked.connect(self.disconnect_device)
        self.ui.send_button.clicked.connect(self.send_command)
        self.ui.smart_send_button.clicked.connect(self.process_natural_language_command)

    def scan_devices(self) -> None:
        self.signals.scan_state.emit(True)
        self.signals.log.emit("Scanning BLE devices...")

        future = self.worker.submit(self.ble_client.scan_devices())
        future.add_done_callback(self._scan_completed)

    def connect_device(self) -> None:
        device = self.ui.selected_device()
        if device is None:
            self.ui.append_log("请先从设备列表中选择一个蓝牙设备。")
            return

        self.signals.connect_state.emit(True)
        self.signals.log.emit(f"正在连接 {device['name']} [{device['address']}]...")

        future = self.worker.submit(self.ble_client.connect(device))
        future.add_done_callback(self._connect_completed)

    def disconnect_device(self) -> None:
        self.signals.log.emit("正在断开蓝牙设备连接...")
        future = self.worker.submit(self.ble_client.disconnect())
        future.add_done_callback(self._disconnect_completed)

    def send_command(self) -> None:
        command = self.ui.current_command().strip()
        if not command:
            self.ui.append_log("请先输入原始指令，再执行发送。")
            return

        self.signals.send_state.emit(True)
        self.ui.append_tagged_log("BLE", f"发送原始指令：\n{command}")

        future = self.worker.submit(self._send_ble_payload(command))
        future.add_done_callback(lambda item: self._send_completed(item, "raw"))

    def process_natural_language_command(self) -> None:
        user_text = self.ui.current_user_text().strip()
        if not user_text:
            self.ui.append_log("请先输入自然语言控制需求。")
            return

        self.ui.append_tagged_log("USER", user_text)
        self.signals.ai_state.emit(True)
        self.signals.log.emit("Step 1/3: AI 正在分析你的控制需求...")

        future = self.worker.submit(self._run_smart_command_pipeline(user_text))
        future.add_done_callback(self._smart_command_completed)

    async def _send_ble_payload(self, payload: str) -> str:
        try:
            return await asyncio.wait_for(
                self.ble_client.send(payload),
                timeout=self.SEND_TIMEOUT_SECONDS,
            )
        except asyncio.TimeoutError as exc:
            raise RuntimeError(
                f"BLE send timed out after {self.SEND_TIMEOUT_SECONDS:.0f}s. "
                "The device may be disconnected or not accepting writes."
            ) from exc

    async def _run_smart_command_pipeline(self, user_text: str) -> dict[str, Any]:
        normalized = normalize_command(await self.ai_engine.analyze_text(user_text))
        if normalized["device"] == "none" or normalized["action"] == "none":
            raise ValueError("AI could not determine a supported device action from that sentence.")

        self.signals.log.emit("Step 2/3: AI 分析完成，正在生成控制命令...")
        self.signals.log.emit(f"AI JSON -> {json.dumps(normalized, ensure_ascii=False)}")

        payload = self.command_bridge.to_ble_payload(normalized)
        self.signals.ai_state.emit(False)
        self.signals.execute_state.emit(True)
        self.signals.log.emit(f"Step 3/3: 正在发送 BLE 指令 -> {payload}")

        sent_payload = await self._send_ble_payload(payload)
        return {"json": normalized, "payload": sent_payload}

    def _scan_completed(self, future: concurrent.futures.Future[Any]) -> None:
        self.signals.scan_state.emit(False)
        try:
            devices = future.result()
        except Exception as exc:
            self.signals.error.emit(f"Scan failed: {exc}")
            return
        self.signals.devices_ready.emit(devices)

    def _connect_completed(self, future: concurrent.futures.Future[Any]) -> None:
        self.signals.connect_state.emit(False)
        try:
            info = future.result()
        except Exception as exc:
            self.signals.error.emit(f"Connect failed: {exc}")
            return
        self.signals.connected.emit(info)

    def _disconnect_completed(self, future: concurrent.futures.Future[Any]) -> None:
        try:
            future.result()
        except Exception as exc:
            self.signals.error.emit(f"Disconnect failed: {exc}")
            return
        self.signals.disconnected.emit("设备已断开连接。")

    def _smart_command_completed(self, future: concurrent.futures.Future[Any]) -> None:
        self.signals.ai_state.emit(False)
        self.signals.execute_state.emit(False)
        try:
            result = future.result()
        except Exception as exc:
            self.signals.error.emit(f"智能控制失败：{exc}")
            return
        self.signals.smart_done.emit(result)

    def _send_completed(self, future: concurrent.futures.Future[Any], context: str) -> None:
        self.signals.send_state.emit(False)
        self.signals.execute_state.emit(False)
        try:
            payload = future.result()
        except Exception as exc:
            self.signals.error.emit(f"发送失败：{exc}")
            return
        self.signals.sent.emit({"payload": payload, "context": context})

    def _handle_remote_disconnect(self, message: str) -> None:
        self.signals.disconnected.emit(message)

    def _on_devices_ready(self, devices: list[dict[str, Any]]) -> None:
        self.ui.populate_devices(devices)
        if devices:
            matched_count = sum(1 for item in devices if item.get("matched"))
            self.ui.append_log(
                f"扫描完成，共发现 {len(devices)} 台设备，其中 {matched_count} 台符合 CH58x 筛选条件。"
            )
        else:
            self.ui.append_log("未发现可用的蓝牙设备。")

    def _on_connected(self, info: dict[str, str]) -> None:
        text = f"已连接：{info['name']}  |  写特征UUID：{info['write_uuid']}"
        self.ui.set_connected(True, text)
        self.ui.append_log(
            f"已连接到 {info['name']} [{info['address']}]，写入模式：{info['write_mode']}。"
        )

    def _on_disconnected(self, message: str) -> None:
        self.ui.set_connected(False, "未连接")
        self.ui.append_log(message)

    def _on_smart_done(self, result: dict[str, Any]) -> None:
        display_text = result["payload"].replace("\n", "\\n")
        self.ui.append_tagged_log(
            "AI",
            json.dumps(result["json"], ensure_ascii=False, indent=4),
        )
        self.ui.append_tagged_log("BLE", f"AI 指令发送成功\n{display_text}")
        self.ui.clear_user_text()

    def _on_sent(self, result: dict[str, str]) -> None:
        payload = result["payload"]
        display_text = payload.replace("\n", "\\n")
        if result["context"] == "raw":
            self.ui.append_tagged_log("BLE", f"原始指令发送成功：\n{display_text}")
            self.ui.clear_command()
        else:
            self.ui.append_tagged_log("BLE", f"AI 指令发送成功\n{display_text}")

    def _on_error(self, message: str) -> None:
        self.ui.append_log(message)
        self.signals.ai_state.emit(False)
        self.signals.execute_state.emit(False)
        self.signals.send_state.emit(False)
        if message.startswith("Connect failed") or message.startswith("Disconnect failed"):
            self.ui.set_connected(False, "未连接")
