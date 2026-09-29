#include "minemu/boot.h"
#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/trap.h"
#include "minemu/trace.h"

#define INPUT_SIZE 256
#define LINE_SIZE 20

static char input[INPUT_SIZE];
static unsigned int input_start;
static unsigned int input_end;

static void uart_receive(void)
{
    while (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY)
    {
        char c = (char)MINEMU_UART0->rx_data;
        unsigned int next = (input_end + 1) % INPUT_SIZE;
        if (next != input_start)
        {
            input[input_end] = c;
            input_end = next;
        }
    }
}

struct minemu_trap_frame *minemu_irq_dispatch(struct minemu_trap_frame *frame)
{
    if (frame->exception_id == (int32_t)MINEMU_IRQ_UART0)
    {
        uart_receive();
    }
    MINEMU_INTERRUPT->eoi = (uint32_t)frame->exception_id;
    return frame;
}

static int uart_getchar(void)
{
    int c = -1;
    minemu_irq_disable();
    if (input_start != input_end)
    {
        c = (unsigned char)input[input_start];
        input_start = (input_start + 1) % INPUT_SIZE;
    }
    minemu_irq_enable();
    return c;
}

static void uart_print(const char *text)
{
    while (*text != '\0')
    {
        while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY))
        {
        }

        MINEMU_UART0->tx_data = (uint32_t)*text;
        text++;
    }
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

void minemu_kernel_main(const struct minemu_boot_info *boot_info)
{
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {
        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }
    minemu_trace_event(1);
    uart_print("hello world\n");
    MINEMU_INTERRUPT->enable = UINT32_C(1) << MINEMU_IRQ_UART0;
    MINEMU_UART0->control = MINEMU_UART_CONTROL_RX_IRQ_ENABLE;
    minemu_irq_enable();

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
