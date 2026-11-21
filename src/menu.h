#ifndef MENU_H
#define MENU_H

#include "ui.h"

#define MENU_MAX 40

enum { MENU_STAY = 0, MENU_EXIT = 1 };

struct menu_item {
    char label[96];
    int (*fn)(void *ctx); /* MENU_STAY — остаться в меню, MENU_EXIT — выйти */
    void *ctx;
};

struct menu {
    const char *title;
    struct menu_item items[MENU_MAX];
    int n;
    void (*refresh)(struct menu *m); /* пересобрать подписи перед отрисовкой */
    int top;                         /* 1 — главное меню: «назад» игнорируется */
};

void menu_set_ui(const struct ui_ops *ui);

void menu_add(struct menu *m, const char *label, int (*fn)(void *), void *ctx);

/* Крутит меню, пока обработчик не вернёт MENU_EXIT или не придёт
 * «назад»/EOF. Возвращает 0 при выходе по «назад», -1 при EOF ввода. */
int menu_run(struct menu *m);

/* Показать сообщение и ждать любую клавишу. */
void menu_message(const char *title, const char *line1, const char *line2);

/* Подтверждение. Возвращает 1 для «да». */
int menu_confirm(const char *question);

/* Показать несколько строк текста. */
void menu_show_lines(const char *title, const char *const *lines, int n);

#endif
