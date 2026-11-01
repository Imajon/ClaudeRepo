#pragma once
#include <stdint.h>
struct Op { uint8_t op, t; int16_t a, b, c, d, e, f, y0, y1; uint16_t col; };   // un appel de dessin du script
