/* Проверка разбора тач-событий без реального устройства: подставляем
 * вместо /dev/input/eventN канал (pipe) и пишем в него input_event.
 * Сборка и запуск:  make test */
#include "../src/input.c"

#include <assert.h>

static int g_pipe[2];

static void ev(int type, int code, int value)
{
    struct input_event e;
    memset(&e, 0, sizeof(e));
    e.type = type;
    e.code = code;
    e.value = value;
    assert(write(g_pipe[1], &e, sizeof(e)) == (ssize_t)sizeof(e));
}

/* Один «кадр» протокола B: положение + SYN. */
static void pos(int x, int y)
{
    ev(EV_ABS, ABS_MT_POSITION_X, x);
    ev(EV_ABS, ABS_MT_POSITION_Y, y);
    ev(EV_SYN, SYN_REPORT, 0);
}

s}tatic void touch_down(int x, int y)
{
    ev(EV_ABS, ABS_MT_TRACKING_ID, 7);
    ev(EV_KEY, BTN_TOUCH, 1);
    pos(x, y);
}

static void touch_up(void)
{
    ev(EV_ABS, ABS_MT_TRACKING_ID, -1);
    ev(EV_KEY, BTN_TOUCH, 0);
    ev(EV_SYN, SYN_REPORT, 0);
}

static void setup(int w, int h, int rawmax)
{
    memset(g_dev, 0, sizeof(g_dev));
    g_qh = g_qn = 0;
    g_ndev = 1;
    g_stdin_mode = 0;
    g_have_touch = 1;
    g_dev[0].fd = g_pipe[0];
    g_dev[0].touch = 1;
    g_dev[0].mt = 1;
    g_dev[0].ax.maximum = rawmax;
    g_dev[0].ay.maximum = rawmax;
    input_set_screen(w, h);
}

int main(void)
{
    int x, y;

    assert(pipe(g_pipe) == 0);
    {
        int fl = fcntl(g_pipe[0], F_GETFL);
        fcntl(g_pipe[0], F_SETFL, fl | O_NONBLOCK);
    }

    /* 1. Касание: сырые 0..1000 -> экран 240x240 */
    setup(240, 240, 1000);
    touch_down(500, 250);
    touch_up();
    assert(input_wait(100) == KEY_EV_TAP);
    input_last_tap(&x, &y);
    assert(x >= 118 && x <= 121);
    assert(y >= 58 && y <= 61);

    /* 2. Свайп пальцем вверх на ~2 шага -> два события «вниз» */
    setup(240, 240, 1000);
    touch_down(500, 800);
    pos(500, 600);
    pos(500, 400);
    touch_up();
    assert(input_wait(100) == KEY_EV_DOWN);
    assert(input_wait(100) == KEY_EV_DOWN);
    assert(input_wait(50) == KEY_NONE);

    /* 3. Свайп вниз -> «вверх» */
    setup(240, 240, 1000);
    touch_down(500, 200);
    pos(500, 450);
    touch_up();
    assert(input_wait(100) == KEY_EV_UP);

    /* 4. Свайп вправо -> «назад» */
    setup(240, 240, 1000);
    touch_down(100, 500);
    pos(800, 520);
    touch_up();
    assert(input_wait(100) == KEY_EV_BACK);

    /* 5. Свайп влево ничего не делает (защита от случайных жестов) */
    setup(240, 240, 1000);
    touch_down(800, 500);
    pos(100, 520);
    touch_up();
    assert(input_wait(50) == KEY_NONE);

    /* 6. Инверсия оси Y */
    setup(240, 240, 1000);
    setenv("RECOVERY_TOUCH_INVY", "1", 1);
    touch_down(500, 0);
    touch_up();
    assert(input_wait(100) == KEY_EV_TAP);
    input_last_tap(&x, &y);
    assert(y >= 238);
    unsetenv("RECOVERY_TOUCH_INVY");

    /* 7. Обмен осей */
    setup(240, 200, 1000);
    setenv("RECOVERY_TOUCH_SWAP", "1", 1);
    touch_down(1000, 0); /* x_raw=1000, y_raw=0 -> после swap: fx=0, fy=1 */
    touch_up();
    assert(input_wait(100) == KEY_EV_TAP);
    input_last_tap(&x, &y);
    assert(x <= 1 && y >= 198);
    unsetenv("RECOVERY_TOUCH_SWAP");

    puts("touch tests: OK");
    return 0;
}
