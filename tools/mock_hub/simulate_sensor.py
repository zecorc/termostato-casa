#!/usr/bin/env python3
"""Simulate a room sensor posting readings to the hub (mock or real ESP32)."""

from __future__ import annotations

import argparse
import sys
import time
import urllib.error
import urllib.request
import json


def post_reading(
    url: str,
    sensor_id: str,
    room: str,
    temp_c: float,
    timeout: float = 5.0,
) -> None:
    base = url.rstrip("/")
    endpoint = f"{base}/api/v1/readings"
    payload = json.dumps(
        {
            "sensor_id": sensor_id,
            "room": room,
            "temp_c": round(temp_c, 2),
        }
    ).encode("utf-8")
    req = urllib.request.Request(
        endpoint,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        resp.read()


def ramp_temp(elapsed_s: float, low: float, high: float, period_s: float) -> float:
    """Triangle wave between low and high over period_s."""
    phase = (elapsed_s % period_s) / period_s
    if phase < 0.5:
        t = phase * 2
        return low + (high - low) * t
    t = (phase - 0.5) * 2
    return high - (high - low) * t


def main() -> int:
    parser = argparse.ArgumentParser(description="Simulate sensor POST to termostato hub")
    parser.add_argument(
        "--url",
        default="http://127.0.0.1:8080",
        help="Hub base URL (mock or ESP32 IP)",
    )
    parser.add_argument("--sensor-id", default="test-01")
    parser.add_argument("--room", default="Soggiorno")
    parser.add_argument("--temp", type=float, default=18.0, help="Fixed temperature (without --ramp)")
    parser.add_argument("--interval", type=float, default=30.0, help="Seconds between POSTs")
    parser.add_argument(
        "--ramp",
        action="store_true",
        help="Cycle temperature low→high→low to test hysteresis",
    )
    parser.add_argument("--ramp-low", type=float, default=17.0)
    parser.add_argument("--ramp-high", type=float, default=22.0)
    parser.add_argument("--ramp-period", type=float, default=120.0, help="Full ramp cycle (s)")
    parser.add_argument("--count", type=int, default=0, help="Stop after N posts (0 = forever)")
    args = parser.parse_args()

    start = time.monotonic()
    posts = 0

    print(f"Posting to {args.url} every {args.interval}s as {args.sensor_id} ({args.room})")
    if args.ramp:
        print(f"Ramp: {args.ramp_low}°C ↔ {args.ramp_high}°C over {args.ramp_period}s")

    while True:
        elapsed = time.monotonic() - start
        temp = (
            ramp_temp(elapsed, args.ramp_low, args.ramp_high, args.ramp_period)
            if args.ramp
            else args.temp
        )

        try:
            post_reading(args.url, args.sensor_id, args.room, temp)
            posts += 1
            print(f"[{posts}] temp_c={temp:.1f}")
        except urllib.error.URLError as exc:
            print(f"POST failed: {exc}", file=sys.stderr)
            return 1

        if args.count and posts >= args.count:
            break

        time.sleep(args.interval)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
