"""Heating controller — port of firmware/hub/src/heating_controller.cpp"""

from __future__ import annotations

import time
from dataclasses import dataclass
from enum import Enum
from typing import TYPE_CHECKING, Callable, Optional

if TYPE_CHECKING:
    from readings import ReadingsStore

TERMO_DEFAULT_TARGET_C = 20.0
TERMO_DEFAULT_HYSTERESIS_C = 0.5

# Shorter than firmware (5 min / 3 min) for interactive local testing.
TERMO_MIN_BOILER_ON_S = 10
TERMO_MIN_BOILER_OFF_S = 5


class HeatMode(str, Enum):
    AUTO = "auto"
    MANUAL_ON = "manual_on"
    MANUAL_OFF = "manual_off"


@dataclass
class HeatingSnapshot:
    mode: HeatMode
    target_c: float
    hysteresis_c: float
    current_temp_c: float
    sensor_valid: bool
    heat_demand: bool
    relay_on: bool
    sensor_age_s: float


class HeatingController:
    def __init__(
        self,
        readings: ReadingsStore,
        on_relay_change: Optional[Callable[[bool], None]] = None,
    ) -> None:
        self._readings = readings
        self._on_relay_change = on_relay_change
        self._mode = HeatMode.AUTO
        self._target_c = TERMO_DEFAULT_TARGET_C
        self._hysteresis_c = TERMO_DEFAULT_HYSTERESIS_C
        self._relay_on = False
        self._heat_demand = False
        self._relay_changed_at = time.monotonic()

    def set_mode(self, mode: HeatMode) -> None:
        self._mode = mode

    def set_target(self, target_c: float) -> None:
        if 5.0 <= target_c <= 35.0:
            self._target_c = target_c

    def set_hysteresis(self, hysteresis_c: float) -> None:
        if 0.1 <= hysteresis_c <= 3.0:
            self._hysteresis_c = hysteresis_c

    @property
    def mode(self) -> HeatMode:
        return self._mode

    @property
    def target_c(self) -> float:
        return self._target_c

    @property
    def hysteresis_c(self) -> float:
        return self._hysteresis_c

    @property
    def relay_on(self) -> bool:
        return self._relay_on

    def _apply_relay(self, on: bool, now: float) -> None:
        if on == self._relay_on:
            return
        self._relay_on = on
        self._relay_changed_at = now
        if self._on_relay_change:
            self._on_relay_change(on)

    def update(self, now: Optional[float] = None) -> None:
        now = now if now is not None else time.monotonic()

        if self._mode == HeatMode.MANUAL_ON:
            self._heat_demand = True
            self._apply_relay(True, now)
            return

        if self._mode == HeatMode.MANUAL_OFF:
            self._heat_demand = False
            self._apply_relay(False, now)
            return

        sensor_valid, temp_c, _age = self._readings.get_control(now)
        if not sensor_valid:
            self._heat_demand = False
            self._apply_relay(False, now)
            return

        on_below = self._target_c - self._hysteresis_c
        since_change = now - self._relay_changed_at

        if not self._relay_on:
            self._heat_demand = temp_c < on_below
            if self._heat_demand and since_change >= TERMO_MIN_BOILER_OFF_S:
                self._apply_relay(True, now)
        else:
            self._heat_demand = temp_c < self._target_c
            if not self._heat_demand and since_change >= TERMO_MIN_BOILER_ON_S:
                self._apply_relay(False, now)

    def snapshot(self, now: Optional[float] = None) -> HeatingSnapshot:
        now = now if now is not None else time.monotonic()
        sensor_valid, temp_c, age_s = self._readings.get_control(now)
        return HeatingSnapshot(
            mode=self._mode,
            target_c=self._target_c,
            hysteresis_c=self._hysteresis_c,
            current_temp_c=temp_c,
            sensor_valid=sensor_valid,
            heat_demand=self._heat_demand,
            relay_on=self._relay_on,
            sensor_age_s=age_s,
        )
