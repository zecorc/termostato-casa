"""Sensor readings store — port of firmware/hub/src/readings_store.cpp"""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Optional

TERMO_MAX_SENSORS = 8
TERMO_SENSOR_TIMEOUT_S = 5 * 60
TERMO_SENSOR_ID_MAX = 32
TERMO_ROOM_NAME_MAX = 32


@dataclass
class TermoReading:
    sensor_id: str
    room: str
    temp_c: float
    humidity: Optional[float] = None
    battery_pct: Optional[int] = None
    rssi: Optional[int] = None


@dataclass
class StoredReading:
    reading: TermoReading
    updated_at: float
    valid: bool = True


class ReadingsStore:
    def __init__(self) -> None:
        self._readings: list[Optional[StoredReading]] = [None] * TERMO_MAX_SENSORS

    def clear(self) -> None:
        self._readings = [None] * TERMO_MAX_SENSORS

    def upsert(self, reading: TermoReading, now: Optional[float] = None) -> bool:
        now = now if now is not None else time.monotonic()

        for i, stored in enumerate(self._readings):
            if stored and stored.valid and stored.reading.sensor_id == reading.sensor_id:
                self._readings[i] = StoredReading(reading=reading, updated_at=now)
                return True

        empty_idx = next(
            (i for i, s in enumerate(self._readings) if s is None or not s.valid),
            None,
        )
        if empty_idx is not None:
            idx = empty_idx
        else:
            idx = min(
                range(TERMO_MAX_SENSORS),
                key=lambda i: self._readings[i].updated_at if self._readings[i] else 0,
            )

        self._readings[idx] = StoredReading(reading=reading, updated_at=now)
        return True

    def find(self, sensor_id: str) -> Optional[StoredReading]:
        for stored in self._readings:
            if stored and stored.valid and stored.reading.sensor_id == sensor_id:
                return stored
        return None

    def count(self) -> int:
        return sum(1 for s in self._readings if s and s.valid)

    def all_valid(self) -> list[StoredReading]:
        return [s for s in self._readings if s and s.valid]

    def get_control(
        self, now: Optional[float] = None
    ) -> tuple[bool, float, float]:
        """Returns (valid, temp_c, age_s) using most recently updated sensor."""
        now = now if now is not None else time.monotonic()
        best: Optional[StoredReading] = None

        for stored in self._readings:
            if not stored or not stored.valid:
                continue
            age = now - stored.updated_at
            if age > TERMO_SENSOR_TIMEOUT_S:
                continue
            if best is None or stored.updated_at > best.updated_at:
                best = stored

        if best is None:
            return False, 0.0, 0.0

        return True, best.reading.temp_c, now - best.updated_at
