"""Construccion y escritura de `manifest.json`: nombre, categoria, duracion,
si es bucle, canales y sonoridad LUFS aproximada de cada fichero exportado."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def build_entry(name: str, category: str, is_loop: bool, channels: int, duration_s: float, lufs: float) -> dict[str, Any]:
    return {
        "name": name,
        "category": category,
        "loop": is_loop,
        "channels": channels,
        "duration_s": round(duration_s, 4),
        "lufs_approx": round(lufs, 2),
        "file": f"{category}/{name}.wav",
    }


def write_manifest(entries: list[dict[str, Any]], out_path: Path) -> None:
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "version": 1,
        "sample_rate": 48000,
        "bit_depth": 16,
        "count": len(entries),
        "sounds": sorted(entries, key=lambda e: (e["category"], e["name"])),
    }
    out_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
