#ifndef FSTAB_H
#define FSTAB_H

#define FSTAB_MAX 32

struct fs_entry {
    char mount_point[64];
    char device[128];
    char fstype[32];
    char options[128];
    int  mounted; /* 1, если смонтировано нами */
};

/* Формат строки: <точка монтирования> <устройство> <fs> [опции]
 * Пример:  /cache  /dev/block/by-name/cache  ext4  rw */
int fstab_load(const char *path);
int fstab_count(void);
struct fs_entry *fstab_get(int i);

int fs_mount(struct fs_entry *e);
int fs_umount(struct fs_entry *e);
int fs_wipe(struct fs_entry *e);

#endif
