#include "fstab.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "sys.h"

static struct fs_entry g_tab[FSTAB_MAX];
static int g_n;

int fstab_load(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512];

    g_n = 0;
    if (!f) {
        log_msg("fstab: %s: %s", path, strerror(errno));
        return -1;
    }
    while (fgets(line, sizeof(line), f) && g_n < FSTAB_MAX) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;

        struct fs_entry *e = &g_tab[g_n];
        memset(e, 0, sizeof(*e));
        int n = sscanf(p, "%63s %127s %31s %127s", e->mount_point, e->device,
                       e->fstype, e->options);
        if (n < 3)
            continue;
        g_n++;
    }
    fclose(f);
    log_msg("fstab: loaded %d entries", g_n);
    return g_n;
}

int fstab_count(void)
{
    return g_n;
}

struct fs_entry *fstab_get(int i)
{
    return (i >= 0 && i < g_n) ? &g_tab[i] : NULL;
}

int fs_mount(struct fs_entry *e)
{
    if (e->mounted)
        return 0;
    if (sys_dryrun()) {
        log_msg("dry-run: mount %s", e->mount_point);
        e->mounted = 1;
        return 0;
    }
    mkdir(e->mount_point, 0755);
    unsigned long flags = 0;
    if (strstr(e->options, "ro"))
        flags |= MS_RDONLY;
    if (mount(e->device, e->mount_point, e->fstype, flags, NULL) != 0) {
        log_msg("mount %s -> %s (%s): %s", e->device, e->mount_point,
                e->fstype, strerror(errno));
        return -1;
    }
    e->mounted = 1;
    log_msg("mounted %s", e->mount_point);
    return 0;
}

int fs_umount(struct fs_entry *e)
{
    if (!e->mounted)
        return 0;
    if (sys_dryrun()) {
        log_msg("dry-run: umount %s", e->mount_point);
        e->mounted = 0;
        return 0;
    }
    sync();
    if (umount(e->mount_point) != 0) {
        log_msg("umount %s: %s", e->mount_point, strerror(errno));
        return -1;
    }
    e->mounted = 0;
    log_msg("unmounted %s", e->mount_point);
    return 0;
}

/* Форматирование через внешнюю утилиту. Для ext4 — mke2fs, для f2fs —
 * mkfs.f2fs. Их нужно положить в образ рекавери (/sbin). */
int fs_wipe(struct fs_entry *e)
{
    const char *tool, *arg1 = NULL;

    if (fs_umount(e) != 0)
        return -1;

    if (sys_dryrun()) {
        log_msg("dry-run: wipe %s", e->device);
        return 0;
    }

    if (strcmp(e->fstype, "ext4") == 0) {
        tool = "/sbin/mke2fs";
        arg1 = "-t";
    } else if (strcmp(e->fstype, "f2fs") == 0) {
        tool = "/sbin/mkfs.f2fs";
    } else {
        log_msg("wipe: unsupported fs '%s'", e->fstype);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        if (arg1)
            execl(tool, tool, arg1, e->fstype, "-F", e->device, (char *)NULL);
        else
            execl(tool, tool, e->device, (char *)NULL);
        _exit(127);
    }
    int st;
    waitpid(pid, &st, 0);
    int ok = WIFEXITED(st) && WEXITSTATUS(st) == 0;
    log_msg("wipe %s: %s", e->device, ok ? "ok" : "failed");
    return ok ? 0 : -1;
}
