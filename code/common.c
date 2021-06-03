#include "common.h"

#ifndef _WIN32
#include <termios.h>
#include <sys/select.h>

int kbhit() {
	struct timeval tv;
	fd_set fds;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
	return FD_ISSET(STDIN_FILENO, &fds);
}

void nonblock(int state) {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, & ttystate); //- get the terminal state
    if (state == NB_ENABLE) {
        ttystate.c_lflag &= ~ICANON; //- turn off canonical mode
        ttystate.c_cc[VMIN] = 1; //- minimum input chars read
    } else if (state == NB_DISABLE) {
        ttystate.c_lflag |= ICANON; //- turn on canonical mode
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}
#endif
