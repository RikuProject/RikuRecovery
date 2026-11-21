#include "menu.h"

#include <string.h>

#include "input.h"
#include "strings.h"

static const struct ui_ops *g_ui;

static const char *hint_text(void)
{
    return input_has_touch() ? tr(S_HINT_TOUCH) : tr(S_HINT);
}

/* Что под пальцем на последнем нарисованном кадре. */
static int tap_hit(void)
{
    int x, y;
    input_last_tap(&x, &y);
    return g_ui->hit ? g_ui->hit(x, y) : UI_HIT_NONE;
}

void menu_set_ui(const struct ui_ops *ui)
{
    g_ui = ui;
}

void menu_add(struct menu *m, const char *label, int (*fn)(void *), void *ctx)
{
    if (m->n >= MENU_MAX)
        return;
    struct menu_item *it = &m->items[m->n++];
    strncpy(it->label, label, sizeof(it->label) - 1);
    it->label[sizeof(it->label) - 1] = '\0';
    it->fn = fn;
    it->ctx = ctx;
}

int menu_run(struct menu *m)
{
    int sel = 0;

    for (;;) {
        if (m->refresh)
            m->refresh(m);
        if (sel >= m->n)
            sel = m->n ? m->n - 1 : 0;

        const char *labels[MENU_MAX];
        for (int i = 0; i < m->n; i++)
            labels[i] = m->items[i].label;
        g_ui->menu(m->title, labels, m->n, sel, hint_text());

        enum key_event ev = input_wait(-1);

        if (ev == KEY_EV_TAP) {
            int h = tap_hit();
            if (h >= 0 && h < m->n) {
                sel = h;
                ev = KEY_EV_SELECT;
            } else if (h == UI_HIT_BACK || h == UI_HIT_HOME) {
                ev = KEY_EV_BACK;
            } else {
                continue;
            }
        }

        switch (ev) {
        case KEY_EV_UP:
            sel = (sel + m->n - 1) % m->n;
            break;
        case KEY_EV_DOWN:
            sel = (sel + 1) % m->n;
            break;
        case KEY_EV_SELECT:
            if (m->n == 0)
                break;
            if (!m->items[sel].fn) /* пункт «Назад» */
                return 0;
            if (m->items[sel].fn(m->items[sel].ctx) == MENU_EXIT)
                return 0;
            break;
        case KEY_EV_BACK:
            if (m->top)
                break; /* из главного меню назад не выходим */
            return 0;
        case KEY_EV_EOF:
            return -1;
        default:
            break;
        }
    }
}

void menu_show_lines(const char *title, const char *const *lines, int n)
{
    g_ui->lines(title, lines, n);
    while (input_wait(-1) == KEY_NONE)
        ;
}

void menu_message(const char *title, const char *line1, const char *line2)
{
    const char *lines[2];
    int n = 0;
    if (line1)
        lines[n++] = line1;
    if (line2)
        lines[n++] = line2;
    menu_show_lines(title, lines, n);
}

int menu_confirm(const char *question)
{
    const char *labels[2] = { tr(S_NO), tr(S_YES) };
    int sel = 0;

    for (;;) {
        g_ui->menu(question, labels, 2, sel, hint_text());
        enum key_event ev = input_wait(-1);

        if (ev == KEY_EV_TAP) {
            int h = tap_hit();
            if (h == 0 || h == 1)
                return h == 1;
            if (h == UI_HIT_BACK || h == UI_HIT_HOME)
                return 0;
            continue;
        }
        switch (ev) {
        case KEY_EV_UP:
        case KEY_EV_DOWN:
            sel = !sel;
            break;
        case KEY_EV_SELECT:
            return sel == 1;
        case KEY_EV_BACK:
        case KEY_EV_EOF:
            return 0;
        default:
            break;
        }
    }
}
