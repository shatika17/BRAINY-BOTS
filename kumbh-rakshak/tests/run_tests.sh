#!/usr/bin/env bash
# Automated tests for the C prototype (routing, crowd levels, services, geofence, SOS queue).
cd "$(dirname "$0")/.." || exit 1
gcc -Wall -O2 prototype-c/kumbh_rakshak.c -o /tmp/kr_test -lm || { echo "BUILD FAILED"; exit 1; }
pass=0; fail=0
check() {  # name, input, expected text
  out=$(printf "$2" | /tmp/kr_test 2>&1)
  if echo "$out" | grep -qF "$3"; then echo "PASS: $1"; pass=$((pass+1)); else echo "FAIL: $1 (expected: $3)"; fail=$((fail+1)); fi
}
check "T1 red zone raises control-room alert"        "2\n1 500\n0\n"                         "ALERT to control room"
check "T2 normal ambulance route uses Market Chowk"  "4\n0 4 1\n0\n"                         "Main Gate -> Market Chowk -> Police Junction -> Hospital Camp"
check "T3 ambulance route avoids red Market Chowk"   "2\n1 500\n4\n0 4 1\n0\n"               "Main Gate -> Parking Zone -> Police Junction -> Hospital Camp"
check "T4 nearest restroom from Main Gate"           "5\n0\n0\n0\n"                          "Toilet Block A"
check "T5 band inside safe zone: no alert"           "7\nRiya 9999999999 0 0 100\n8\n1000 50 0 0\n0\n" "50 m from safe-zone centre"
check "T6 geofence alert outside safe zone"          "7\nRiya 9999999999 0 0 100\n8\n1000 150 0 0\n0\n" "GEOFENCE ALERT"
check "T7 panic button alert"                        "7\nRiya 9999999999 0 0 100\n8\n1000 10 0 1\n0\n"  "PANIC!"
check "T8 SOS saved while offline"                   "1\n6\n2\nMedical\n0\n"                 "OFFLINE: SOS saved"
check "T9 queued SOS synced when back online"        "1\n6\n2\nMedical\n1\n0\n"              "SYNCED queued SOS"
check "T10 drone update blocked while offline"       "1\n2\n0\n"                             "Offline: drone data cannot sync"
echo "Passed: $pass  Failed: $fail"
[ "$fail" -eq 0 ]
