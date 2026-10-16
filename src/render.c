#include "render.h"

#include <math.h>

#define M_PER_COL 20.0
#define M_PER_ROW 25.0
#define PLANE_COL 20
#define RUNWAY_LENGTH 300.0

size_t render_frame(const Aircraft *a, const State *s, double runway_x, char *buf, size_t cap)
{
    (void)a;
    if (cap < (size_t)(RENDER_COLS + 1) * RENDER_ROWS + 1) return 0;
    char grid[RENDER_ROWS][RENDER_COLS];
    for (int r = 0; r < RENDER_ROWS; r++)
        for (int c = 0; c < RENDER_COLS; c++) grid[r][c] = ' ';

    /* Row RENDER_ROWS-1 is the ground line (altitude 0). */
    int ground = RENDER_ROWS - 1;
    for (int c = 0; c < RENDER_COLS; c++) {
        double x = s->x + (c - PLANE_COL) * M_PER_COL;
        int on_runway = x >= runway_x && x <= runway_x + RUNWAY_LENGTH;
        grid[ground][c] = on_runway ? '=' : (fmod(floor(x / M_PER_COL), 25.0) == 0.0 ? '|' : '_');
    }
    int row = ground - 1 - (int)lround(s->h / M_PER_ROW);
    if (row < 0) row = 0;
    if (row > ground - 1) row = ground - 1;
    double slope = s->gamma;
    grid[row][PLANE_COL] = slope < -0.12 ? '\\' : slope > 0.12 ? '/' : '>';

    size_t used = 0;
    for (int r = 0; r < RENDER_ROWS; r++) {
        for (int c = 0; c < RENDER_COLS; c++) buf[used++] = grid[r][c];
        buf[used++] = '\n';
    }
    buf[used] = '\0';
    return used;
}
