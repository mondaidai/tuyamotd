#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>
extern volatile sig_atomic_t keep_running;

typedef void Sigfunc(int);

Sigfunc *Signal(int signo, Sigfunc *func);

void handle_signal(int signo);

#endif