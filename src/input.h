#ifndef INPUT_H
#define INPUT_H

enum key_event {
    KEY_NONE = 0,
    KEY_EV_UP,
    KEY_EV_DOWN,
    KEY_EV_SELECT,
    KEY_EV_BACK,
    KEY_EV_TAP, /* касание: координаты через input_last_tap() */
    KEY_EV_EOF,
};

/* Открывает все /dev/input/event*. Если их нет или RECOVERY_STDIN=1,
 * ввод идёт из stdin — для отладки на хосте:
 *   w/s/e/q — вверх/вниз/выбор/назад,
 *   Tx,y;   — касание в точке (x,y) экрана, например T120,80;
 * Возвращает число источников ввода. */
int input_init(void);

/* Размер экрана в пикселях — для перевода координат тачскрина. */
void input_set_screen(int w, int h);

/* 1, если найден тачскрин (или включён режим stdin с касаниями). */
int input_has_touch(void);

/* Координаты последнего касания в пикселях экрана. */
void input_last_tap(int *x, int *y);

/* Ждёт событие до timeout_ms (-1 — бесконечно). */
enum key_event input_wait(int timeout_ms);

void input_exit(void);

#endif
