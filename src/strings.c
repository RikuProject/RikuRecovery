#include "strings.h"

static enum lang g_lang = LANG_RU;

static const char *const ru[S_COUNT] = {
    [S_TITLE]           = "Рекавери",
    [S_REBOOT_SYSTEM]   = "Перезагрузить систему",
    [S_REBOOT_BOOTLOADER] = "В загрузчик",
    [S_POWER_OFF]       = "Выключить",
    [S_MOUNT_MENU]      = "Монтирование разделов",
    [S_WIPE_MENU]       = "Очистка разделов",
    [S_SHELL]           = "Командная строка",
    [S_SHOW_LOG]        = "Показать лог",
    [S_INSTALL_ZIP]     = "Установить ZIP",
    [S_BACK]            = "Назад",
    [S_OK]              = "Готово",
    [S_FAIL]            = "Ошибка",
    [S_MOUNTED]         = "смонтирован",
    [S_UNMOUNTED]       = "размонтирован",
    [S_NOT_IMPLEMENTED] = "Пока не реализовано",
    [S_CONFIRM_WIPE]    = "Стереть раздел?",
    [S_YES]             = "Да, стереть",
    [S_NO]              = "Нет",
    [S_NO_FSTAB]        = "Нет таблицы разделов",
    [S_HINT]            = "+/- выбор  ПИТ - ОК",
    [S_HINT_TOUCH]      = "Нажмите на пункт",
    [S_BOOT_INIT]       = "Запуск…",
    [S_BOOT_PARTS]      = "Разделы",
    [S_BOOT_INPUT]      = "Кнопки",
    [S_PRESS_ANY]       = "Любая кнопка",
};

static const char *const en[S_COUNT] = {
    [S_TITLE]           = "Recovery",
    [S_REBOOT_SYSTEM]   = "Reboot system",
    [S_REBOOT_BOOTLOADER] = "Bootloader",
    [S_POWER_OFF]       = "Power off",
    [S_MOUNT_MENU]      = "Mount partitions",
    [S_WIPE_MENU]       = "Wipe partitions",
    [S_SHELL]           = "Shell",
    [S_SHOW_LOG]        = "Show log",
    [S_INSTALL_ZIP]     = "Install ZIP",
    [S_BACK]            = "Back",
    [S_OK]              = "Done",
    [S_FAIL]            = "Failed",
    [S_MOUNTED]         = "mounted",
    [S_UNMOUNTED]       = "unmounted",
    [S_NOT_IMPLEMENTED] = "Not implemented yet",
    [S_CONFIRM_WIPE]    = "Wipe partition?",
    [S_YES]             = "Yes, wipe",
    [S_NO]              = "No",
    [S_NO_FSTAB]        = "No partition table",
    [S_HINT]            = "+/- select  PWR - OK",
    [S_HINT_TOUCH]      = "Tap an item",
    [S_BOOT_INIT]       = "Starting…",
    [S_BOOT_PARTS]      = "Partitions",
    [S_BOOT_INPUT]      = "Buttons",
    [S_PRESS_ANY]       = "Any button",
};

void str_set_lang(enum lang l)
{
    g_lang = l;
}

const char *tr(enum str_id id)
{
    const char *s = (g_lang == LANG_RU ? ru : en)[id];
    return s ? s : "?";
}
