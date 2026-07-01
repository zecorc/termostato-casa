# Mock hub — test senza ESP32

Simula l'hub ESP32 sul PC con la stessa API HTTP del firmware (`firmware/hub/src/web_server.cpp`).

## Requisiti

- Python 3.10+

## Avvio rapido

```bash
cd tools/mock_hub
pip install -r requirements.txt
python mock_hub.py
```

Apri nel browser: **http://127.0.0.1:8080/**

In un secondo terminale, simula un sensore:

```bash
cd tools/mock_hub
python simulate_sensor.py --temp 18.0
```

## Endpoint

| Path | Metodo | Descrizione |
|------|--------|-------------|
| `/` | GET | Web UI |
| `/api/v1/status` | GET | Stato hub, sensori, relè |
| `/api/v1/readings` | POST | Riceve temperatura da sensore |
| `/api/v1/config` | POST | Target, isteresi, modalità |

## Esempi curl

```bash
# Stato
curl http://127.0.0.1:8080/api/v1/status

# Invia lettura (sensore finto)
curl -X POST http://127.0.0.1:8080/api/v1/readings \
  -H "Content-Type: application/json" \
  -d "{\"sensor_id\":\"test-01\",\"room\":\"Soggiorno\",\"temp_c\":18.0}"

# Cambia target e modalità
curl -X POST http://127.0.0.1:8080/api/v1/config \
  -H "Content-Type: application/json" \
  -d "{\"target_c\":20.0,\"mode\":\"auto\"}"
```

## Sensore simulato

```bash
# Temperatura fissa ogni 30 s
python simulate_sensor.py --url http://127.0.0.1:8080 --temp 18.0 --interval 30

# Ramp 17→22°C per testare isteresi (relè ON/OFF in console)
python simulate_sensor.py --ramp --ramp-low 17 --ramp-high 22 --interval 5

# Una sola lettura
python simulate_sensor.py --temp 19.5 --count 1
```

## Differenze rispetto al firmware ESP32

| Aspetto | Firmware | Mock |
|---------|----------|------|
| Tempi min ON/OFF caldaia | 5 min / 3 min | 10 s / 5 s |
| WiFi / mDNS | Sì | No (localhost) |
| Display TFT | Sì | No |
| Campo `mock` in status | assente | `"mock": true` |

Logica isteresi, fail-safe (timeout sensore 5 min) e API JSON sono allineati al firmware.

## Opzioni mock hub

```bash
python mock_hub.py --host 0.0.0.0 --port 8080
```

`--host 0.0.0.0` espone sulla LAN (utile per testare il futuro firmware sensore verso il PC).

## File

- `readings.py` — registro sensori (port di `readings_store.cpp`)
- `heating.py` — relè e isteresi (port di `heating_controller.cpp`)
- `mock_hub.py` — server Flask
- `simulate_sensor.py` — client sensore di test
