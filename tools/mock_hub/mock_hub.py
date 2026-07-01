#!/usr/bin/env python3
"""Mock ESP32 hub — same HTTP API as firmware/hub for testing without hardware."""

from __future__ import annotations

import argparse
import threading
import time

from flask import Flask, jsonify, request

from heating import HeatMode, HeatingController
from readings import ReadingsStore, TermoReading

TERMO_API_VERSION = "v1"
MOCK_IP = "127.0.0.1"

INDEX_HTML = """<!DOCTYPE html>
<html lang="it">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Termostato (mock)</title>
  <style>
    body { font-family: system-ui, sans-serif; margin: 1.5rem; background: #111; color: #eee; }
    h1 { margin-top: 0; }
    .card { background: #222; border-radius: 12px; padding: 1rem 1.2rem; margin-bottom: 1rem; }
    .big { font-size: 2.4rem; font-weight: 700; }
    .on { color: #f66; }
    .off { color: #6f6; }
    label { display: block; margin: 0.8rem 0 0.3rem; }
    input, select, button { font-size: 1rem; padding: 0.5rem; border-radius: 8px; border: 1px solid #444; background: #1a1a1a; color: #eee; }
    button { margin-top: 1rem; cursor: pointer; background: #2a5; border: none; color: #fff; }
    .muted { color: #999; font-size: 0.9rem; }
    ul { padding-left: 1.2rem; }
    .badge { display: inline-block; background: #333; padding: 0.2rem 0.5rem; border-radius: 6px; font-size: 0.85rem; margin-bottom: 0.5rem; }
  </style>
</head>
<body>
  <h1>Termostato</h1>
  <div class="badge">Mock hub — test senza ESP32</div>
  <div class="card">
    <div class="muted">Temperatura attuale</div>
    <div class="big" id="temp">--</div>
    <div id="boiler" class="big off">OFF</div>
    <div class="muted" id="meta"></div>
  </div>
  <div class="card">
    <label for="target">Target (&deg;C)</label>
    <input id="target" type="number" step="0.5" min="5" max="35">
    <label for="mode">Modalit&agrave;</label>
    <select id="mode">
      <option value="auto">Auto</option>
      <option value="manual_on">Manuale ON</option>
      <option value="manual_off">Manuale OFF</option>
    </select>
    <button id="save">Salva</button>
  </div>
  <div class="card">
    <div class="muted">Sensori</div>
    <ul id="sensors"></ul>
  </div>
  <script>
    async function refresh() {
      const res = await fetch('/api/v1/status');
      const s = await res.json();
      document.getElementById('temp').textContent =
        s.sensor_valid ? s.current_temp_c.toFixed(1) + ' \\u00b0C' : 'Nessun sensore';
      const boiler = document.getElementById('boiler');
      boiler.textContent = s.relay_on ? 'CALDAIA ON' : 'CALDAIA OFF';
      boiler.className = 'big ' + (s.relay_on ? 'on' : 'off');
      document.getElementById('meta').textContent =
        'Target ' + s.target_c.toFixed(1) + ' \\u00b0C | ' + s.mode + ' | ' + (s.ip || '');
      document.getElementById('target').value = s.target_c;
      document.getElementById('mode').value = s.mode;
      const list = document.getElementById('sensors');
      list.innerHTML = '';
      (s.sensors || []).forEach(function(item) {
        const li = document.createElement('li');
        li.textContent = item.room + ': ' + item.temp_c.toFixed(1) + ' \\u00b0C (' + item.sensor_id + ')';
        list.appendChild(li);
      });
    }
    document.getElementById('save').addEventListener('click', async function() {
      await fetch('/api/v1/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          target_c: parseFloat(document.getElementById('target').value),
          mode: document.getElementById('mode').value
        })
      });
      refresh();
    });
    refresh();
    setInterval(refresh, 5000);
  </script>
</body>
</html>
"""

readings_store = ReadingsStore()
heating = HeatingController(
    readings_store,
    on_relay_change=lambda on: print(f"RELAY {'ON' if on else 'OFF'}"),
)
app = Flask(__name__)
_state_lock = threading.Lock()


def _parse_mode(value: str | None) -> HeatMode | None:
    if not value:
        return None
    try:
        return HeatMode(value)
    except ValueError:
        return None


def _heating_loop(stop: threading.Event, interval: float) -> None:
    while not stop.is_set():
        with _state_lock:
            heating.update()
        stop.wait(interval)


@app.get("/")
def index():
    return INDEX_HTML


@app.get("/api/v1/status")
def status():
    with _state_lock:
        heating.update()
        snap = heating.snapshot()
        now = time.monotonic()
        sensors = []
        for stored in readings_store.all_valid():
            sensors.append(
                {
                    "sensor_id": stored.reading.sensor_id,
                    "room": stored.reading.room,
                    "temp_c": stored.reading.temp_c,
                    "age_s": round(now - stored.updated_at, 1),
                }
            )

    return jsonify(
        {
            "api_version": TERMO_API_VERSION,
            "mode": snap.mode.value,
            "target_c": snap.target_c,
            "hysteresis_c": snap.hysteresis_c,
            "relay_on": snap.relay_on,
            "heat_demand": snap.heat_demand,
            "sensor_valid": snap.sensor_valid,
            "current_temp_c": snap.current_temp_c,
            "sensor_age_s": round(snap.sensor_age_s, 1),
            "ip": MOCK_IP,
            "rssi": 0,
            "sensors": sensors,
            "mock": True,
        }
    )


@app.post("/api/v1/readings")
def readings_post():
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"error": "invalid json"}), 400

    sensor_id = data.get("sensor_id")
    if not sensor_id:
        return jsonify({"error": "sensor_id required"}), 400
    if "temp_c" not in data:
        return jsonify({"error": "temp_c required"}), 400

    reading = TermoReading(
        sensor_id=str(sensor_id)[:31],
        room=str(data.get("room", ""))[:31],
        temp_c=float(data["temp_c"]),
        humidity=data.get("humidity"),
        battery_pct=data.get("battery_pct"),
        rssi=data.get("rssi"),
    )

    with _state_lock:
        readings_store.upsert(reading)
        heating.update()

    return jsonify({"ok": True})


@app.post("/api/v1/config")
def config_post():
    data = request.get_json(silent=True)
    if not data:
        return jsonify({"error": "invalid json"}), 400

    with _state_lock:
        if "target_c" in data:
            heating.set_target(float(data["target_c"]))
        if "hysteresis_c" in data:
            heating.set_hysteresis(float(data["hysteresis_c"]))
        mode = _parse_mode(data.get("mode"))
        if mode is not None:
            heating.set_mode(mode)
        heating.update()

    return jsonify({"ok": True})


@app.errorhandler(404)
def not_found(_err):
    return jsonify({"error": "not found"}), 404


def main() -> None:
    parser = argparse.ArgumentParser(description="Mock termostato hub")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--tick", type=float, default=1.0, help="Heating update interval (s)")
    args = parser.parse_args()

    global MOCK_IP
    MOCK_IP = args.host if args.host != "0.0.0.0" else "127.0.0.1"

    stop = threading.Event()
    thread = threading.Thread(
        target=_heating_loop, args=(stop, args.tick), daemon=True
    )
    thread.start()

    print(f"Mock hub: http://{args.host}:{args.port}/")
    print("API: POST /api/v1/readings, GET /api/v1/status, POST /api/v1/config")
    try:
        app.run(host=args.host, port=args.port, debug=False, threaded=True)
    finally:
        stop.set()
        thread.join(timeout=2)


if __name__ == "__main__":
    main()
