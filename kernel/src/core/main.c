#include "minemu/boot.h"
#include "minemu/user_mode.h"

void minemu_kernel_main(const struct minemu_boot_info *boot_info)
{
    minemu_start_user(boot_info);
}
