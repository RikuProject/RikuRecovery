#ifndef MASCOT_GUARD_H
#define MASCOT_GUARD_H

#include <stdint.h>

/* Оригинальный маскот — лисёнок Рику. Нарисован пиксель-артом в коде,
 * левая половина зеркалится, поэтому правки делаются в одном месте. */
#define MASCOT_W 32
#define MASCOT_H 22

/* Цвет пикселя (0xAARRGGBB) или 0, если прозрачный. */
uint32_t mascot_pixel(int x, int y);

#endif
