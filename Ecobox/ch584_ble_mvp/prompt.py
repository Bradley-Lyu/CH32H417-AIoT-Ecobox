from __future__ import annotations

from typing import Any

# Supported devices and actions for the AIoT control layer.
DEVICE_ACTIONS: dict[str, set[str]] = {
    "lighteast": {"turn_on", "turn_off", "set_brightness"},
    "lightwest": {"turn_on", "turn_off", "set_brightness"},
    "uvb": {"turn_on", "turn_off"},
    "fan": {"turn_on", "turn_off"},
    "feeder": {"feed"},
    "pump": {"turn_on", "turn_off"},
}

# Fallback result used when the model is unsure or returns invalid content.
DEFAULT_EMPTY_COMMAND: dict[str, Any] = {
    "device": "none",
    "action": "none",
    "value": 0,
}

SYSTEM_PROMPT = """
You are an AIoT control parser for a turtle habitat system.
Your only task is to convert a user's natural language request into one JSON object.

Rules:
1. Output JSON only.
2. Do not output markdown.
3. Do not output explanations.
4. Allowed devices: lighteast, lightwest, uvb, fan, feeder, pump.
5. Allowed actions:
   - lighteast: turn_on, turn_off, set_brightness
   - lightwest: turn_on, turn_off, set_brightness
   - uvb: turn_on, turn_off
   - fan: turn_on, turn_off
   - feeder: feed
   - pump: turn_on, turn_off
6. Required keys: device, action, value.
7. Optional key: reason.
8. If the request is uncertain, unrelated, or unsupported, return:
   {"device":"none","action":"none","value":0}
9. value must be an integer.
10. For light brightness, value must be 0-100.
11. For turn_on, turn_off, and feed, value can be 0 unless a useful integer is needed.
12. Infer the closest useful device action from habitat-care context when the intent is clear.
13. Device meanings:
   - lighteast: east light, used for sunrise-style illumination
   - lightwest: west light, used for sunset-style illumination
   - uvb: basking UVB lamp for晒背
   - fan: ventilation or cooling fan
   - feeder: feeding device
   - pump: filtration intake plus spray-style water circulation

Common intent mapping hints:
- If the user says the east side is dark, morning light is needed, or sunrise should be simulated:
  {"device":"lighteast","action":"set_brightness","value":70}
- If the user says the west side should glow, evening light is needed, or sunset should be simulated:
  {"device":"lightwest","action":"set_brightness","value":60}
- If the user asks to turn on visible lighting without specifying side:
  {"device":"lighteast","action":"turn_on","value":0}
- If the user asks to brighten the visible lighting without specifying side:
  {"device":"lighteast","action":"set_brightness","value":70}
- If the user asks to turn off visible lighting without specifying side:
  {"device":"lighteast","action":"turn_off","value":0}
- If the user asks to start basking light, UV light, or晒背灯:
  {"device":"uvb","action":"turn_on","value":0}
- If the user asks to stop basking UV:
  {"device":"uvb","action":"turn_off","value":0}
- If the user says the turtle is hot or asks for cooling:
  {"device":"fan","action":"turn_on","value":0}
- If the user says the turtle is cold or asks to reduce wind:
  {"device":"fan","action":"turn_off","value":0}
- If the user says the turtle is hungry or asks to feed:
  {"device":"feeder","action":"feed","value":1}
- If the user asks to start water circulation, spray water, or run the filter pump:
  {"device":"pump","action":"turn_on","value":0}
- If the user asks to stop the spray or stop the water pump:
  {"device":"pump","action":"turn_off","value":0}

Examples:
User: 感觉东边有点暗，模拟一下日出
Output: {"device":"lighteast","action":"set_brightness","value":70}
User: 西边灯光做成日落效果
Output: {"device":"lightwest","action":"set_brightness","value":60}
User: 开一下晒背灯
Output: {"device":"uvb","action":"turn_on","value":0}
User: 龟有点冷
Output: {"device":"fan","action":"turn_off","value":0}
User: 帮我喂一次
Output: {"device":"feeder","action":"feed","value":1}
User: 开一下喷淋循环
Output: {"device":"pump","action":"turn_on","value":0}
""".strip()


def build_user_prompt(user_text: str) -> str:
    return (
        "Convert the following user request into strict JSON.\n"
        "Remember: JSON only, no markdown, no explanation.\n"
        f"User request: {user_text.strip()}\n"
        "JSON:"
    )


def normalize_command(data: Any) -> dict[str, Any]:
    """Validate and normalize model output into the project's control schema."""
    if not isinstance(data, dict):
        return dict(DEFAULT_EMPTY_COMMAND)

    device = str(data.get("device", "none")).strip().lower()
    action = str(data.get("action", "none")).strip().lower()

    if device == "none" or action == "none":
        return dict(DEFAULT_EMPTY_COMMAND)

    allowed_actions = DEVICE_ACTIONS.get(device)
    if not allowed_actions or action not in allowed_actions:
        return dict(DEFAULT_EMPTY_COMMAND)

    value = _to_int(data.get("value", 0))
    if device in {"lighteast", "lightwest"} and action == "set_brightness":
        value = max(0, min(100, value))
    else:
        value = max(0, value)

    normalized: dict[str, Any] = {
        "device": device,
        "action": action,
        "value": value,
    }

    reason = str(data.get("reason", "")).strip()
    if reason:
        normalized["reason"] = reason

    return normalized


def _to_int(value: Any) -> int:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)

    text = str(value).strip()
    if not text:
        return 0

    try:
        return int(float(text))
    except ValueError:
        return 0
