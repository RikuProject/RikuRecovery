#include "sys.h"

#include <linux/reboot.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "log.h"

int sys_dryrun(void)
{
    const char *v = getenv("RECOVERY_DRYRUN");
    return v && v[0] == '1';
}

void sys_reboot(const char *arg)
{
    log_msg("reboot: %s", arg ? arg : "(system)");
    sync();

    if (sys_dryrun()) {
        log_msg("dry-run: reboot skipped");
        return;
    }

    if (arg && strcmp(arg, "poweroff") == 0) {
        syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_POWER_OFF, NULL);
    } else if (arg) {
        syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_RESTART2, arg);
    } else {
        syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_RESTART, NULL);
    }
}
