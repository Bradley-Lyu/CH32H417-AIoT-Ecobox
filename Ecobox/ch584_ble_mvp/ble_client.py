from __future__ import annotations

import asyncio
from typing import Any, Callable, Iterable, Optional

from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
from bleak.uuids import normalize_uuid_str

WCH_UART_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9f"
WCH_UART_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9f"
WCH_UART_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9f"
WCH_IOCHUB_SERVICE_UUID = "0000fff3-0000-1000-8000-00805f9b34fb"
WCH_IOCHUB_RX_UUID = "0000fff5-0000-1000-8000-00805f9b34fb"

DEFAULT_NAME_FILTERS = ("CH584", "CH585", "CH58")
PREFERRED_SERVICE_UUIDS = {
    normalize_uuid_str(WCH_UART_SERVICE_UUID),
    normalize_uuid_str(WCH_IOCHUB_SERVICE_UUID),
}
PREFERRED_WRITE_UUIDS = [
    normalize_uuid_str(WCH_UART_RX_UUID),
    normalize_uuid_str(WCH_IOCHUB_RX_UUID),
]


class BLEClient:
    def __init__(self, disconnect_callback: Optional[Callable[[str], None]] = None) -> None:
        self._disconnect_callback = disconnect_callback
        self._client: Optional[BleakClient] = None
        self._write_uuid: Optional[str] = None
        self._write_with_response = False
        self._last_device_address: Optional[str] = None
        self._last_device_name: str = "Unknown"
        self._preferred_write_uuid: Optional[str] = None
        self._manual_disconnect = False

    async def scan_devices(self, timeout: float = 5.0) -> list[dict[str, Any]]:
        discoveries = await self._discover(timeout)
        devices: list[dict[str, Any]] = []

        for device, advertisement in discoveries:
            name = (getattr(device, "name", None) or getattr(advertisement, "local_name", None) or "Unknown").strip()
            address = getattr(device, "address", "") or ""
            service_uuids = self._collect_service_uuids(device, advertisement)
            matched = self._is_target_device(name, service_uuids)

            devices.append(
                {
                    "name": name,
                    "address": address,
                    "matched": matched,
                    "service_uuids": sorted(service_uuids),
                }
            )

        devices.sort(key=lambda item: (not item["matched"], item["name"].lower(), item["address"].lower()))
        return devices

    async def connect(
        self,
        device: dict[str, Any],
        preferred_write_uuid: Optional[str] = None,
        retries: int = 1,
    ) -> dict[str, str]:
        address = device.get("address", "").strip()
        if not address:
            raise ValueError("Device address is empty.")

        self._last_device_address = address
        self._last_device_name = device.get("name", "Unknown") or "Unknown"
        self._preferred_write_uuid = normalize_uuid_str(preferred_write_uuid) if preferred_write_uuid else None

        await self.disconnect()

        last_error: Optional[Exception] = None
        for attempt in range(retries + 1):
            client = BleakClient(address, disconnected_callback=self._handle_disconnect)
            step = "connect"
            try:
                await client.connect(timeout=10.0)
                step = "discover services / find writable characteristic"
                characteristic = await self._find_write_characteristic(client, self._preferred_write_uuid)
                self._client = client
                self._write_uuid = normalize_uuid_str(characteristic.uuid)
                properties = {prop.lower() for prop in characteristic.properties}
                self._write_with_response = "write" in properties

                return {
                    "name": self._last_device_name,
                    "address": address,
                    "write_uuid": self._write_uuid,
                    "write_mode": "write" if self._write_with_response else "write-without-response",
                }
            except Exception as exc:
                last_error = exc
                try:
                    if client.is_connected:
                        await client.disconnect()
                except Exception:
                    pass
                if attempt < retries:
                    await asyncio.sleep(1.0)

        if last_error is None:
            raise RuntimeError("Connect failed: unknown error")

        error_type = type(last_error).__name__
        error_detail = str(last_error).strip() or repr(last_error)
        raise RuntimeError(f"Connect failed during {step}: {error_type}: {error_detail}") from last_error

    async def send(self, data: str) -> str:
        command = data.strip()
        if not command:
            raise ValueError("Command cannot be empty.")

        payload = f"{command}\n"
        await self._ensure_connected()
        if self._client is None or self._write_uuid is None:
            raise RuntimeError("No active BLE connection.")

        await self._client.write_gatt_char(
            self._write_uuid,
            payload.encode("utf-8"),
            response=self._write_with_response,
        )
        return payload

    async def disconnect(self) -> None:
        client = self._client
        self._client = None
        self._write_uuid = None
        self._write_with_response = False

        if client is None:
            return

        self._manual_disconnect = True
        try:
            if client.is_connected:
                await client.disconnect()
        finally:
            self._manual_disconnect = False

    async def _ensure_connected(self) -> None:
        if self._client is not None and self._client.is_connected and self._write_uuid:
            return

        if not self._last_device_address:
            raise RuntimeError("Device is not connected.")

        await self.connect(
            {"name": self._last_device_name, "address": self._last_device_address},
            preferred_write_uuid=self._preferred_write_uuid,
            retries=1,
        )

    async def _discover(self, timeout: float) -> list[tuple[Any, Any]]:
        try:
            raw = await BleakScanner.discover(timeout=timeout, return_adv=True)
            return [(device, advertisement) for device, advertisement in raw.values()]
        except TypeError:
            devices = await BleakScanner.discover(timeout=timeout)
            return [(device, None) for device in devices]

    def _collect_service_uuids(self, device: Any, advertisement: Any) -> set[str]:
        service_uuids: set[str] = set()

        device_metadata = getattr(device, "metadata", {}) or {}
        for key in ("uuids", "UUIDs"):
            for item in device_metadata.get(key, []) or []:
                service_uuids.add(normalize_uuid_str(item))

        if advertisement is not None:
            for item in getattr(advertisement, "service_uuids", []) or []:
                service_uuids.add(normalize_uuid_str(item))

        return service_uuids

    def _is_target_device(self, name: str, service_uuids: Iterable[str]) -> bool:
        upper_name = name.upper()
        if any(keyword in upper_name for keyword in DEFAULT_NAME_FILTERS):
            return True

        normalized = {normalize_uuid_str(item) for item in service_uuids}
        return bool(normalized & PREFERRED_SERVICE_UUIDS)

    async def _find_write_characteristic(
        self,
        client: BleakClient,
        preferred_write_uuid: Optional[str],
    ) -> BleakGATTCharacteristic:
        services = client.services
        if services is None:
            services = await client.get_services()

        candidates = list(self._iter_characteristics(services))
        if not candidates:
            raise RuntimeError("No GATT characteristics were found.")

        if preferred_write_uuid:
            for characteristic in candidates:
                if normalize_uuid_str(characteristic.uuid) == preferred_write_uuid:
                    return characteristic
            raise RuntimeError(f"Preferred write characteristic not found: {preferred_write_uuid}")

        for uuid_text in PREFERRED_WRITE_UUIDS:
            for characteristic in candidates:
                if normalize_uuid_str(characteristic.uuid) == uuid_text:
                    return characteristic

        write_without_response = [char for char in candidates if "write-without-response" in self._properties(char)]
        if write_without_response:
            return write_without_response[0]

        write_with_response = [char for char in candidates if "write" in self._properties(char)]
        if write_with_response:
            return write_with_response[0]

        raise RuntimeError("No writable GATT characteristic was found.")

    def _iter_characteristics(self, services: Any) -> Iterable[BleakGATTCharacteristic]:
        service_map = getattr(services, "services", None)
        iterable = service_map.values() if isinstance(service_map, dict) else services
        for service in iterable:
            for characteristic in getattr(service, "characteristics", []):
                if self._is_writable(characteristic):
                    yield characteristic

    def _is_writable(self, characteristic: BleakGATTCharacteristic) -> bool:
        properties = self._properties(characteristic)
        return "write" in properties or "write-without-response" in properties

    def _properties(self, characteristic: BleakGATTCharacteristic) -> set[str]:
        return {prop.lower() for prop in getattr(characteristic, "properties", [])}

    def _handle_disconnect(self, _: BleakClient) -> None:
        self._client = None
        self._write_uuid = None
        self._write_with_response = False
        if not self._manual_disconnect and self._disconnect_callback is not None:
            self._disconnect_callback("BLE connection was closed.")
