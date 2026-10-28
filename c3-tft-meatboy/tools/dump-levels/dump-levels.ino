/*
 * Outil temporaire : dump des 100 niveaux generes par c3-tft-meatboy.ino,
 * pour analyse hors-ligne (SVG de structure / level design).
 * Copie VERBATIM du generateur de niveaux (meme RNG ESP32 : randomSeed()
 * bascule sur srand()/rand() cote arduino-esp32, cf WMath.cpp) pour garantir
 * des niveaux identiques a ceux du vrai jeu.
 * A flasher temporairement, capturer le Serial, puis reflasher le vrai jeu.
 */

#define TILE 16
#define COLS 20
#define WROWS 34
#define NUM_LEVELS 100

#define MS 6
struct Saw { bool active; int axis; float fixed, cur, lo, hi, spd, dir, r; };

#define MT 12
enum { KSPIKE, KFALL };
struct Trap { bool active; int kind; int tx, ty; float t, p1, p2; float y, vy; bool st, st2; int trig; };

#define MC 16
struct Crumble { int tx, ty, state; float t; };

char grid[WROWS][COLS + 1];
int  levH;

void clearGrid() {
  for (int r = 0; r < WROWS; r++) { for (int c = 0; c < COLS; c++) grid[r][c] = '.'; grid[r][COLS] = 0; }
}
void putH(int c0, int c1, int r, char ch) {
  if (r < 0 || r >= WROWS) return;
  for (int c = c0; c <= c1; c++) if (c >= 0 && c < COLS) grid[r][c] = ch;
}
void putV(int r0, int r1, int c, char ch) {
  if (c < 0 || c >= COLS) return;
  for (int r = r0; r <= r1; r++) if (r >= 0 && r < WROWS) grid[r][c] = ch;
}

Saw saws[MS];
int nSaws;
void addSaw(int axis, float fx, float lo, float hi, float spd, float r) {
  if (nSaws >= MS) return;
  Saw &s = saws[nSaws++];
  s.active = true; s.axis = axis; s.fixed = fx; s.lo = lo; s.hi = hi;
  s.cur = (lo + hi) / 2; s.spd = spd; s.dir = 1; s.r = r;
}

Trap traps[MT]; int nTraps;
int gTrapPct, gFallPct;
void addTrapSpike(int tx, int ty) {
  if (nTraps >= MT) return;
  Trap &t = traps[nTraps++];
  t.active = true; t.kind = KSPIKE; t.tx = tx; t.ty = ty;
  t.p1 = 0.8; t.p2 = 0.10; t.trig = 30; t.t = 0; t.st = false; t.st2 = false;
}
void addTrapFall(int tx, int ty) {
  if (nTraps >= MT) return;
  Trap &t = traps[nTraps++];
  t.active = true; t.kind = KFALL; t.tx = tx; t.ty = ty;
  t.y = ty * TILE; t.vy = 0; t.trig = 26; t.st = false; t.st2 = false;
}

Crumble crumbs[MC]; int nCrumbs;
int gCrumblePct;

int goalTX, goalTY;
int gpX[4], gpY[4], gpN, gpIdx;
float goalFlash;

int  nStarPos;
int  starTX[3], starTY[3];
bool starGot[3];
void addStar(int tx, int ty) {
  if (nStarPos < 3 && ty >= 0) { starTX[nStarPos] = tx; starTY[nStarPos] = ty; starGot[nStarPos] = false; nStarPos++; }
}
void addStarAbove(int tx, int topRow) {
  if (tx < 0 || tx >= COLS) return;
  int ty = topRow - 2;
  if (ty < 0 || grid[ty][tx] != '.') ty = topRow - 1;
  if (ty < 0 || ty >= WROWS || grid[ty][tx] != '.') return;
  addStar(tx, ty);
}

int aTX[48], aTY[48], nAnc;
void addAnchor(int tx, int ty) { if (nAnc < 48) { aTX[nAnc] = tx; aTY[nAnc] = ty; nAnc++; } }
void sprinkleStars() {
  if (nAnc <= 0) return;
  int idx[3] = { nAnc / 5, nAnc / 2, (nAnc * 4) / 5 };
  for (int k = 0; k < 3; k++) {
    int i = idx[k]; if (i < 0) i = 0; if (i >= nAnc) i = nAnc - 1;
    addStarAbove(aTX[i], aTY[i]);
  }
}

// ================= GENERATEURS (copie verbatim du jeu) =================
void buildChain(int n, int gLo, int gHi, int upMax, int dnMax,
                int wLo, int wHi, int spikePct, int sawPct, int climbBias) {
  float sawSpd = 40.0 + n * 0.7; if (sawSpd > 120) sawSpd = 120;
  int r = 11;
  putH(0, 2, r, '#'); putH(0, 2, r + 1, '#');
  grid[r - 1][1] = 'S';
  addAnchor(1, r);
  int prevR = r, x = 3, gLX = 0, gLR = r, gLW = 3;
  while (x < COLS - 3) {
    int gap = gLo + random(0, gHi - gLo + 1);
    int dy  = random(-upMax, dnMax + 1) - climbBias;
    int nr = prevR + dy; if (nr < 3) nr = 3; if (nr > 12) nr = 12;
    int pw = random(wLo, wHi + 1);
    int nx = x + gap;
    if (nx + pw > COLS - 1) pw = COLS - 1 - nx;
    if (pw < 2) break;
    bool crumble = (gCrumblePct > 0 && (int)random(0, 100) < gCrumblePct);
    putH(nx, nx + pw - 1, nr, crumble ? 'C' : '#');
    addAnchor(nx + pw / 2, nr);
    if (!crumble && pw >= 4 && (int)random(0, 100) < spikePct) {
      int sc = nx + 1 + random(0, pw - 3);
      if ((int)random(0, 100) < gTrapPct) addTrapSpike(sc, nr - 1);
      else grid[nr - 1][sc] = '^';
    }
    if (pw >= 3 && nr > 5 && (int)random(0, 100) < gFallPct)
      addTrapFall(nx + pw / 2, max(1, nr - 6));
    if ((int)random(0, 100) < sawPct) {
      float sxpx = ((x + nx) / 2.0) * TILE + 8;
      int lr = min(prevR, nr) - 2, hr = max(prevR, nr) + 1;
      addSaw(1, sxpx, lr * TILE, hr * TILE, sawSpd, 9);
    } else if (pw >= 4 && nx < COLS - 6 && (int)random(0, 100) < sawPct) {
      addSaw(0, nr * TILE - 10, nx * TILE + 8, (nx + pw) * TILE - 8, sawSpd, 9);
    }
    prevR = nr; x = nx + pw; gLX = nx; gLR = nr; gLW = pw;
  }
  putH(gLX, gLX + gLW - 1, gLR, '#');
  grid[gLR - 1][gLX + gLW / 2] = 'G';
}
void genChain(int n)   { levH = 15; gCrumblePct = n < 10 ? 0 : min(40, 10 + (n - 10) * 2); gTrapPct = n < 15 ? 0 : min(55, (n - 15) * 2); gFallPct = n < 20 ? 0 : min(40, (n - 20) * 2); buildChain(n, 2, min(4, 2 + n / 30), min(2, 1 + n / 50), 3, max(2, 4 - n / 40), 4, min(60, 5 + n / 2), (n <= 6 ? 0 : min(55, (n - 6) * 3)), 0); }
void genStairs(int n)  { levH = 15; gCrumblePct = n < 10 ? 0 : min(38, 8 + (n - 10) * 2); gTrapPct = n < 18 ? 0 : min(50, (n - 18) * 2); gFallPct = n < 22 ? 0 : min(35, (n - 22) * 2); buildChain(n, 2, 3, 2, 2, 3, 4, min(40, n / 2), (n <= 8 ? 0 : min(45, (n - 8) * 3)), 1); }
void genIslands(int n) { levH = 15; gCrumblePct = n < 10 ? 0 : min(35, 8 + (n - 10) * 2); gTrapPct = n < 18 ? 0 : min(50, (n - 18) * 2); gFallPct = n < 24 ? 0 : min(35, (n - 24) * 2); buildChain(n, 3, 4, 1, 2, 2, 2, min(50, n / 2), (n <= 8 ? 0 : min(50, (n - 8) * 3)), 0); }
void genTraps(int n) {
  levH = 15;
  gCrumblePct = n < 10 ? 0 : min(35, 8 + (n - 10) * 2);
  gTrapPct = min(75, 40 + n / 3);
  gFallPct = min(60, 30 + n / 4);
  buildChain(n, 2, 3, 2, 2, 3, 4, 30, min(40, 10 + n), 0);
  if (nAnc >= 4) {
    gpN = 2;
    gpX[0] = aTX[nAnc / 2];      gpY[0] = aTY[nAnc / 2] - 1;
    gpX[1] = aTX[(nAnc * 3) / 4]; gpY[1] = aTY[(nAnc * 3) / 4] - 1;
  }
}
void genChimney(int n) {
  levH = 15;
  int wl = 6 + random(0, 3);
  int wr = wl + 4;
  int goalR = max(2, 8 - n / 15);
  for (int r = goalR + 1; r <= 13; r++) { grid[r][wl] = '#'; grid[r][wr] = '#'; }
  putH(0, COLS - 1, 14, '#');
  int mid = (wl + wr) / 2;
  grid[12][mid] = '#'; grid[11][mid] = 'S';
  putH(wl + 1, wr - 1, 13, '^');
  grid[goalR][mid] = 'G';
  addStar(mid, 9); addStar(wl + 1, goalR + 3); addStar(wr - 1, goalR + 1);
}
void genWell(int n) {
  clearGrid();
  levH = 26;
  int wl = 8, wr = 11;
  int top = 5;
  int bot = levH - 2;
  putV(top, bot, wl, '#');
  putV(top, bot, wr, '#');
  putH(wl, wr, bot, '#');
  grid[bot - 1][9] = 'S';
  putH(wr, 16, top, '#');
  grid[top - 1][15] = 'G';
  addStar(9, bot - 6);
  addStar(10, top + 6);
  addStar(15, top - 2);
}
void genClimb(int n) {
  clearGrid();
  levH = 30;
  int r = levH - 2;
  putH(1, 5, r, '#'); putH(1, 5, r + 1, '#');
  grid[r - 1][3] = 'S';
  addAnchor(3, r);
  int cx = 3;
  int gLX = 3, gLR = r, gLW = 5;
  while (r > 3) {
    r -= 2; if (r < 2) r = 2;
    int pw = 3 + random(0, 2);
    int dx = random(-3, 4);
    int nx = cx + dx - pw / 2;
    if (nx < 1) nx = 1; if (nx + pw > COLS - 1) nx = COLS - 1 - pw;
    putH(nx, nx + pw - 1, r, '#');
    addAnchor(nx + pw / 2, r);
    cx = nx + pw / 2;
    if (n > 25 && random(0, 100) < 22)
      addSaw(0, r * TILE - 10, nx * TILE + 8, (nx + pw) * TILE - 8, 60, 8);
    gLX = nx; gLR = r; gLW = pw;
  }
  grid[gLR - 1][gLX + gLW / 2] = 'G';
  sprinkleStars();
}
void genLevel(int n) {
  randomSeed((uint32_t)n * 2654435761u + 1u);
  nSaws = 0; for (int i = 0; i < MS; i++) saws[i].active = false;
  nTraps = 0; gTrapPct = 0; gFallPct = 0;
  nCrumbs = 0; gCrumblePct = 0;
  gpN = 0; gpIdx = 0; goalFlash = 0;
  nStarPos = 0; nAnc = 0;
  clearGrid(); levH = 15;

  if (n == 0)            genChain(0);
  else if (n % 10 == 3)  genWell(n);
  else if (n % 10 == 7)  genClimb(n);
  else if (n % 10 == 5)  genTraps(n);
  else if (n % 5 == 4)   genChimney(n);
  else {
    int a = n % 3;
    if (a == 0)      genChain(n);
    else if (a == 1) genStairs(n);
    else             genIslands(n);
  }
  if (nStarPos == 0) sprinkleStars();
}

// ================= DUMP =================
const char* kindOf(int n) {
  if (n == 0) return "chain";
  if (n % 10 == 3) return "well";
  if (n % 10 == 7) return "climb";
  if (n % 10 == 5) return "traps";
  if (n % 5 == 4) return "chimney";
  int a = n % 3;
  return a == 0 ? "chain" : (a == 1 ? "stairs" : "islands");
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("BEGIN");
  for (int n = 0; n < NUM_LEVELS; n++) {
    genLevel(n);
    Serial.printf("LEVEL %d %s H%d\n", n, kindOf(n), levH);
    for (int r = 0; r < levH; r++) {
      for (int c = 0; c < COLS; c++) Serial.print(grid[r][c]);
      Serial.print('\n');
    }
    for (int i = 0; i < nSaws; i++)
      Serial.printf("SAW %d %.1f %.1f %.1f\n", saws[i].axis, saws[i].fixed, saws[i].lo, saws[i].hi);
    for (int i = 0; i < nTraps; i++)
      Serial.printf("TRAP %d %d %d\n", traps[i].kind, traps[i].tx, traps[i].ty);
    for (int i = 0; i < nStarPos; i++)
      Serial.printf("STAR %d %d\n", starTX[i], starTY[i]);
    for (int i = 0; i < gpN; i++)
      Serial.printf("DECOY %d %d\n", gpX[i], gpY[i]);
    Serial.println("END");
  }
  Serial.println("ALLDONE");
}
void loop() { delay(1000); }
