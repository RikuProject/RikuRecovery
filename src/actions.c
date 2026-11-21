#include "actions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fstab.h"
#include "log.h"
#include "menu.h"
#include "strings.h"
#include "sys.h"

/* ---------- простые действия ---------- */

static int act_reboot_system(void *ctx)
{
    (void)ctx;
    sys_reboot(NULL);
    return MENU_EXIT;
}

static int act_reboot_bootloader(void *ctx)
{
    (void)ctx;
    sys_reboot("bootloader");
    return MENU_EXIT;
}

static int act_poweroff(void *ctx)
{
    (void)ctx;
    sys_reboot("poweroff");
    return MENU_EXIT;
}

static int act_shell(void *ctx)
{
    (void)ctx;
    static const char *const shells[] = {
        "/system/bin/sh", "/sbin/sh", "/bin/sh", NULL
    };

    if (sys_dryrun()) {
        menu_message(tr(S_SHELL), "dry-run", NULL);
        return MENU_STAY;
    }

    const char *sh = NULL;
    for (int i = 0; shells[i]; i++) {
        if (access(shells[i], X_OK) == 0) {
            sh = shells[i];
            break;
        }
    }
    if (!sh) {
        menu_message(tr(S_SHELL), tr(S_FAIL), "no shell");
        return MENU_STAY;
    }

    pid_t pid = fork();
    if (pid == 0) {
        execl(sh, sh, (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        int st;
        waitpid(pid, &st, 0);
    }
    return MENU_STAY;
}

static int act_show_log(void *ctx)
{
    (void)ctx;
    enum { KEEP = 12, LEN = 160 };
    static char buf[KEEP][LEN];
    const char *lines[KEEP];
    int n = 0, total = 0;

    FILE *f = fopen(log_path(), "r");
    if (f) {
        char line[LEN];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            strcpy(buf[total % KEEP], line);
            total++;
        }
        fclose(f);
    }
    int first = total > KEEP ? total - KEEP : 0;
    for (int i = first; i < total; i++)
        lines[n++] = buf[i % KEEP];
    menu_show_lines(tr(S_SHOW_LOG), lines, n);
    return MENU_STAY;
}

static int act_install_zip(void *ctx)
{
    (void)ctx;
    /* TODO: разбор ZIP (updater-script / update-binary или свой формат),
     * проверка подписи, источник: /sdcard, USB, sideload. */
    menu_message(tr(S_INSTALL_ZIP), tr(S_NOT_IMPLEMENTED), NULL);
    return MENU_STAY;
}

/* ---------- монтирование ---------- */

static int act_toggle_mount(void *ctx)
{
    struct fs_entry *e = ctx;
    int r = e->mounted ? fs_umount(e) : fs_mount(e);
    if (r != 0)
        menu_message(e->mount_point, tr(S_FAIL), NULL);
    return MENU_STAY;
}

static void mount_refresh(struct menu *m)
{
    for (int i = 0; i < fstab_count(); i++) {
        struct fs_entry *e = fstab_get(i);
        snprintf(m->items[i].label, sizeof(m->items[i].label), "%s [%s]",
                 e->mount_point, e->mounted ? tr(S_MOUNTED) : tr(S_UNMOUNTED));
    }
}

static int act_mount_menu(void *ctx)
{
    (void)ctx;
    if (fstab_count() == 0) {
        menu_message(tr(S_MOUNT_MENU), tr(S_NO_FSTAB), NULL);
        return MENU_STAY;
    }
    struct menu m = { .title = tr(S_MOUNT_MENU), .refresh = mount_refresh };
    for (int i = 0; i < fstab_count(); i++)
        menu_add(&m, fstab_get(i)->mount_point, act_toggle_mount, fstab_get(i));
    menu_add(&m, tr(S_BACK), NULL, NULL);
    return menu_run(&m) < 0 ? MENU_EXIT : MENU_STAY;
}

/* ---------- очистка ---------- */

static int act_wipe_one(void *ctx)
{
    struct fs_entry *e = ctx;
    if (!menu_confirm(tr(S_CONFIRM_WIPE)))
        return MENU_STAY;
    int r = fs_wipe(e);
    menu_message(e->mount_point, r == 0 ? tr(S_OK) : tr(S_FAIL), NULL);
    return MENU_STAY;
}

static int act_wipe_menu(void *ctx)
{
    (void)ctx;
    if (fstab_count() == 0) {
        menu_message(tr(S_WIPE_MENU), tr(S_NO_FSTAB), NULL);
        return MENU_STAY;
    }
    struct menu m = { .title = tr(S_WIPE_MENU) };
    for (int i = 0; i < fstab_count(); i++)
        menu_add(&m, fstab_get(i)->mount_point, act_wipe_one, fstab_get(i));
    menu_add(&m, tr(S_BACK), NULL, NULL);
    return menu_run(&m) < 0 ? MENU_EXIT : MENU_STAY;
}

/* ---------- главное меню ---------- */

int actions_main_menu(void)
{
    struct menu m = { .title = tr(S_TITLE), .top = 1 };

    menu_add(&m, tr(S_REBOOT_SYSTEM),     act_reboot_system, NULL);
    menu_add(&m, tr(S_REBOOT_BOOTLOADER), act_reboot_bootloader, NULL);
    menu_add(&m, tr(S_INSTALL_ZIP),       act_install_zip, NULL);
    menu_add(&m, tr(S_MOUNT_MENU),        act_mount_menu, NULL);
    menu_add(&m, tr(S_WIPE_MENU),         act_wipe_menu, NULL);
    menu_add(&m, tr(S_SHELL),             act_shell, NULL);
    menu_add(&m, tr(S_SHOW_LOG),          act_show_log, NULL);
    menu_add(&m, tr(S_POWER_OFF),         act_poweroff, NULL);

    return menu_run(&m);
}
