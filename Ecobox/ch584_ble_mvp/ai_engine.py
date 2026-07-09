from __future__ import annotations

import asyncio
import json
from typing import Any
from urllib import error, request

from prompt import DEFAULT_EMPTY_COMMAND, SYSTEM_PROMPT, build_user_prompt, normalize_command


class AIEngine:
    """Talks to a local Ollama model and returns validated JSON commands."""

    def __init__(
        self,
        base_url: str = "http://localhost:11434",
        model: str = "qwen2.5:3b",
        timeout: float = 30.0,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.model = model
        self.timeout = timeout

    async def analyze_text(self, user_text: str) -> dict[str, Any]:
        text = user_text.strip()
        if not text:
            raise ValueError("Natural language input cannot be empty.")

        raw_result = await asyncio.to_thread(self._request_ollama, text)
        return normalize_command(raw_result)

    def _request_ollama(self, user_text: str) -> dict[str, Any]:
        payload = {
            "model": self.model,
            "system": SYSTEM_PROMPT,
            "prompt": build_user_prompt(user_text),
            "stream": False,
            "format": "json",
            "options": {
                "temperature": 0.2,
            },
        }

        url = f"{self.base_url}/api/generate"
        data = json.dumps(payload).encode("utf-8")
        http_request = request.Request(
            url,
            data=data,
            headers={"Content-Type": "application/json"},
            method="POST",
        )

        try:
            with request.urlopen(http_request, timeout=self.timeout) as response:
                response_body = response.read().decode("utf-8")
        except error.URLError as exc:
            raise RuntimeError(
                "Failed to reach Ollama. Please confirm Ollama is running at "
                f"{self.base_url} and model {self.model} is installed."
            ) from exc

        try:
            response_json = json.loads(response_body)
        except json.JSONDecodeError as exc:
            raise RuntimeError(f"Ollama returned invalid JSON: {response_body}") from exc

        if response_json.get("error"):
            raise RuntimeError(f"Ollama error: {response_json['error']}")

        model_text = str(response_json.get("response", "")).strip()
        if not model_text:
            return dict(DEFAULT_EMPTY_COMMAND)

        parsed = self._parse_model_json(model_text)
        if not isinstance(parsed, dict):
            return dict(DEFAULT_EMPTY_COMMAND)
        return parsed

    def _parse_model_json(self, model_text: str) -> dict[str, Any]:
        try:
            return json.loads(model_text)
        except json.JSONDecodeError:
            # Some local models may still wrap JSON with stray text. Extract the
            # outermost object defensively so the GUI stays usable.
            start = model_text.find("{")
            end = model_text.rfind("}")
            if start == -1 or end == -1 or end <= start:
                raise RuntimeError(f"Model did not return valid JSON: {model_text}")

            try:
                return json.loads(model_text[start : end + 1])
            except json.JSONDecodeError as exc:
                raise RuntimeError(f"Model returned malformed JSON: {model_text}") from exc
