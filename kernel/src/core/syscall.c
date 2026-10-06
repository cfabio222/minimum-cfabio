#include <stdint.h>

#include "minemu/mmu.h"
#include "minemu/platform.h"
#include "minemu/syscall.h"
#include "minemu/user_abi.h"
#include "minemu/user_mode.h"

static int32_t uart_ioctl(volatile struct minemu_uart_regs *uart,
    uint32_t request, uint32_t address, uint32_t length, uint32_t limit)
{
    if (request == MINEMU_IOCTL_UART_STATUS)
    {
        return (int32_t)uart->status;
    }

    if (request != MINEMU_IOCTL_UART_READ &&
        request != MINEMU_IOCTL_UART_WRITE)
    {
        return MINEMU_IOCTL_ERROR;
    }

    if (length > limit)
    {
        return MINEMU_IOCTL_ERROR;
    }

    uint32_t permission = request == MINEMU_IOCTL_UART_READ
        ? MINEMU_PTE_WRITABLE : MINEMU_PTE_READABLE;

    if (!minemu_user_range_valid(address, length, permission))
    {
        return MINEMU_IOCTL_ERROR;
    }

    unsigned char *buffer = (unsigned char *)(uintptr_t)address;
    uint32_t count = 0;

    if (request == MINEMU_IOCTL_UART_READ)
    {
        while (count < length &&
            (uart->status & MINEMU_UART_STATUS_RX_READY))
        {
            buffer[count++] = (unsigned char)uart->rx_data;
        }
    }
    else
    {
        while (count < length)
        {
            while (!(uart->status & MINEMU_UART_STATUS_TX_READY))
            {
            }
            uart->tx_data = buffer[count++];
        }
    }

    return (int32_t)count;
}

static int32_t rng_ioctl(uint32_t request, uint32_t argument)
{
    if (request == MINEMU_IOCTL_RNG_SEED)
    {
        MINEMU_RNG->seed = argument;
        return 0;
    }

    if (request != MINEMU_IOCTL_RNG_NEXT &&
        request != MINEMU_IOCTL_RNG_STATE)
    {
        return MINEMU_IOCTL_ERROR;
    }

    if (!minemu_user_range_valid(argument, sizeof(uint32_t),
        MINEMU_PTE_WRITABLE))
    {
        return MINEMU_IOCTL_ERROR;
    }

    uint32_t value;
    if (request == MINEMU_IOCTL_RNG_NEXT)
    {
        value = MINEMU_RNG->data;
    }
    else
    {
        value = MINEMU_RNG->state;
    }

    unsigned char *destination = (unsigned char *)(uintptr_t)argument;
    const unsigned char *source = (const unsigned char *)&value;

    for (uint32_t i = 0; i < sizeof(value); i++)
    {
        destination[i] = source[i];
    }

    return 0;
}

static int32_t trace_ioctl(uint32_t request, uint32_t value)
{
    if (request != MINEMU_IOCTL_TRACE_EVENT)
    {
        return MINEMU_IOCTL_ERROR;
    }

    MINEMU_TRACE->event = value;
    return 0;
}

static int32_t dispatch_ioctl(uint32_t fd, uint32_t request,
    uint32_t argument, uint32_t length)
{
    if (fd == MINEMU_UART0_BASE || fd == MINEMU_UART1_BASE)
    {
        if (request > 1)
        {
            return MINEMU_IOCTL_ERROR;
        }

        request = request == 0
            ? MINEMU_IOCTL_UART_READ : MINEMU_IOCTL_UART_WRITE;

        volatile struct minemu_uart_regs *uart = fd == MINEMU_UART0_BASE
            ? MINEMU_UART0 : MINEMU_UART1;

        return uart_ioctl(uart, request, argument, length, UINT32_MAX);
    }

    if (fd == MINEMU_RNG_BASE)
    {
        if (request > 1)
        {
            return MINEMU_IOCTL_ERROR;
        }
        request = request == 0
            ? MINEMU_IOCTL_RNG_SEED : MINEMU_IOCTL_RNG_NEXT;
        return rng_ioctl(request, argument);
    }

    if (fd == MINEMU_TRACE_BASE)
    {
        if (request != 0)
        {
            return MINEMU_IOCTL_ERROR;
        }
        return trace_ioctl(MINEMU_IOCTL_TRACE_EVENT, argument);
    }

    switch (fd)
    {
        case MINEMU_FD_UART_IN:
        case MINEMU_FD_UART_OUT:
            return uart_ioctl(MINEMU_UART0, request, argument, length,
                MINEMU_IOCTL_MAX_TRANSFER);

        case MINEMU_FD_UART_DIAG:
            return uart_ioctl(MINEMU_UART1, request, argument, length,
                MINEMU_IOCTL_MAX_TRANSFER);

        case MINEMU_FD_RNG:
            return rng_ioctl(request, argument);

        case MINEMU_FD_TRACE:
            return trace_ioctl(request, argument);

        default:
            return MINEMU_IOCTL_ERROR;
    }
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame)
{
    if (frame->exception_id != MINEMU_EXCEPTION_SVC ||
        (frame->spsr & 0x1fU) != 0x10U ||
        (frame->spsr & 0x20U) != 0 ||
        (frame->fault_pc & 3U) != 0 ||
        !minemu_user_range_valid(frame->fault_pc, sizeof(uint32_t),
            MINEMU_PTE_READABLE | MINEMU_PTE_EXECUTABLE))
    {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    const uint32_t *instruction = (const uint32_t *)(uintptr_t)frame->fault_pc;

    if (*instruction != UINT32_C(0xef000000) ||
        frame->r[7] != MINEMU_SYSCALL_IOCTL)
    {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    frame->r[0] = (uint32_t)dispatch_ioctl(
        frame->r[0], frame->r[1], frame->r[2], frame->r[3]);

    return frame;
}
