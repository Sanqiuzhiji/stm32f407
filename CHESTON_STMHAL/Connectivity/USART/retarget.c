#include <errno.h>
#include <sys/time.h>
#include <retarget.h>
#include <string.h>
#include "usart.h"

#if !defined(OS_USE_SEMIHOSTING)

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

UART_HandleTypeDef *gHuart;

void RetargetInit(UART_HandleTypeDef *huart)
{
    gHuart = huart;
    /* Disable I/O buffering for STDOUT stream, so that
     * chars are sent out as soon as they are printed. */
    setvbuf(stdout, NULL, _IONBF, 0);
}

int _isatty(const int fd)
{
    if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
        return 1;

    errno = EBADF;
    return 0;
}

int _write(const int fd, const char *ptr, const int len)
{
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO)
    {
        // ToDo: Change this to DMA version
        HAL_UART_Transmit(gHuart, (uint8_t *) ptr, len, HAL_MAX_DELAY);
        return len;
    } else
        return -1;
}


int _close(const int fd)
{
    if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
        return 0;

    errno = EBADF;
    return -1;
}

int _lseek(const int fd, const int ptr, const int dir)
{
    (void) fd;
    (void) ptr;
    (void) dir;

    errno = EBADF;
    return -1;
}

int _read(const int fd, char *ptr, int len)
{
    if (fd == STDIN_FILENO)
    {
        const HAL_StatusTypeDef hstatus = HAL_UART_Receive(gHuart, (uint8_t*)ptr, 1, HAL_MAX_DELAY);
        if (hstatus == HAL_OK)
            return 1;
        return EIO;
    }
    errno = EBADF;
    return -1;
}

int _fstat(const int fd, struct stat *st)
{
    if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
    {
        st->st_mode = S_IFCHR;
        return 0;
    }

    errno = EBADF;
    return 0;
}

void Serial_SendByte(const uint8_t Byte)
{
    HAL_UART_Transmit(gHuart, &Byte, sizeof(Byte), HAL_MAX_DELAY);
}

void Serial_SendArray(const uint8_t *Array, const uint8_t Array_length)
{
    HAL_UART_Transmit(gHuart, Array, Array_length, HAL_MAX_DELAY);
}

#endif //#if !defined(OS_USE_SEMIHOSTING)


