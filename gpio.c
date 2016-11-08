
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>

#define GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 64

/* returns ?? */
int gpio_export(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(GPIO_DIR "/export", O_WRONLY);
    if (fd < 0)
    {
        perror("gpio/export");
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);
    return 0;
}

int gpio_unexport(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(GPIO_DIR "/unexport", O_WRONLY);
    if (fd < 0) {
        perror("gpio/export");
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);
    return 0;
}

/*  out_flag = 1 for output, 0 for input */
int gpio_set_dir(unsigned int gpio, unsigned int out_flag)
{
    int fd;
    char buf[MAX_BUF];
    snprintf(buf, sizeof(buf), GPIO_DIR  "/gpio%d/direction", gpio);
    fd = open(buf, O_WRONLY);

    if (fd < 0)
    {
        perror("gpio/direction");
        return fd;
    }

    if (out_flag)
        write(fd, "out", 4);
    else
        write(fd, "in", 3);

    close(fd);
    return 0;
}

int gpio_set_value(unsigned int gpio, unsigned int value)
{
    int fd;
    char buf[MAX_BUF];
    snprintf(buf, sizeof(buf), GPIO_DIR "/gpio%d/value", gpio);
    fd = open(buf, O_WRONLY);

    if (fd < 0)
    {
        perror("gpio_set_value");
        return fd;
    }

    if (value)
        write(fd, "1", 2);
    else
        write(fd, "0", 2);

    close(fd);
    return 0;
}

int gpio_get_value(unsigned int gpio, unsigned int *value)
{
    int fd;
    char buf[MAX_BUF];
    snprintf(buf, sizeof(buf), GPIO_DIR "/gpio%d/value", gpio);
    fd = open(buf, O_RDONLY);

    if (fd < 0)
    {
        perror("gpio/get-value");
        return fd;
    }

    gpio_get_value_fd(fd, value);
    close(fd);
    return 0;
}

int gpio_get_value_fd(int fd, unsigned int *value)
{
    char ch;
    read(fd, &ch, 1);

    if (ch != '0')
    {
        *value = 1;
    } else
    {
        *value = 0;
    }

    return 0;
}

int gpio_fd_open(unsigned int gpio)
{
    int fd;
    char buf[MAX_BUF];
    snprintf(buf, sizeof(buf), GPIO_DIR "/gpio%d/value", gpio);
    fd = open(buf, O_RDONLY | O_NONBLOCK );

    if (fd < 0)
    {
        perror("gpio/fd_open");
    }
    return fd;
}

