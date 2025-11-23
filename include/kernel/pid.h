#ifndef ARCLINE_PID_H
#define ARCLINE_PID_H

#include <stdint.h>

#define MAX_PID 32768

void pid_init(void);
int pid_alloc(void);
void pid_free(int pid);

#endif
