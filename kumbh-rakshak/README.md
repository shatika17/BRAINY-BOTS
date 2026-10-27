# Kumbh Rakshak

**Offline emergency response app with a smart child safety band for Kumbh Mela.**
Team BRAINY_BOTS | Hack for Social Cause (HSC)

Theme: Disaster Mitigation, Kumbh Mela & Health Service Delivery

## The problem
Millions of pilgrims gather in a small area. Crowds build up, children get separated, ambulances get stuck on crowded roads, and mobile networks jam so online apps fail.

## The solution
| Feature | What it does |
|---|---|
| Crowd map | Zones shown as green / yellow / red (volunteer-updated) |
| Ambulance route | Dijkstra shortest path with extra cost on crowded roads |
| Nearest help | Closest toilet, water, medical camp, police booth, shelter |
| SOS | Location sent by SMS, queued on the phone when offline |
| Kid safety band | ESP32 Bluetooth band: safe-zone alert, panic button, ring the band |
| Offline-first | Installable web app (PWA) that works without internet |

## Repository structure
```
kumbh-rakshak/
├── app/                 Web app (index.html) + sw.js, manifest.json, icons
├── firmware/kumbh_band/ ESP32 band firmware (Arduino C++)
├── prototype-c/         C console prototype of the core logic
├── data/                Sample zones, services and band messages
├── docs/                ARCHITECTURE.md, TEST_CASES.md
├── tests/               run_tests.sh (automated tests for the C prototype)
├── .env.example         Optional settings (no secrets needed)
└── LICENSE              MIT
```

## Setup

### 1. Web app (main prototype)
1. Open `app/index.html` in Chrome. For Bluetooth, GPS and offline install, host it over HTTPS
   (for example drag the `app` folder onto Netlify Drop, or enable GitHub Pages).
2. On Android Chrome, open the link, tap the three dots, then **Add to Home screen**.
3. Test offline: open the app once online, turn on airplane mode, reopen it.

### 2. Kid band (ESP32)
1. Install Arduino IDE and the **ESP32** board package. Select **ESP32 Dev Module**.
2. Open `firmware/kumbh_band/kumbh_band.ino`, connect the board and click Upload.
3. Wiring: button on GPIO 4 to GND, active buzzer on GPIO 5 to GND, LED on GPIO 2.
   Optional GPS NEO-6M: TX to GPIO 16, RX to GPIO 17, VCC to 3V3, GND to GND.
4. Open Serial Monitor at 115200 baud. Type `w` to simulate the child wandering, `r` to reset.
5. In the app, open the **Kid** tab and tap **Connect Real Band (BLE)** (Android Chrome only).

### 3. C console prototype (routing, geofence, SOS queue logic)
```bash
cd prototype-c
gcc -Wall -O2 kumbh_rakshak.c -o kumbh_rakshak -lm
./kumbh_rakshak
```

## Environment variables
None required. No API keys or secrets are used. See `.env.example` for the optional
settings (family contact number, meeting-point coordinates) that you edit in the code.

## Run the tests
```bash
bash tests/run_tests.sh
```
See `docs/TEST_CASES.md` for the full list, including manual app and band tests.

## BLE protocol (band to app)
- Service `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- Notify (band to app) `6e400003-b5a3-f393-e0a9-e50e24dcca9e`: text `x,y,battery,panic` (metres east and north of the meeting point, battery 0-100, panic 0 or 1)
- Write (app to band) `6e400002-b5a3-f393-e0a9-e50e24dcca9e`: text `RING` makes the band beep

## Honest scope
Crowd levels are volunteer-entered or simulated in the demo. The band runs in a simulator mode
unless GPS is attached. A camera or drone crowd counter, LoRa and regional languages are planned next steps.

## License
MIT, see `LICENSE`.
