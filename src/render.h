#ifndef RENDER_H
#define RENDER_H

#include <stddef.h>

#include "flight.h"

#define RENDER_COLS 78
#define RENDER_ROWS 24

/* Side view (1 column = 20 m, 1 row = 25 m). The aircraft sits at column 20; the runway
 * starts at runway_x and is 300 m long. Returns bytes written, or 0 if cap is too small. */
size_t render_frame(const Aircraft *a, const State *s, double runway_x, char *buf, size_t cap);

#endif
