#include "mascot.h"

#include <string.h>

/* Левая половина (16 колонок), правая получается зеркалом.
 * o контур   f рыжий мех   d тень меха   w светлая мордочка
 * p розовое ухо   e глаз   h блик   n нос   . прозрачно */
static const char *const left_half[MASCOT_H] = {
    "...oo...........",
    "..ofdo..........",
    "..offdo.........",
    ".ofpffdo........",
    ".ofppffdo.......",
    ".ofppfffdooooooo",
    ".ofpffffffffffff",
    "offfffffffffffff",
    "offfffffffffffff",
    "offfeeeeffffffff",
    "offfehheffffffff",
    "offfeeeeffffffff",
    "offffeefffffffff",
    "offfffffwwwwwwww",
    "offffwwwwwwwwwww",
    "ofdfwwwwwwwwwnnn",
    "ofdwwwwwwwwwwwnn",
    "oddwwwwwwwwwwwwo",
    "ooddwwwwwwwwoowo",
    ".oooddwwwwwwwwwo",
    "..oooodwwwwwwwwo",
    "....oooooooooooo",
};

static uint32_t color_of(char c)
{
    switch (c) {
    case 'o': return 0xFF3A2418;
    case 'f': return 0xFFF28C28;
    case 'd': return 0xFFC85F17;
    case 'w': return 0xFFFFF1DE;
    case 'p': return 0xFFF6A6B5;
    case 'e': return 0xFF1C1C2E;
    case 'h': return 0xFFFFFFFF;
    case 'n': return 0xFF2A1A1A;
    default:  return 0;
    }
}

uint32_t mascot_pixel(int x, int y)
{
    if (x < 0 || y < 0 || x >= MASCOT_W || y >= MASCOT_H)
        return 0;
    int half = MASCOT_W / 2;
    int col = x < half ? x : (MASCOT_W - 1 - x);
    return color_of(left_half[y][col]);
}
