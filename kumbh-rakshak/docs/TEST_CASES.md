# Test Cases

## A. Automated (C prototype)
Run `bash tests/run_tests.sh`. Expected: `Passed: 10  Failed: 0`.

| ID | Scenario | Expected |
|---|---|---|
| T1 | Set Market Chowk to 500 people | Control-room alert for the red zone |
| T2 | Ambulance route, Main Gate to Hospital Camp, no crowd | Main Gate > Market Chowk > Police Junction > Hospital Camp |
| T3 | Same route after Market Chowk turns red | Route changes to go through Parking Zone |
| T4 | Nearest restroom from Main Gate | Toilet Block A |
| T5 | Band 50 m from a 100 m safe zone | Distance shown, no alert |
| T6 | Band 150 m from a 100 m safe zone | Geofence alert |
| T7 | Band sends panic = 1 | Panic alert |
| T8 | SOS while offline | Saved on phone |
| T9 | Network returns | Queued SOS is sent |
| T10 | Drone/crowd update while offline | Update blocked, last known data used |

## B. Manual (web app and band)
| ID | Steps | Expected |
|---|---|---|
| M1 | Open the app, switch to airplane mode, reload | App opens; map, services, routes work |
| M2 | Crowd map: change a zone above 400 | Zone turns red; ambulance route updates |
| M3 | Hold SEND SOS for 1.5 seconds | SMS opens with location link; offline press is queued |
| M4 | Nearest help: choose Medical | Nearest point and distance shown; route drawn |
| M5 | Kid tab: simulate wandering past the radius | Red alert and vibration |
| M6 | Press the band button | Full-screen panic alert in the app |
| M7 | Tap Ring the band | Band buzzer beeps |
| M8 | Switch the language to Hindi or Kannada | Main labels change |
| M9 | Turn the band off | After 10 s the app shows band out of range |

## C. Sample data
See `data/sample_zones.json`, `data/sample_services.json` and `data/sample_band_messages.txt`.
