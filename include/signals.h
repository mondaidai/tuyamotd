#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>

typedef void Sigfunc(int);

extern volatile sig_atomic_t keep_running;

void handle_signal(int signo);

Sigfunc *Signal(int signo, Sigfunc *func);

#endif