//Mostly taken from GNU less's source code.  Probably only works in Linux and probably other unixes but not likely to work in Windows without something like Cygwin, and even then...

#include <string.h>

#include "rawterm.h"

static struct termios save_term;
static struct termios raw_term;

int rawterm_init() {
	if(tcgetattr(0, &save_term) != 0) {
		return(-1);
	}
	memcpy(&raw_term, &save_term, sizeof(struct termios));

	raw_term.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL);
	raw_term.c_cc[VMIN] = 1;
	raw_term.c_cc[VTIME] = 0;

	return(0);
}

/* sets the terminal for raw, non blocking IO mode.
 * 
 * return
 *		-1	couldn't set flags
 *		0	success
 */
int rawterm_set() {
	fsync(0);
	if(tcsetattr(0, TCSADRAIN, &raw_term) != 0) {
		return(-2);
	}

	return(0);
}

/* restores terminal to original state
 *
 * return
 *		-1	couldn't set old flags
 *		0	success
 */
int rawterm_unset() {
	fsync(0);
	if(tcsetattr(0, TCSADRAIN, &save_term) != 0) {
		return(-1);
	}

	return(0);
}

/* prints using write()
 *
 * format	fprintf format string
 * ...		arguments to format string
 * returns
 *		-ENOMEM		couldn't allocate memory
 *		-1			not all bytes were written
 *		>=0			length written
 */
int rawterm_printf(const char *format, ...) {
	va_list argptr;
	char* temp;
	int len;

	temp = NULL;
	va_start(argptr, format);
	len = vsnprintf(temp, 0, format, argptr);
	va_end(argptr);

	if(len > 0) {
		temp = malloc(len + 1);
		if(temp == NULL) {
			return(-ENOMEM);
		}
		va_start(argptr, format);
		if(vsprintf(temp, format, argptr) < len) {
			return(-1);
		}
		va_end(argptr);
		if(write(0, temp, len) < len) {
			return(-1);
		}
		free(temp);
	}

	return(len);
}
