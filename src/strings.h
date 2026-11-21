#ifndef STRINGS_H
#define STRINGS_H

/* Строки интерфейса. Язык по умолчанию — русский. */

enum str_id {
    S_TITLE,
    S_REBOOT_SYSTEM,
    S_REBOOT_BOOTLOADER,
    S_POWER_OFF,
    S_MOUNT_MENU,
    S_WIPE_MENU,
    S_SHELL,
    S_SHOW_LOG,
    S_INSTALL_ZIP,
    S_BACK,
    S_OK,
    S_FAIL,
    S_MOUNTED,
    S_UNMOUNTED,
    S_NOT_IMPLEMENTED,
    S_CONFIRM_WIPE,
    S_YES,
    S_NO,
    S_NO_FSTAB,
    S_HINT,
    S_HINT_TOUCH,
    S_BOOT_INIT,
    S_BOOT_PARTS,
    S_BOOT_INPUT,
    S_PRESS_ANY,
    S_COUNT
};

enum lang { LANG_RU, LANG_EN };

void str_set_lang(enum lang l);
const char *tr(enum str_id id);

#endif
