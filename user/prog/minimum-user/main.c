#include <stdint.h>
#include "minemu/user_abi.h"

void minemu_user_main(void)
{
    static char message[] = "hello world\n";

    ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE,
        (void *)message, (uint32_t)(sizeof(message) - 1U));

    for (;;)
    {
        __asm__ volatile("nop");
    }
}
