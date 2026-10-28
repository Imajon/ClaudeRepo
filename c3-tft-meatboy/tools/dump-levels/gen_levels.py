#!/usr/bin/env python3
"""Port Python du generateur de niveaux de c3-tft-meatboy.ino (memes fonctions,
memes parametres, meme ordre d'appels random) -> permet de visualiser la
structure des 100 niveaux sans flasher/brancher la console.

NB : le flux aleatoire n'est pas bit-a-bit identique a celui de l'ESP32
(random() y retombe sur srand()/rand() de la libc du toolchain une fois
randomSeed() appele - cf arduino-esp32 WMath.cpp), donc les niveaux exacts
differeront un peu du rendu console. La structure, les parametres et la
progression de difficulte par niveau sont eux fideles au code source.

Usage: python gen_levels.py > levels_dump.log
"""
import random
import sys

TILE = 16
COLS = 20
WROWS = 80
NUM_LEVELS = 100

KSPIKE, KFALL = 0, 1


def rnd(a, b=None):
    """Equivalent de Arduino random(a,b) : entier dans [a,b) ; random(n) == [0,n)."""
    if b is None:
        a, b = 0, a
    if a >= b:
        return a
    return a + random.randrange(0, b - a)


class Level:
    def __init__(self):
        self.grid = [['.'] * COLS for _ in range(WROWS)]
        self.levH = 15
        self.saws = []      # (axis, fixed, lo, hi)
        self.traps = []     # (kind, tx, ty)
        self.crumbles = 0
        self.gCrumblePct = 0
        self.gTrapPct = 0
        self.gFallPct = 0
        self.stars = []     # (tx, ty)
        self.anchors = []   # (tx, ty)
        self.gp = []        # positions du drapeau (leurres puis reelle)

    def clear_grid(self):
        self.grid = [['.'] * COLS for _ in range(WROWS)]

    def putH(self, c0, c1, r, ch):
        if r < 0 or r >= WROWS:
            return
        for c in range(c0, c1 + 1):
            if 0 <= c < COLS:
                self.grid[r][c] = ch

    def putV(self, r0, r1, c, ch):
        if c < 0 or c >= COLS:
            return
        for r in range(r0, r1 + 1):
            if 0 <= r < WROWS:
                self.grid[r][c] = ch

    def add_saw(self, axis, fx, lo, hi, spd, r):
        if len(self.saws) >= 12:
            return
        self.saws.append((axis, fx, lo, hi))

    def add_trap_spike(self, tx, ty, hidden=False):
        if len(self.traps) >= 32:
            return
        self.traps.append((KSPIKE, tx, ty, hidden))

    def add_trap_fall(self, tx, ty, hidden=False):
        if len(self.traps) >= 32:
            return
        self.traps.append((KFALL, tx, ty, hidden))

    def add_star(self, tx, ty):
        if len(self.stars) < 3 and ty >= 0:
            self.stars.append((tx, ty))

    def add_star_above(self, tx, top_row):
        if tx < 0 or tx >= COLS:
            return
        ty = top_row - 2
        if ty < 0 or self.grid[ty][tx] != '.':
            ty = top_row - 1
        if ty < 0 or ty >= WROWS or self.grid[ty][tx] != '.':
            return
        self.add_star(tx, ty)

    def add_anchor(self, tx, ty):
        if len(self.anchors) < 48:
            self.anchors.append((tx, ty))

    def sprinkle_stars(self):
        n = len(self.anchors)
        if n <= 0:
            return
        idx = [n // 5, n // 2, (n * 4) // 5]
        for i in idx:
            i = max(0, min(i, n - 1))
            tx, ty = self.anchors[i]
            self.add_star_above(tx, ty)

    # ================= GENERATEURS (port du .ino) =================
    def build_chain(self, n, gLo, gHi, upMax, dnMax, wLo, wHi, spikePct, sawPct, climbBias,
                     hiddenHazards=False, holePct=0):
        sawSpd = min(40.0 + n * 0.7, 120.0)
        r = 11
        self.putH(0, 2, r, '#'); self.putH(0, 2, r + 1, '#')
        self.grid[r - 1][1] = 'S'
        self.add_anchor(1, r)
        prevR, x, gLX, gLR, gLW = r, 3, 0, r, 3
        while x < COLS - 3:
            gap = gLo + rnd(0, gHi - gLo + 1)
            dy = rnd(-upMax, dnMax + 1) - climbBias
            nr = prevR + dy
            nr = max(3, min(12, nr))
            pw = rnd(wLo, wHi + 1)
            nx = x + gap
            if nx + pw > COLS - 1:
                pw = COLS - 1 - nx
            if pw < 2:
                break
            crumble = (not hiddenHazards) and self.gCrumblePct > 0 and rnd(0, 100) < self.gCrumblePct
            self.putH(nx, nx + pw - 1, nr, 'C' if crumble else '#')
            self.add_anchor(nx + pw // 2, nr)
            if not crumble and pw >= 4 and rnd(0, 100) < spikePct:
                sc = nx + 1 + rnd(0, pw - 3)
                if rnd(0, 100) < self.gTrapPct:
                    self.add_trap_spike(sc, nr - 1, hiddenHazards)
                else:
                    self.grid[nr - 1][sc] = 'X' if hiddenHazards else '^'
            if pw >= 3 and nr > 5 and rnd(0, 100) < self.gFallPct:
                self.add_trap_fall(nx + pw // 2, max(1, nr - 6), hiddenHazards)
            if hiddenHazards and holePct > 0 and pw >= 3 and rnd(0, 100) < holePct:
                hc = nx + 1 + rnd(0, pw - 2)
                self.grid[nr][hc] = 'H'
            if (not hiddenHazards) and rnd(0, 100) < sawPct:
                sxpx = ((x + nx) / 2.0) * TILE + 8
                lr, hr = min(prevR, nr) - 2, max(prevR, nr) + 1
                self.add_saw(1, sxpx, lr * TILE, hr * TILE, sawSpd, 9)
            elif (not hiddenHazards) and pw >= 4 and nx < COLS - 6 and rnd(0, 100) < sawPct:
                self.add_saw(0, nr * TILE - 10, nx * TILE + 8, (nx + pw) * TILE - 8, sawSpd, 9)
            prevR, x, gLX, gLR, gLW = nr, nx + pw, nx, nr, pw
        self.putH(gLX, gLX + gLW - 1, gLR, '#')
        self.grid[gLR - 1][gLX + gLW // 2] = 'G'

    def gen_chain(self, n):
        self.levH = 15
        self.gCrumblePct = 0 if n < 10 else min(40, 10 + (n - 10) * 2)
        self.gTrapPct = 0 if n < 15 else min(55, (n - 15) * 2)
        self.gFallPct = 0 if n < 20 else min(40, (n - 20) * 2)
        self.build_chain(n, 2, min(4, 2 + n // 30), min(2, 1 + n // 50), 3,
                          max(2, 4 - n // 40), 4, min(60, 5 + n // 2),
                          0 if n <= 6 else min(55, (n - 6) * 3), 0)

    def gen_stairs(self, n):
        self.levH = 15
        self.gCrumblePct = 0 if n < 10 else min(38, 8 + (n - 10) * 2)
        self.gTrapPct = 0 if n < 18 else min(50, (n - 18) * 2)
        self.gFallPct = 0 if n < 22 else min(35, (n - 22) * 2)
        self.build_chain(n, 2, 3, 2, 2, 3, 4, min(40, n // 2),
                          0 if n <= 8 else min(45, (n - 8) * 3), 1)

    def gen_islands(self, n):
        self.levH = 15
        self.gCrumblePct = 0 if n < 10 else min(35, 8 + (n - 10) * 2)
        self.gTrapPct = 0 if n < 18 else min(50, (n - 18) * 2)
        self.gFallPct = 0 if n < 24 else min(35, (n - 24) * 2)
        self.build_chain(n, 3, 4, 1, 2, 2, 2, min(50, n // 2),
                          0 if n <= 8 else min(50, (n - 8) * 3), 0)

    def gen_traps(self, n):
        self.levH = 15
        self.gCrumblePct = 0 if n < 10 else min(35, 8 + (n - 10) * 2)
        self.gTrapPct = min(75, 40 + n // 3)
        self.gFallPct = min(60, 30 + n // 4)
        self.build_chain(n, 2, 3, 2, 2, 3, 4, 30, min(40, 10 + n), 0)
        n_anc = len(self.anchors)
        if n_anc >= 4:
            a, b = self.anchors[n_anc // 2], self.anchors[(n_anc * 3) // 4]
            self.gp = [(a[0], a[1] - 1), (b[0], b[1] - 1)]

    # ============== dizaine 31-40 : briques a pousser / tours fantomes (miroir du .ino) ==============
    def _putHL(self, x0, x1, r, ch, mir):
        for x in range(x0, x1 + 1):
            if 0 <= x < COLS and 0 <= r < WROWS:
                self.grid[r][COLS - 1 - x if mir else x] = ch

    def gen_push(self, n, pos):
        self.clear_grid()
        k = pos // 2
        stages = 1 if k < 2 else (2 if k < 4 else 3)
        SP = 8
        self.levH = 15 + SP * (stages - 1)
        f0 = self.levH - 2
        for st in range(stages):
            mir = (st % 2 == 1)
            mx = (lambda x: COLS - 1 - x) if mir else (lambda x: x)
            f = f0 - SP * st
            W = 6 if (k == 0 or (k == 2 and st == 0)) else 7
            nb = W - 5
            a = 6 if st == 0 else 9
            x0 = 0 if st == 0 else 4
            self._putHL(x0, 19, f, '#', mir)
            if st == 0:
                self._putHL(0, 19, f + 1, '#', mir)
            else:
                self._putHL(a, a + W - 1, f + 1, '#', mir)
            self._putHL(a, a + W - 1, f, '^', mir)
            for i in range(nb):
                self.grid[f - 1][mx(a - 3 + i)] = 'B'
            if st == 0:
                self.grid[f - 1][mx(1)] = 'S'
            land = a + W
            if k >= 2 and 19 - land >= 5:
                self.add_trap_spike(mx(land + 2), f - 1, True)
            if k >= 4 and 19 - land >= 5:
                self.add_trap_fall(mx(land + 1), f - 6, True)
            if st < stages - 1:
                self._putHL(18, 19, f - 3, '#', mir)
                self._putHL(16, 17, f - 6, '#', mir)
            else:
                self.grid[f - 1][mx(18)] = 'G'

    def gen_phantom_tower(self, n, pos):
        self.clear_grid()
        k = pos // 2
        tiers = 2 + k // 2
        SP = 7
        self.levH = max(15, 8 + SP * (tiers - 1))
        base0 = self.levH - 3
        gapMax = 2 + (1 if k >= 2 else 0)
        spikePct, holePct, fallPct = 30 + k * 8, 10 + k * 6, 20 + k * 8
        self.gTrapPct = 50
        for t in range(tiers):
            mir = (t % 2 == 1)
            mx = (lambda x: COLS - 1 - x) if mir else (lambda x: x)
            base = base0 - SP * t
            if t == 0:
                self._putHL(0, 2, base, '#', mir); self.grid[base - 1][mx(1)] = 'S'; e = 2
            else:
                self._putHL(4, 6, base, '#', mir); e = 6
            while e < 12:
                gap = 2 + rnd(0, gapMax - 1)
                w = 2 + rnd(0, 3)
                x = e + 1 + gap
                if x + w - 1 > 14:
                    w = 14 - x + 1
                if w < 2:
                    x, w = 13, 2
                r = base + rnd(-1, 2)
                if r > self.levH - 2:
                    r = self.levH - 2
                self._putHL(x, x + w - 1, r, '#', mir)
                if w >= 3 and rnd(0, 100) < spikePct:
                    sc = x + 1 + rnd(0, w - 2)
                    if rnd(0, 100) < self.gTrapPct:
                        self.add_trap_spike(mx(sc), r - 1, True)
                    else:
                        self.grid[r - 1][mx(sc)] = 'X'
                elif w >= 3 and rnd(0, 100) < holePct:
                    self.grid[r][mx(x + 1 + rnd(0, w - 2))] = 'H'
                if rnd(0, 100) < fallPct:
                    self.add_trap_fall(mx(x + w // 2), max(0, r - 5), True)
                e = x + w - 1
            self._putHL(16, 19, base, '#', mir)
            if t < tiers - 1:
                self._putHL(18, 19, base - 2, '#', mir)
                self._putHL(16, 17, base - 5, '#', mir)
            else:
                self.grid[base - 1][mx(18)] = 'G'

    def gen_phantom(self, n):
        self.levH = 15
        pos = n % 10
        hidden = pos >= 2
        sinuosity = min(3, 1 + pos // 3)
        gap_max = min(4, 2 + pos // 4)
        self.gCrumblePct = 0
        self.gTrapPct = 60 if hidden else 15
        self.gFallPct = 45 if hidden else 10
        spike_pct = (35 + pos * 3) if hidden else 15
        hole_pct = (15 + pos * 3) if hidden else 0
        self.build_chain(n, 2, gap_max, sinuosity, sinuosity, 3, 5, spike_pct, 0, 0, hidden, hole_pct)

    def gen_chimney(self, n):
        self.levH = 15
        wl = 6 + rnd(0, 3)
        wr = wl + 4
        goalR = max(2, 8 - n // 15)
        for r in range(goalR + 1, 14):
            self.grid[r][wl] = '#'; self.grid[r][wr] = '#'
        self.putH(0, COLS - 1, 14, '#')
        mid = (wl + wr) // 2
        self.grid[12][mid] = '#'; self.grid[11][mid] = 'S'
        self.putH(wl + 1, wr - 1, 13, '^')
        self.grid[goalR][mid] = 'G'
        self.add_star(mid, 9); self.add_star(wl + 1, goalR + 3); self.add_star(wr - 1, goalR + 1)

    def gen_well(self, n):
        self.clear_grid()
        self.levH = 26
        wl, wr, top = 8, 11, 5
        bot = self.levH - 2
        self.putV(top, bot, wl, '#'); self.putV(top, bot, wr, '#')
        self.putH(wl, wr, bot, '#')
        self.grid[bot - 1][9] = 'S'
        self.putH(wr, 16, top, '#')
        self.grid[top - 1][15] = 'G'
        self.add_star(9, bot - 6); self.add_star(10, top + 6); self.add_star(15, top - 2)

    def gen_climb(self, n):
        self.clear_grid()
        self.levH = 30
        r = self.levH - 2
        self.putH(1, 5, r, '#'); self.putH(1, 5, r + 1, '#')
        self.grid[r - 1][3] = 'S'
        self.add_anchor(3, r)
        cx = 3
        gLX, gLR, gLW = 3, r, 5
        while r > 3:
            r -= 2
            r = max(r, 2)
            pw = 3 + rnd(0, 2)
            dx = rnd(-3, 4)
            nx = cx + dx - pw // 2
            nx = max(nx, 1)
            if nx + pw > COLS - 1:
                nx = COLS - 1 - pw
            self.putH(nx, nx + pw - 1, r, '#')
            self.add_anchor(nx + pw // 2, r)
            cx = nx + pw // 2
            if n > 25 and rnd(0, 100) < 22:
                self.add_saw(0, r * TILE - 10, nx * TILE + 8, (nx + pw) * TILE - 8, 60, 8)
            gLX, gLR, gLW = nx, r, pw
        self.grid[gLR - 1][gLX + gLW // 2] = 'G'
        self.sprinkle_stars()

    # ============== dizaine 11-20 : colonnes empilees (miroir de genColumns) ==============
    def gen_columns(self, n, pos):
        self.clear_grid()
        seg_h = 7
        N = pos + 1
        self.levH = max(15, N * seg_h + 6)
        F = self.levH - 2
        for k in range(N):
            R = (k % 2 == 1)
            xo, xi = (18, 14) if R else (1, 5)
            T = F - seg_h
            if k == 0:
                self.putH(1, 5, F, '#'); self.grid[F - 1][3] = 'S'
            else:
                self.putH(12 if R else 1, 18 if R else 7, F, '#')
            self.putV(T - 2, F - 1, xo, '#')
            self.putV(T, F - 1 if k == 0 else F - 3, xi, '#')
            self.putH(12 if R else 5, 14 if R else 7, T, '#')
            ir = T - 1 - (1 if (pos >= 5 and k % 2 == 1) else 0)
            self.putH(9, 10, ir, '#')
            if pos >= 7 and k % 2 == 0:
                self.grid[ir][10 if R else 9] = '.'
            if k == N - 1:
                self.putH(2 if R else 13, 6 if R else 17, T, '#')
                self.grid[T - 1][3 if R else 16] = 'G'
            if N >= 3:
                if k in (0, (N - 1) // 2, N - 1):
                    self.add_star((xo + xi) // 2, T + 2)
            else:
                self.add_star((xo + xi) // 2, T + 2)
                if k == N - 1:
                    self.add_star(10 if R else 9, ir - 2); self.add_star(5 if R else 14, T - 2)
            gap_x = 8 if R else 11
            if pos >= 5 or k % 2 == 0:
                self.add_saw(1, gap_x * TILE + 8, (ir - 3) * TILE, (ir + 1) * TILE, 45 + pos * 6, 9)
            if pos >= 3 or k % 2 == 0:
                self.add_trap_fall(9 if R else 10, max(0, ir - 5))
            if k % 2 == pos % 2 or pos == 6:
                self.add_trap_spike(12 if R else 7, T - 1)
            if k > 0 and pos >= 4 and (pos < 7 or k % 3 == 1):
                self.add_trap_spike(17 if R else 2, F - 1)
            F = T

    # ============== dizaine 21-30 : briques fragiles (rouge) ==============
    def gen_fragile_well(self, n, pos):
        self.clear_grid()
        self.levH = 24
        wl, wr, top = 8, 11, 5
        bot = self.levH - 2
        self.putV(top, bot, wl, '#'); self.putV(top, bot, wr, '#')
        self.putH(wl, wr, bot, '#')
        self.grid[bot - 1][9] = 'S'
        fp, exit_len = frag_pct(pos), 7
        for i in range(exit_len):
            c = wr + i
            if c >= COLS:
                break
            self.grid[top][c] = 'C' if rnd(0, 100) < fp else '#'
        ge = min(wr + exit_len - 1, COLS - 1)
        self.grid[top - 1][ge] = 'G'
        self.add_star(9, bot - 6); self.add_star(15, top - 2)

    def gen_fragile_traps(self, n, pos):
        self.levH = 15
        self.gCrumblePct = frag_pct(pos)
        self.gTrapPct = min(70, 35 + pos * 3)
        self.gFallPct = min(55, 25 + pos * 3)
        self.build_chain(n, 2, 3, 2, 2, 3, 4, 30, min(35, 8 + pos), 0)
        n_anc = len(self.anchors)
        if n_anc >= 4:
            a, b = self.anchors[n_anc // 2], self.anchors[(n_anc * 3) // 4]
            self.gp = [(a[0], a[1] - 1), (b[0], b[1] - 1)]

    def gen_fragile_stairs(self, n, pos):
        self.levH = 15
        self.gCrumblePct = frag_pct(pos)
        self.gTrapPct = min(45, 10 + pos * 3)
        self.gFallPct = min(30, pos * 3)
        self.build_chain(n, 2, 3, 2, 2, 3, 4, min(35, pos * 3), 0, 1)

    # ============== dizaine 41-50 : cumul final ==============
    def gen_hard_phantom(self, n, pos):
        self.levH = 15
        self.gCrumblePct = 0
        self.gTrapPct = 65
        self.gFallPct = 50
        spike_pct = 45 + pos * 2
        hole_pct = 25 + pos * 2
        self.build_chain(n, 3, 4, 3, 3, 2, 4, spike_pct, 0, 0, True, hole_pct)

    def gen_hard_fragile(self, n, pos):
        self.levH = 15
        self.gCrumblePct = min(85, 55 + pos * 3)
        self.gTrapPct = min(80, 50 + pos * 3)
        self.gFallPct = min(65, 35 + pos * 3)
        self.build_chain(n, 2, 3, 2, 2, 2, 3, 40, min(45, 15 + pos * 3), 0)

    def gen_hard_chain_well(self, n, pos):
        self.clear_grid()
        self.levH = 26
        wl, wr, top = 8, 11, 4
        bot = self.levH - 2
        self.putV(top, bot, wl, '#'); self.putV(top, bot, wr, '#')
        self.putH(wl, wr, bot, '#')
        self.grid[bot - 1][9] = 'S'
        self.grid[(top + bot) // 2][9] = '^'
        exit_len = 6
        for i in range(exit_len):
            c = wr + i
            if c >= COLS:
                break
            self.grid[top][c] = 'C' if rnd(0, 100) < 60 else '#'
        hole_c = min(wr + exit_len // 2, COLS - 1)
        ge = min(wr + exit_len - 1, COLS - 1)
        self.grid[top][hole_c] = 'H'
        self.grid[top - 1][ge] = 'G'
        self.add_star(9, bot - 6); self.add_star(9, top + 6); self.add_star(ge, top - 2)



def fill_stars(grid, levH, stars, traps):
    """Miroir de fillStars() : complete jusqu'a 3 etoiles (modifie stars en place)."""
    def ok(c, r):
        if r < 1 or r >= levH - 1 or grid[r][c] != '.' or grid[r - 1][c] != '.':
            return False
        if grid[r + 1][c] not in '#C':
            return False
        if any(abs(sx - c) < 2 and abs(sy - r) < 2 for sx, sy in stars):
            return False
        for dr in range(-1, 2):
            for dc in range(-1, 2):
                rr, cc = r + dr, c + dc
                if 0 <= rr < levH and 0 <= cc < COLS and grid[rr][cc] in 'SG':
                    return False
        if any(t[1] == c and abs(t[2] - r) <= 1 for t in traps):
            return False
        return True
    order = [(c, r) for r in range(levH - 2, 0, -1) for c in range(COLS)]
    n_cand = sum(1 for c, r in order if ok(c, r))
    need = 3 - len(stars)
    for k in range(need):
        if len(stars) >= 3 or n_cand == 0:
            break
        target = (k + 1) * n_cand // (need + 1)
        placed = False
        for pas in range(2):
            idx = 0
            for c, r in order:
                if not ok(c, r):
                    continue
                if pas == 0:
                    idx += 1
                    if idx - 1 < target:
                        continue
                stars.append((c, r)); placed = True
                break
            if placed:
                break

def frag_pct(pos):
    return min(85, 20 + pos * 7)


def kind_of(n):
    pos = n % 10
    if n < 20:
        return "columns"
    if n < 30:
        if pos in (3, 8):
            return "fragile_well"
        if pos in (5, 9):
            return "fragile_traps"
        return "fragile_stairs"
    if n // 10 == 3:
        return "push" if pos % 2 == 0 else "phantom_tower"
    if n < 50:
        if pos in (8, 9):
            return "hard_chainwell"
        return "hard_phantom" if pos % 2 == 0 else "hard_fragile"
    if n == 50:
        return "chain"
    if n % 10 == 3:
        return "well"
    if n % 10 == 7:
        return "climb"
    if n % 10 == 5:
        return "traps"
    if n % 5 == 4:
        return "chimney"
    a = n % 3
    return "chain" if a == 0 else ("stairs" if a == 1 else "islands")


def gen_level(n):
    random.seed(n * 2654435761 + 1)
    lvl = Level()
    kind = kind_of(n)
    pos = n % 10
    if kind == "columns":
        lvl.gen_columns(n, pos)
    elif kind == "fragile_well":
        lvl.gen_fragile_well(n, pos)
    elif kind == "fragile_traps":
        lvl.gen_fragile_traps(n, pos)
    elif kind == "fragile_stairs":
        lvl.gen_fragile_stairs(n, pos)
    elif kind == "push":
        lvl.gen_push(n, pos)
    elif kind == "phantom_tower":
        lvl.gen_phantom_tower(n, pos)
    elif kind == "phantom":
        lvl.gen_phantom(n)
    elif kind == "hard_chainwell":
        lvl.gen_hard_chain_well(n, pos)
    elif kind == "hard_phantom":
        lvl.gen_hard_phantom(n, pos)
    elif kind == "hard_fragile":
        lvl.gen_hard_fragile(n, pos)
    elif kind == "well":
        lvl.gen_well(n)
    elif kind == "climb":
        lvl.gen_climb(n)
    elif kind == "traps":
        lvl.gen_traps(n)
    elif kind == "chimney":
        lvl.gen_chimney(n)
    elif kind == "stairs":
        lvl.gen_stairs(n)
    elif kind == "islands":
        lvl.gen_islands(n)
    else:
        lvl.gen_chain(n)
    if not lvl.stars:
        lvl.sprinkle_stars()
    fill_stars(lvl.grid, lvl.levH, lvl.stars, lvl.traps)
    return lvl, kind


def main():
    out = sys.stdout
    out.write("BEGIN\n")
    for n in range(NUM_LEVELS):
        lvl, kind = gen_level(n)
        out.write(f"LEVEL {n} {kind} H{lvl.levH}\n")
        for r in range(lvl.levH):
            out.write("".join(lvl.grid[r]) + "\n")
        for (axis, fixed, lo, hi) in lvl.saws:
            out.write(f"SAW {axis} {fixed:.1f} {lo:.1f} {hi:.1f}\n")
        for (kind_, tx, ty, hidden) in lvl.traps:
            out.write(f"TRAP {kind_} {tx} {ty} {int(hidden)}\n")
        for (tx, ty) in lvl.stars:
            out.write(f"STAR {tx} {ty}\n")
        for (tx, ty) in lvl.gp:
            out.write(f"DECOY {tx} {ty}\n")
        out.write("END\n")
    out.write("ALLDONE\n")


if __name__ == "__main__":
    main()
