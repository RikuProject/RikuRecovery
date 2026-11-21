#include "input.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "log.h"

#define MAX_DEVS 16
#define QCAP 16

struct dev {
    int fd;
    int touch;      /* 1 — тачскрин */
    int mt;         /* 1 — используются ABS_MT_POSITION_* */
    struct input_absinfo ax, ay;
    int down, need_start;
    int cx, cy;     /* текущие «сырые» координаты */
    int sx, sy;     /* координаты начала касания, в пикселях экрана */
};

static struct dev g_dev[MAX_DEVS];
static int g_ndev;
static int g_stdin_mode;
static int g_have_touch;
static int g_sw = 240, g_sh = 240;
static int g_tap_x, g_tap_y;

static enum key_event g_q[QCAP];
static int g_qh, g_qn;

static void push(enum key_event e)
{
    if (g_qn < QCAP)
        g_q[(g_qh + g_qn++) % QCAP] = e;
}

static enum key_event pop(void)
{
    if (g_qn == 0)
        return KEY_NONE;
    enum key_event e = g_q[g_qh];
    g_qh = (g_qh + 1) % QCAP;
    g_qn--;
    return e;
}

#define BITS_LONG (8 * sizeof(unsigned long))
static int test_bit(int bit, const unsigned long *arr)
{
    return (arr[bit / BITS_LONG] >> (bit % BITS_LONG)) & 1;
}

static void probe_touch(struct dev *d)
{
    unsigned long abs[(ABS_MAX + BITS_LONG) / BITS_LONG];
    unsigned long key[(KEY_MAX + BITS_LONG) / BITS_LONG];

    memset(abs, 0, sizeof(abs));
    memset(key, 0, sizeof(key));
    if (ioctl(d->fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) < 0)
        return;
    ioctl(d->fd, EVIOCGBIT(EV_KEY, sizeof(key)), key);

    if (test_bit(ABS_MT_POSITION_X, abs) && test_bit(ABS_MT_POSITION_Y, abs)) {
        d->mt = 1;
        ioctl(d->fd, EVIOCGABS(ABS_MT_POSITION_X), &d->ax);
        ioctl(d->fd, EVIOCGABS(ABS_MT_POSITION_Y), &d->ay);
    } else if (test_bit(ABS_X, abs) && test_bit(ABS_Y, abs) &&
               test_bit(BTN_TOUCH, key)) {
        ioctl(d->fd, EVIOCGABS(ABS_X), &d->ax);
        ioctl(d->fd, EVIOCGABS(ABS_Y), &d->ay);
    } else {
        return;
    }
    if (d->ax.maximum <= d->ax.minimum || d->ay.maximum <= d->ay.minimum)
        return;
    d->touch = 1;
}

int input_init(void)
{
    g_ndev = 0;
    g_stdin_mode = 0;
    g_have_touch = 0;

    const char *force = getenv("RECOVERY_STDIN");
    if (!(force && force[0] == '1')) {
        DIR *dir = opendir("/dev/input");
        if (dir) {
            struct dirent *e;
            while ((e = readdir(dir)) != NULL && g_ndev < MAX_DEVS) {
                if (strncmp(e->d_name, "event", 5) != 0)
                    continue;
                char path[300];
                snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);
                int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (fd < 0)
                    continue;
                struct dev *d = &g_dev[g_ndev++];
                memset(d, 0, sizeof(*d));
                d->fd = fd;
                probe_touch(d);
                if (d->touch)
                    g_have_touch = 1;
                log_msg("input: %s%s", path, d->touch ? " (touch)" : "");
            }
            closedir(dir);
        }
    }

    if (g_ndev == 0) {
        g_stdin_mode = 1;
        g_have_touch = 1; /* в stdin-режиме касания эмулируются командой T */
        log_msg("input: stdin mode (w/s/e/q, Tx,y;)");
        return 1;
    }
    return g_ndev;
}

void input_set_screen(int w, int h)
{
    if (w > 0 && h > 0) {
        g_sw = w;
        g_sh = h;
    }
}

int input_has_touch(void)
{
    return g_have_touch;
}

void input_last_tap(int *x, int *y)
{
    if (x)
        *x = g_tap_x;
    if (y)
        *y = g_tap_y;
}

/* ---------- кнопки ---------- */

static enum key_event map_code(int code)
{
    switch (code) {
    case KEY_VOLUMEUP:
    case KEY_UP:
        return KEY_EV_UP;
    case KEY_VOLUMEDOWN:
    case KEY_DOWN:
        return KEY_EV_DOWN;
    case KEY_POWER:
    case KEY_ENTER:
    case KEY_OK:
        return KEY_EV_SELECT;
    case KEY_BACK:
    case KEY_ESC:
        return KEY_EV_BACK;
    default:
        return KEY_NONE;
    }
}

/* ---------- тач ---------- */

static int env_on(const char *name)
{
    const char *v = getenv(name);
    return v && v[0] == '1';
}

/* Перевод «сырых» координат в пиксели экрана. Для панелей, у которых тач
 * повёрнут или зеркален: RECOVERY_TOUCH_SWAP, RECOVERY_TOUCH_INVX,
 * RECOVERY_TOUCH_INVY = 1. */
static void to_screen(const struct dev *d, int rx, int ry, int *ox, int *oy)
{
    double fx = (double)(rx - d->ax.minimum) / (d->ax.maximum - d->ax.minimum);
    double fy = (double)(ry - d->ay.minimum) / (d->ay.maximum - d->ay.minimum);

    if (env_on("RECOVERY_TOUCH_SWAP")) {
        double t = fx;
        fx = fy;
        fy = t;
    }
    if (env_on("RECOVERY_TOUCH_INVX"))
        fx = 1.0 - fx;
    if (env_on("RECOVERY_TOUCH_INVY"))
        fy = 1.0 - fy;
    if (fx < 0) fx = 0;
    if (fx > 1) fx = 1;
    if (fy < 0) fy = 0;
    if (fy > 1) fy = 1;
    *ox = (int)(fx * (g_sw - 1));
    *oy = (int)(fy * (g_sh - 1));
}

/* Палец поднят: решаем, это касание или свайп. */
static void finish_touch(struct dev *d)
{
    int ex, ey;
    to_screen(d, d->cx, d->cy, &ex, &ey);
    int dx = ex - d->sx, dy = ey - d->sy;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int step = g_sh / 6; /* сдвиг на пункт списка */

    if (step < 12)
        step = 12;

    if (ady >= step && ady > adx) {
        int n = ady / step;
        if (n > 5)
            n = 5;
        /* палец вверх — список едет вверх, то есть выбор вниз */
        for (int i = 0; i < n; i++)
            push(dy < 0 ? KEY_EV_DOWN : KEY_EV_UP);
    } else if (adx >= g_sw / 3 && adx > ady * 2) {
        if (dx > 0)
            push(KEY_EV_BACK); /* свайп вправо — назад */
    } else {
        g_tap_x = ex;
        g_tap_y = ey;
        push(KEY_EV_TAP);
    }
}

static void touch_event(struct dev *d, const struct input_event *ev)
{
    switch (ev->type) {
    case EV_ABS:
        if (ev->code == (d->mt ? ABS_MT_POSITION_X : ABS_X))
            d->cx = ev->value;
        else if (ev->code == (d->mt ? ABS_MT_POSITION_Y : ABS_Y))
            d->cy = ev->value;
        else if (ev->code == ABS_MT_TRACKING_ID) {
            if (ev->value >= 0 && !d->down) {
                d->down = 1;
                d->need_start = 1;
            } else if (ev->value < 0 && d->down) {
                d->down = 0;
                finish_touch(d);
            }
        }
        break;
    case EV_KEY:
        if (ev->code == BTN_TOUCH) {
            if (ev->value == 1 && !d->down) {
                d->down = 1;
                d->need_start = 1;
            } else if (ev->value == 0 && d->down) {
                d->down = 0;
                finish_touch(d);
            }
        }
        break;
    case EV_SYN:
        if (ev->code == SYN_REPORT && d->down && d->need_start) {
            to_screen(d, d->cx, d->cy, &d->sx, &d->sy);
            d->need_start = 0;
        }
        break;
    default:
        break;
    }
}

/* ---------- stdin (для отладки) ---------- */

static enum key_event wait_stdin(int timeout_ms)
{
    struct pollfd p = { .fd = STDIN_FILENO, .events = POLLIN };
    for (;;) {
        int r = poll(&p, 1, timeout_ms);
        if (r <= 0)
            return KEY_NONE;
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == 0)
            return KEY_EV_EOF;
        if (n < 0)
            return KEY_NONE;
        switch (c) {
        case 'w': return KEY_EV_UP;
        case 's': return KEY_EV_DOWN;
        case 'e': return KEY_EV_SELECT;
        case 'q': return KEY_EV_BACK;
        case 'T': {
            char buf[24];
            int i = 0;
            while (i < (int)sizeof(buf) - 1 && read(STDIN_FILENO, &c, 1) == 1 &&
                   c != ';')
                buf[i++] = c;
            buf[i] = '\0';
            int x, y;
            if (sscanf(buf, "%d,%d", &x, &y) == 2) {
                g_tap_x = x;
                g_tap_y = y;
                return KEY_EV_TAP;
            }
            continue;
        }
        default:  continue;
        }
    }
}

/* ---------- основной цикл ---------- */

enum key_event input_wait(int timeout_ms)
{
    enum key_event e = pop();
    if (e != KEY_NONE)
        return e;

    if (g_stdin_mode)
        return wait_stdin(timeout_ms);

    struct pollfd pfds[MAX_DEVS];
    for (int i = 0; i < g_ndev; i++) {
        pfds[i].fd = g_dev[i].fd;
        pfds[i].events = POLLIN;
    }

    for (;;) {
        int r = poll(pfds, g_ndev, timeout_ms);
        if (r <= 0)
            return KEY_NONE;
        for (int i = 0; i < g_ndev; i++) {
            if (!(pfds[i].revents & POLLIN))
                continue;
            struct input_event ev;
            while (read(g_dev[i].fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
                if (g_dev[i].touch) {
                    touch_event(&g_dev[i], &ev);
                } else if (ev.type == EV_KEY && ev.value == 1) {
                    enum key_event k = map_code(ev.code);
                    if (k != KEY_NONE)
                        push(k);
                }
            }
        }
        e = pop();
        if (e != KEY_NONE)
            return e;
    }
}

void input_exit(void)
{
    for (int i = 0; i < g_ndev; i++)
        close(g_dev[i].fd);
    g_ndev = 0;
}
