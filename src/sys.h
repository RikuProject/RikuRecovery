#ifndef SYS_H
#define SYS_H

/* RECOVERY_DRYRUN=1 — ничего не монтировать, не форматировать и не
 * перезагружать. Нужно для безопасной отладки на обычном Linux. */
int sys_dryrun(void);

/* arg: NULL — обычная перезагрузка, "bootloader" и т.п. — RESTART2,
 * "poweroff" — выключение. */
void sys_reboot(const char *arg);

#endif
