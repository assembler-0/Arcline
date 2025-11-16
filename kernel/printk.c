#include <drivers/serial.h>
#include <kernel/printk.h>
#include <kernel/types.h>

// Output device function pointer
typedef void (*output_func_t)(char c);

// Output devices
static output_func_t stdout_output = serial_putc;
static output_func_t stderr_output = serial_putc;

// Get output function for file descriptor
static output_func_t get_output_func(int fd) {
    switch (fd) {
    case STDOUT_FD:
        return stdout_output;
    case STDERR_FD:
        return stderr_output;
    default:
        return NULL;
    }
}

// Simple printf implementation
static int do_printf(output_func_t output, const char *fmt, va_list args) {
    if (!output || !fmt)
        return -1;

    int count = 0;

    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                char buf[12];
                int i = 0;
                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    while (val > 0) {
                        buf[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                }
                while (i > 0) {
                    output(buf[--i]);
                    count++;
                }
                break;
            }

            case 'd': {
                int val = va_arg(args, int);
                if (val < 0) {
                    output('-');
                    count++;
                    val = -val;
                }
                char buf[12];
                int i = 0;
                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    while (val > 0) {
                        buf[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                }
                while (i > 0) {
                    output(buf[--i]);
                    count++;
                }
                break;
            }

            case 'l': {
                if (*(fmt + 1) == 'l' && *(fmt + 2) == 'u') {
                    fmt += 2;
                    uint64_t val = va_arg(args, uint64_t);
                    char buf[24];
                    int i = 0;
                    if (val == 0) {
                        buf[i++] = '0';
                    } else {
                        while (val > 0) {
                            buf[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                    }
                    while (i > 0) {
                        output(buf[--i]);
                        count++;
                    }
                } else if (*(fmt + 1) == 'l' && *(fmt + 2) == 'd') {
                    fmt += 2;
                    int64_t val = va_arg(args, int64_t);
                    if (val < 0) {
                        output('-');
                        count++;
                        val = -val;
                    }
                    char buf[24];
                    int i = 0;
                    if (val == 0) {
                        buf[i++] = '0';
                    } else {
                        while (val > 0) {
                            buf[i++] = '0' + (val % 10);
                            val /= 10;
                        }
                    }
                    while (i > 0) {
                        output(buf[--i]);
                        count++;
                    }
                }
                break;
            }

            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                char buf[9];
                int i = 0;

                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    while (val > 0) {
                        int digit = val & 0xF;
                        buf[i++] =
                            (digit < 10) ? '0' + digit : 'a' + digit - 10;
                        val >>= 4;
                    }
                }

                while (i > 0) {
                    output(buf[--i]);
                    count++;
                }
                break;
            }

            case 'p': {
                void *ptr = va_arg(args, void *);
                uint64_t val = (uint64_t)ptr;

                output('0');
                output('x');
                count += 2;

                char buf[17];
                int i = 0;

                if (val == 0) {
                    buf[i++] = '0';
                } else {
                    while (val > 0) {
                        int digit = val & 0xF;
                        buf[i++] =
                            (digit < 10) ? '0' + digit : 'a' + digit - 10;
                        val >>= 4;
                    }
                }

                while (i > 0) {
                    output(buf[--i]);
                    count++;
                }
                break;
            }

            case 's': {
                const char *str = va_arg(args, const char *);
                if (!str)
                    str = "(null)";

                while (*str) {
                    output(*str++);
                    count++;
                }
                break;
            }

            case 'c': {
                char c = (char)va_arg(args, int);
                output(c);
                count++;
                break;
            }

            case '%':
                output('%');
                count++;
                break;

            default:
                output('%');
                output(*fmt);
                count += 2;
                break;
            }
        } else {
            output(*fmt);
            count++;
        }
        fmt++;
    }

    return count;
}

int vfprintk(int fd, const char *fmt, va_list args) {
    if (!fmt)
        return -1;

    output_func_t output = get_output_func(fd);
    if (!output)
        return -1;

    return do_printf(output, fmt, args);
}

int vprintk(const char *fmt, va_list args) {
    return vfprintk(STDOUT_FD, fmt, args);
}

int fprintk(int fd, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vfprintk(fd, fmt, args);
    va_end(args);
    return ret;
}

// Non-variadic version for testing
int printk_simple(const char *fmt) {
    if (!fmt)
        return -1;

    while (*fmt) {
        serial_putc(*fmt++);
    }
    return 0;
}

int printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = vprintk(fmt, args);
    va_end(args);
    return ret;
}

void printk_init(void) {
    // For now, both stdout and stderr go to serial
    // Later we can add framebuffer support
}