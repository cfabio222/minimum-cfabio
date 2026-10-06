#include <stdint.h>
#include "minemu/user_abi.h"

#define LINE_SIZE 128

static int uart_getchar(void)
{
    unsigned char c;
    int32_t result = ioctl(MINEMU_FD_UART_IN, MINEMU_IOCTL_UART_READ,
        (void *)&c, UINT32_C(1));

    if (result == 1)
    {
        return c;
    }
    return -1;
}

static void uart_print(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0')
    {
        length++;
    }

    ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE,
        (void *)text, length);
}

static void run_command(char *line)
{
    char *command = line;
    char *end;

    while (*command == ' ')
    {
        command++;
    }
    if (*command == '\0')
    {
        return;
    }

    end = command;
    while (*end != ' ' && *end != '\0')
    {
        end++;
    }

    if (end - command == 4 && command[0] == 'e' && command[1] == 'c' &&
        command[2] == 'h' && command[3] == 'o')
    {
        while (*end == ' ')
        {
            end++;
        }
        char *write = end;
        int space = 0;

        for (char *read = end; *read != '\0'; read++)
        {
            if (*read == ' ')
            {
                space = 1;
            }
            else
            {
                if (space && write != end)
                {
                    *write++ = ' ';
                }
                *write++ = *read;
                space = 0;
            }
        }
        *write = '\0';

        uart_print(end);
        uart_print("\n");
    }
    else
    {
        char saved = *end;
        *end = '\0';
        uart_print("command not found: ");
        uart_print(command);
        uart_print("\n");
        *end = saved;
    }
}

void minemu_user_main(void)
{
    char line[LINE_SIZE + 1];
    unsigned int length = 0;
    int too_long = 0;
    uart_print("msh> ");
    for (;;)
    {
        int c = uart_getchar();
        if (c < 0)
        {
            __asm__ volatile("nop");
            continue;
        }
        if (c == '\n')
        {
            line[length] = '\0';
            if (!too_long)
            {
                run_command(line);
            }
            length = 0;
            too_long = 0;
            uart_print("msh> ");
        }
        else if (c == 0x08 || c == 0x7f)
        {
            if (too_long)
            {
                too_long--;
            }
            else if (length > 0)
            {
                length--;
            }
        }
        else if (too_long)
        {
            too_long++;
        }
        else if (length < LINE_SIZE)
        {
            line[length++] = (char)c;
        }
        else
        {
            too_long = 1;
        }
    }
}
