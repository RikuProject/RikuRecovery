#include <stdio.h>
#include <unistd.h>

#include "ui.h"

static int g_tty;

static int con_init(void)
{
    g_tty = isatty(STDOUT_FILENO);
    return 0;
}

static void clear(void)
{
    if (g_tty)
        printf("\033[2J\033[H");
}

static void con_splash(const char *msg, int pct)
{
    clear();
    printf("=== Riku Recovery ===\n%s [%d%%]\n", msg, pct);
    fflush(stdout);
}

static void con_menu(const char *title, const char *const *labels, int n,
                     int sel, const char *hint)
{
    clear();
    printf("=== %s ===\n", title);
    for (int i = 0; i < n; i++)
        printf("%s %s\n", i == sel ? ">" : " ", labels[i]);
    if (hint)
        printf("-- %s --\n", hint);
    fflush(stdout);
}

static void con_lines(const char *title, const char *const *lines, int n)
{
    clear();
    printf("=== %s ===\n", title);
    for (int i = 0; i < n; i++)
        printf("%s\n", lines[i]);
    fflush(stdout);
}

static void con_exit(void)
{
    fflush(stdout);
}

const struct ui_ops ui_console = {
    .name   = "console",
    .init   = con_init,
    .splash = con_splash,
    .menu   = con_menu,
    .lines  = con_lines,
    .exit   = con_exit,
};
