#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include "actions.h"
#include "fstab.h"
#include "input.h"
#include "log.h"
#include "menu.h"
#include "strings.h"
#include "sys.h"
#include "ui.h"

/* Ранняя инициализация, когда мы PID 1 (рекавери запущено как /init). */
static void early_init(void)
{
    mkdir("/proc", 0555);
    mkdir("/sys", 0555);
    mkdir("/dev", 0755);
    mkdir("/tmp", 01777);

    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, "mode=0755") != 0)
        mount("tmpfs", "/dev", "tmpfs", 0, "mode=0755");
    mount("tmpfs", "/tmp", "tmpfs", 0, "mode=1777");

    int fd = open("/dev/console", O_RDWR);
    if (fd >= 0) {
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        if (fd > 2)
            close(fd);
    }
}

/* Пауза, чтобы экран загрузки был виден; в dry-run не ждём. */
static void boot_pause(int ms)
{
    if (!sys_dryrun())
        usleep((useconds_t)ms * 1000);
}

int main(int argc, char **argv)
{
    int is_init = (getpid() == 1);
    const char *log_file = getenv("RECOVERY_LOG");
    const char *fstab_file = getenv("RECOVERY_FSTAB");

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lang=en") == 0)
            str_set_lang(LANG_EN);
        else if (strcmp(argv[i], "--lang=ru") == 0)
            str_set_lang(LANG_RU);
    }

    if (is_init)
        early_init();

    log_init(log_file ? log_file : (is_init ? "/tmp/recovery.log" : "recovery.log"));
    log_msg("recovery core start (pid %d)%s", (int)getpid(),
            sys_dryrun() ? " [dry-run]" : "");

    if (!fstab_file)
        fstab_file = "/etc/recovery.fstab";

    /* Графика, если есть framebuffer; иначе консоль. */
    const struct ui_ops *ui = &ui_console;
    const char *want = getenv("RECOVERY_UI");
    if (!(want && strcmp(want, "console") == 0) && ui_fb.init() == 0)
        ui = &ui_fb;
    else if (ui_console.init() != 0) {
        log_msg("ui init failed");
        return 1;
    }
    log_msg("ui: %s", ui->name);
    menu_set_ui(ui);

    ui->splash(tr(S_BOOT_INIT), 10);
    boot_pause(300);

    fstab_load(fstab_file);
    ui->splash(tr(S_BOOT_PARTS), 45);
    boot_pause(300);

    if (input_init() == 0) {
        log_msg("no input");
        return 1;
    }
    {
        int w, h;
        if (ui->size && ui->size(&w, &h) == 0)
            input_set_screen(w, h);
        if (ui->touch_mode)
            ui->touch_mode(input_has_touch());
    }
    ui->splash(tr(S_BOOT_INPUT), 75);
    boot_pause(300);

    ui->splash(tr(S_OK), 100);
    boot_pause(500);

    int r = actions_main_menu();

    input_exit();
    ui->exit();

    /* PID 1 не имеет права завершаться — иначе kernel panic. */
    if (is_init) {
        sys_reboot(NULL);
        for (;;)
            pause();
    }
    return r < 0 ? 0 : r;
}
