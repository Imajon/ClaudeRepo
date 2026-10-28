"""Mini autorouteur 2 couches (grille 0,2 mm, A* 8 directions + vias) pour make_kicad_tab.py.
route(pads, static) -> (pistes, vias, reseaux_en_echec). Coordonnees de conception (mm, Y vers le bas)."""
import heapq, math
import numpy as np
from PIL import Image, ImageDraw
from shapely.geometry import Point, LineString, box
from shapely.ops import unary_union

G, X0, Y0, NX, NY = .2, -1.0, -3.0, 720, 610
CLR, VIA_R, MARGIN, EDGE = .2, .4, .15, .55
ORDER = ["RST", "MOSI", "CS", "DC", "SCLK", "SAUT", "DROITE", "GAUCHE", "IO8", "IO0", "IO20", "IO21", "+3V3", "GND"]
WIDTHS = {"+3V3": (.4, .3, .25), "GND": (.4, .3, .25)}
LAYERS = ("F.Cu", "B.Cu")
DIRS = [(1, 0, 1), (-1, 0, 1), (0, 1, 1), (0, -1, 1), (1, 1, 1.4142), (1, -1, 1.4142), (-1, 1, 1.4142), (-1, -1, 1.4142)]


def geoms(g):
    return list(g.geoms) if hasattr(g, "geoms") else [g]


def raster(g):
    im = Image.new("L", (NX, NY), 0)
    d = ImageDraw.Draw(im)
    for p in sorted((p for p in geoms(g) if not p.is_empty), key=lambda p: -p.area):
        d.polygon([((x - X0) / G, (y - Y0) / G) for x, y in p.exterior.coords], fill=255)
        for h in p.interiors:
            d.polygon([((x - X0) / G, (y - Y0) / G) for x, y in h.coords], fill=0)
    return np.array(im) > 0


def padgeom(p):
    return box(p["x"] - p["w"] / 2, p["y"] - p["h"] / 2, p["x"] + p["w"] / 2, p["y"] + p["h"] / 2) if p["shape"] == "rect" else Point(p["x"], p["y"]).buffer(p["w"] / 2)


_static = {}


def static_maps(st, infl):
    """Obstacles fixes (bord, cuivre des lettres, plaque QR, trous M3, zone du module) pour une garde `infl`, par couche."""
    k = round(infl, 3)
    if k not in _static:
        inside = st["body"].buffer(-(EDGE + infl - CLR))
        common = unary_union([Point(m).buffer(1.6 + infl) for m in st["mount"]])
        front = unary_union([st["copper"].buffer(infl), st["plate"].buffer(infl), common])
        back = unary_union([st["module"].buffer(.05), st.get("back_cu", box(0, 0, 0, 0)).buffer(infl), common])   # back_cu : cuivre fixe au dos (plaque QR)
        out = ~raster(inside)
        _static[k] = [out | raster(front), out | raster(back)]
    return _static[k]


def key(l, i, j):
    return (l * NY + j) * NX + i


def cells_of(geom, layers):
    m = raster(geom)
    js, is_ = np.nonzero(m)
    return {key(l, int(i), int(j)) for l in layers for i, j in zip(is_, js)}


def astar(obs, viaobs, srcs, tgt, tcx, tcy, via_cost=7.0):
    heap = []
    best = {}
    prev = {}
    for s in srcs:
        best[s] = 0.0
        prev[s] = None
        l, r = divmod(s, NY * NX)
        j, i = divmod(r, NX)
        heapq.heappush(heap, (math.hypot(i - tcx, j - tcy) * .2, 0.0, s))
    while heap:
        _, g, s = heapq.heappop(heap)
        if g > best.get(s, 1e18):
            continue
        if s in tgt:
            path = []
            while s is not None:
                path.append(s)
                s = prev[s]
            return path[::-1]
        l, r = divmod(s, NY * NX)
        j, i = divmod(r, NX)
        ob = obs[l]
        for di, dj, c in DIRS:
            ni, nj = i + di, j + dj
            if not (0 <= ni < NX and 0 <= nj < NY):
                continue
            ns = key(l, ni, nj)
            if ob[nj * NX + ni] and ns not in tgt:
                continue
            if di and dj and (ob[j * NX + ni] or ob[nj * NX + i]):
                continue
            ng = g + c
            if ng < best.get(ns, 1e18):
                best[ns] = ng
                prev[ns] = s
                dx, dy = abs(ni - tcx), abs(nj - tcy)
                heapq.heappush(heap, (ng + (max(dx, dy) + .4142 * min(dx, dy)) * .98, ng, ns))
        if not viaobs[j * NX + i]:        # via possible ici (libre sur les deux couches)
            ns = key(1 - l, i, j)
            ng = g + via_cost
            if ng < best.get(ns, 1e18):
                best[ns] = ng
                prev[ns] = s
                heapq.heappush(heap, (ng + math.hypot(i - tcx, j - tcy) * .2, ng, ns))
    return None


def route_net(net, pads, st, items, width):
    infl = CLR + width / 2 + MARGIN
    sf, sb = static_maps(st, infl)
    vf, vb = static_maps(st, CLR + VIA_R + MARGIN - .05 + .0)
    dyn = [[], []]
    dynv = [[], []]
    for it in items:
        if it["net"] == net:
            continue
        for l in (0, 1):
            if it["g"][l] is not None:
                dyn[l].append(it["g"][l].buffer(infl))
                dynv[l].append(it["g"][l].buffer(CLR + VIA_R + MARGIN + .2))   # +.2 : erreur de grille en diagonale
    obs = [(sf | raster(unary_union(dyn[0]))) if dyn[0] else sf, (sb | raster(unary_union(dyn[1]))) if dyn[1] else sb]
    vobs = [(vf | raster(unary_union(dynv[0]))) if dynv[0] else vf, (vb | raster(unary_union(dynv[1]))) if dynv[1] else vb]
    obs_b = [bytes(o.astype(np.uint8).ravel().tobytes()) for o in obs]
    via_b = bytes((vobs[0] | vobs[1] | obs[0] | obs[1]).astype(np.uint8).ravel().tobytes())
    pcells = []
    for p in pads:
        pcells.append(cells_of(padgeom(p).buffer(-.15), (0, 1) if p["tht"] else (0,) if p.get("front") else (1,)))   # CMS : au dos sauf "front"
    tree = set(pcells[0])
    left = list(range(1, len(pads)))
    tr, vi = [], []
    ci = lambda p: ((p["x"] - X0) / G, (p["y"] - Y0) / G)
    connected = [pads[0]]
    while left:
        k = min(left, key=lambda a: min(math.hypot(pads[a]["x"] - c["x"], pads[a]["y"] - c["y"]) for c in connected))
        left.remove(k)
        p = pads[k]
        tcx, tcy = ci(p)
        path = astar(obs_b, via_b, tree, pcells[k], tcx, tcy)
        if path is None:
            return None
        tree |= set(path) | pcells[k]
        connected.append(p)
        pts = []
        for s in path:
            l, r = divmod(s, NY * NX)
            j, i = divmod(r, NX)
            pts.append((l, X0 + i * G, Y0 + j * G))
        pts.append((pts[-1][0], p["x"], p["y"]))                   # puis au centre de la pastille (dernier point deja dedans)
        for a, b in zip(pts, pts[1:]):
            if a[0] != b[0]:
                vi.append((a[1], a[2]))
        # segments : fusion des points alignes sur une meme couche
        seg = [pts[0]]
        for a in pts[1:]:
            if a[0] != seg[-1][0]:
                tr.append(seg)
                seg = [a]
                continue
            if len(seg) >= 2:
                (l0, x0, y0), (l1, x1, y1) = seg[-2], seg[-1]
                if abs((x1 - x0) * (a[2] - y1) - (y1 - y0) * (a[1] - x1)) < 1e-6:
                    seg[-1] = a
                    continue
            seg.append(a)
        tr.append(seg)
    return [(s[i][1], s[i][2], s[i + 1][1], s[i + 1][2], width, LAYERS[s[i][0]], net) for s in tr for i in range(len(s) - 1)], vi


def route(pads, st, skip=(), first=()):
    """skip : reseaux non routes (ex. GND relie par les zones de cuivre) ; first : reseaux routes en premier."""
    byn = {}
    for p in pads:
        if p["net"] and p["net"] not in skip:
            byn.setdefault(p["net"], []).append(p)
    order = [n for n in first if n in byn] + [n for n in ORDER if n in byn and n not in first] + [n for n in byn if n not in ORDER and n not in first]
    for attempt in range(10):
        items = [dict(net=p["net"], g=[padgeom(p), padgeom(p)] if p["tht"] else [padgeom(p), None] if p.get("front") else [None, padgeom(p)]) for p in pads]
        tracks, vias, failed = [], [], []
        for net in order:
            res = None
            for wd in WIDTHS.get(net, (.25,)):
                res = route_net(net, sorted(byn[net], key=lambda p: (p["ref"] != "U1", p["ref"], str(p["num"]))), st, items, wd)
                if res:
                    break
            if not res:
                failed.append(net)
                continue
            t, v = res
            tracks += t
            vias += [(x, y, net) for x, y in v]
            for (x0, y0, x1, y1, wd, ly, nt) in t:
                g = LineString([(x0, y0), (x1, y1)]).buffer(wd / 2)
                items.append(dict(net=net, g=[g, None] if ly == "F.Cu" else [None, g]))
            for x, y in v:
                items.append(dict(net=net, g=[Point(x, y).buffer(VIA_R + .2)] * 2))
        if not failed:
            return tracks, vias, []
        order = failed + [n for n in order if n not in failed]
    return tracks, vias, failed
