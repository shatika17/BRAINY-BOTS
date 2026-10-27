/*
 * KUMBH RAKSHAK - console prototype (C)
 * ---------------------------------------------------------------
 * Modules:
 *  1. Offline map graph (junctions + roads)           -> hard-coded / loadable from file
 *  2. Drone crowd feed (simulated people counts)      -> Green / Yellow / Red per zone
 *  3. Ambulance / pilgrim routing (Dijkstra)          -> avoids crowded zones
 *  4. Nearest emergency service (restroom, water...)  -> works fully offline
 *  5. Kid safety band tracker + geofence + panic
 *  6. SOS queue (stored offline, synced when online)
 *
 * Build:  gcc -Wall -O2 kumbh_rakshak.c -o kumbh_rakshak -lm
 * Run:    ./kumbh_rakshak
 *
 * In the real system:
 *  - crowd counts come from YOLO/OpenCV on the ground station
 *  - band positions come from ESP32 + GPS + LoRa/BLE gateways
 *  - the same logic would run inside the mobile app
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define MAX_NODES   16
#define MAX_EDGES   64
#define MAX_SERV    32
#define MAX_BANDS   20
#define MAX_SOS     50
#define INF         1e18

/* ---------- 1. MAP DATA ---------- */
typedef struct { char name[32]; double x, y; int count; } Node;   /* x,y in metres */
typedef struct { int a, b; double dist; } Edge;

Node  nodes[MAX_NODES];  int nNodes = 0;
Edge  edges[MAX_EDGES];  int nEdges = 0;

int addNode(const char *name, double x, double y) {
    strncpy(nodes[nNodes].name, name, 31);
    nodes[nNodes].x = x; nodes[nNodes].y = y; nodes[nNodes].count = 0;
    return nNodes++;
}
void addEdge(int a, int b) {
    double dx = nodes[a].x - nodes[b].x, dy = nodes[a].y - nodes[b].y;
    edges[nEdges].a = a; edges[nEdges].b = b; edges[nEdges].dist = sqrt(dx*dx + dy*dy);
    nEdges++;
}

/* ---------- 4. SERVICES ---------- */
typedef enum { RESTROOM, WATER, MEDICAL, POLICE, SHELTER } SType;
const char *sTypeName[] = { "Restroom", "Water point", "Medical camp", "Police booth", "Shelter" };
typedef struct { SType type; char name[32]; int node; } Service;
Service services[MAX_SERV]; int nServ = 0;

void addService(SType t, const char *name, int node) {
    services[nServ].type = t; strncpy(services[nServ].name, name, 31);
    services[nServ].node = node; nServ++;
}

void loadMap(void) {
    int gate = addNode("Main Gate",      0,    0);
    int mkt  = addNode("Market Chowk",   300,  100);
    int ghat = addNode("Sangam Ghat",    700,  150);
    int pol  = addNode("Police Junction",450,  400);
    int hosp = addNode("Hospital Camp",  800,  450);
    int brg  = addNode("North Bridge",   900,  250);
    int park = addNode("Parking Zone",   100,  350);
    int shel = addNode("Shelter Area",   600,  600);

    addEdge(gate, mkt);  addEdge(mkt, ghat);  addEdge(mkt, pol);
    addEdge(pol, hosp);  addEdge(ghat, brg);  addEdge(brg, hosp);
    addEdge(gate, park); addEdge(park, pol);  addEdge(pol, shel);
    addEdge(shel, hosp); addEdge(mkt, park);

    addService(RESTROOM, "Toilet Block A",   gate);
    addService(RESTROOM, "Toilet Block B",   pol);
    addService(RESTROOM, "Toilet Block C",   shel);
    addService(WATER,    "Water Tank 1",     mkt);
    addService(WATER,    "Water Tank 2",     ghat);
    addService(MEDICAL,  "Main Hospital",    hosp);
    addService(MEDICAL,  "First Aid Post",   mkt);
    addService(POLICE,   "Police Booth",     pol);
    addService(POLICE,   "Lost & Found",     gate);
    addService(SHELTER,  "Relief Shelter",   shel);
}

/* ---------- 2. CROWD LEVELS (from drone) ---------- */
int online = 1;                       /* 1 = network available, 0 = offline */
int levelOf(int count) { return count < 200 ? 0 : (count < 400 ? 1 : 2); }
const char *levelName[] = { "GREEN ", "YELLOW", "RED   " };

void showCrowdMap(void) {
    printf("\n--- Crowd map (last known state) ---\n");
    for (int i = 0; i < nNodes; i++)
        printf("  [%s] %-16s people: %d\n", levelName[levelOf(nodes[i].count)],
               nodes[i].name, nodes[i].count);
}

void droneUpdate(void) {
    if (!online) {
        printf("Offline: drone data cannot sync. Using last known crowd state.\n");
        return;
    }
    int id, cnt;
    printf("Zone id (0-%d) and people count from drone: ", nNodes - 1);
    if (scanf("%d %d", &id, &cnt) != 2 || id < 0 || id >= nNodes || cnt < 0) {
        printf("Invalid input.\n"); while (getchar() != '\n'); return;
    }
    nodes[id].count = cnt;
    printf("%s -> %s\n", nodes[id].name, levelName[levelOf(cnt)]);
    if (levelOf(cnt) == 2)
        printf("ALERT to control room: %s is overcrowded! Open another path.\n", nodes[id].name);
}

/* ---------- 3. ROUTING (Dijkstra with crowd penalty) ---------- */
/* ambulance=1 uses a smaller penalty (sirens + volunteers clear the lane) */
double edgeCost(Edge *e, int ambulance) {
    int lv = levelOf(nodes[e->a].count);
    int lv2 = levelOf(nodes[e->b].count);
    if (lv2 > lv) lv = lv2;
    double mult[2][3] = { {1, 2, 6}, {1, 1.5, 3} };   /* [pilgrim/ambulance][level] */
    return e->dist * mult[ambulance][lv];
}

double dijkstra(int src, int dst, int ambulance, int prev[], double dist[]) {
    int done[MAX_NODES] = {0};
    for (int i = 0; i < nNodes; i++) { dist[i] = INF; prev[i] = -1; }
    dist[src] = 0;
    for (int it = 0; it < nNodes; it++) {
        int u = -1;
        for (int i = 0; i < nNodes; i++)
            if (!done[i] && (u == -1 || dist[i] < dist[u])) u = i;
        if (u == -1 || dist[u] >= INF) break;
        done[u] = 1;
        for (int k = 0; k < nEdges; k++) {
            int v = -1;
            if (edges[k].a == u) v = edges[k].b;
            else if (edges[k].b == u) v = edges[k].a;
            if (v == -1 || done[v]) continue;
            double c = dist[u] + edgeCost(&edges[k], ambulance);
            if (c < dist[v]) { dist[v] = c; prev[v] = u; }
        }
    }
    return dist[dst];
}

void printPath(int prev[], int dst) {
    int path[MAX_NODES], n = 0;
    for (int v = dst; v != -1; v = prev[v]) path[n++] = v;
    for (int i = n - 1; i >= 0; i--)
        printf("%s%s", nodes[path[i]].name, i ? " -> " : "\n");
}

void routeMenu(void) {
    int s, d, amb;
    for (int i = 0; i < nNodes; i++) printf("  %d = %s\n", i, nodes[i].name);
    printf("From, To, Ambulance(1/0): ");
    if (scanf("%d %d %d", &s, &d, &amb) != 3 || s < 0 || s >= nNodes || d < 0 || d >= nNodes) {
        printf("Invalid input.\n"); while (getchar() != '\n'); return;
    }
    int prev[MAX_NODES]; double dist[MAX_NODES];
    double c = dijkstra(s, d, amb, prev, dist);
    if (c >= INF) { printf("No route found.\n"); return; }
    printf("\n%s route (cost %.0f, crowd-aware):\n  ", amb ? "AMBULANCE" : "Pilgrim", c);
    printPath(prev, d);
    printf("%s\n", online ? "(live crowd data)" : "(offline: using last known crowd state)");
}

/* ---------- 4. NEAREST SERVICE ---------- */
void nearestService(void) {
    int cur, t;
    for (int i = 0; i < nNodes; i++) printf("  %d = %s\n", i, nodes[i].name);
    printf("Your location id: ");
    if (scanf("%d", &cur) != 1 || cur < 0 || cur >= nNodes) {
        printf("Invalid.\n"); while (getchar() != '\n'); return;
    }
    printf("Type: 0 Restroom, 1 Water, 2 Medical, 3 Police, 4 Shelter: ");
    if (scanf("%d", &t) != 1 || t < 0 || t > 4) {
        printf("Invalid.\n"); while (getchar() != '\n'); return;
    }
    int prev[MAX_NODES]; double dist[MAX_NODES];
    dijkstra(cur, cur, 0, prev, dist);
    int best = -1;
    for (int i = 0; i < nServ; i++)
        if (services[i].type == (SType)t &&
            (best == -1 || dist[services[i].node] < dist[services[best].node])) best = i;
    if (best == -1 || dist[services[best].node] >= INF) { printf("None found.\n"); return; }
    int d = services[best].node;
    dijkstra(cur, d, 0, prev, dist);
    printf("\nNearest %s: %s\n  Path: ", sTypeName[t], services[best].name);
    printPath(prev, d);
}

/* ---------- 6. SOS QUEUE ---------- */
typedef struct { int node; char msg[48]; int synced; } SOS;
SOS sosQ[MAX_SOS]; int nSOS = 0;

void sendSOS(int node, const char *msg) {
    if (nSOS >= MAX_SOS) { printf("SOS queue full!\n"); return; }
    sosQ[nSOS].node = node; strncpy(sosQ[nSOS].msg, msg, 47);
    sosQ[nSOS].msg[47] = 0; sosQ[nSOS].synced = 0;
    if (online) {
        sosQ[nSOS].synced = 1;
        printf("SOS SENT to control room: [%s] near %s\n", msg, nodes[node].name);
    } else {
        printf("OFFLINE: SOS saved on phone. Will send when network returns.\n");
    }
    nSOS++;
}

void syncSOS(void) {
    if (!online) return;
    for (int i = 0; i < nSOS; i++)
        if (!sosQ[i].synced) {
            sosQ[i].synced = 1;
            printf("SYNCED queued SOS: [%s] near %s\n", sosQ[i].msg, nodes[sosQ[i].node].name);
        }
}

void sosMenu(void) {
    int loc; char msg[48];
    printf("Your location id: ");
    if (scanf("%d", &loc) != 1 || loc < 0 || loc >= nNodes) {
        printf("Invalid.\n"); while (getchar() != '\n'); return;
    }
    printf("Short message (no spaces, e.g. Medical): ");
    if (scanf("%47s", msg) != 1) return;
    sendSOS(loc, msg);
}

/* ---------- 5. KID SAFETY BAND ---------- */
typedef struct {
    int id; char name[24]; char parent[16];
    double cx, cy, radius;            /* safe zone centre and radius (m) */
    double x, y;                      /* last known position */
    int panic, active;
} Band;
Band bands[MAX_BANDS]; int nBands = 0;

void registerBand(void) {
    if (nBands >= MAX_BANDS) { printf("Band limit reached.\n"); return; }
    Band *b = &bands[nBands];
    b->id = 1000 + nBands;
    printf("Child name (no spaces), parent phone, safe-zone centre x y (m), radius (m): ");
    if (scanf("%23s %15s %lf %lf %lf", b->name, b->parent, &b->cx, &b->cy, &b->radius) != 5) {
        printf("Invalid.\n"); while (getchar() != '\n'); return;
    }
    b->x = b->cx; b->y = b->cy; b->panic = 0; b->active = 1;
    printf("Band %d registered for %s. Parent: %s\n", b->id, b->name, b->parent);
    nBands++;
}

void bandUpdate(void) {
    int id, panic; double x, y;
    printf("Band id, x, y, panic(1/0)  [from LoRa/BLE gateway]: ");
    if (scanf("%d %lf %lf %d", &id, &x, &y, &panic) != 4) {
        printf("Invalid.\n"); while (getchar() != '\n'); return;
    }
    for (int i = 0; i < nBands; i++) if (bands[i].id == id) {
        Band *b = &bands[i]; b->x = x; b->y = y; b->panic = panic;
        double d = hypot(x - b->cx, y - b->cy);
        printf("%s is %.0f m from safe-zone centre.\n", b->name, d);
        if (panic)
            printf("PANIC! Child %s pressed the button. Alert to %s and volunteers.\n", b->name, b->parent);
        if (d > b->radius)
            printf("GEOFENCE ALERT -> %s: your child %s left the safe zone (%.0f m away).\n",
                   b->parent, b->name, d);
        return;
    }
    printf("Band not found.\n");
}

void listBands(void) {
    printf("\n--- Kid bands ---\n");
    if (!nBands) printf("  none\n");
    for (int i = 0; i < nBands; i++) {
        Band *b = &bands[i];
        double d = hypot(b->x - b->cx, b->y - b->cy);
        printf("  #%d %-10s pos(%.0f,%.0f) %s%s\n", b->id, b->name, b->x, b->y,
               d > b->radius ? "OUTSIDE ZONE " : "safe ", b->panic ? "PANIC" : "");
    }
}

/* ---------- MAIN MENU ---------- */
int main(void) {
    loadMap();
    int ch;
    do {
        printf("\n=========== KUMBH RAKSHAK  [%s] ===========\n", online ? "ONLINE" : "OFFLINE");
        printf(" 1 Toggle online/offline\n 2 Drone crowd update\n 3 Show crowd map\n"
               " 4 Route (ambulance/pilgrim)\n 5 Nearest service\n 6 SOS\n"
               " 7 Register kid band\n 8 Band position update\n 9 List bands\n 0 Exit\nChoice: ");
        if (scanf("%d", &ch) != 1) break;
        switch (ch) {
            case 1: online = !online; printf("Now %s.\n", online ? "ONLINE" : "OFFLINE"); syncSOS(); break;
            case 2: droneUpdate(); break;
            case 3: showCrowdMap(); break;
            case 4: routeMenu(); break;
            case 5: nearestService(); break;
            case 6: sosMenu(); break;
            case 7: registerBand(); break;
            case 8: bandUpdate(); break;
            case 9: listBands(); break;
        }
    } while (ch != 0);
    printf("Stay safe!\n");
    return 0;
}
