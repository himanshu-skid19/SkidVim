#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

#define CTRL_KEY(k) ((k) & 0x1f) /// this line defines a macro that's used to control key input by masking the input key with a bitwise & operation. CTRL_KEY(k) is the name of the macro with k being a key code or key representation. ((k) & 0x1f) performs a bitwise AND operation
struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}
#define SKIDVIM_VERSION "0.0.1"
#define SKIDVIM_TAB_STOP 8
#define SKIDVIM_QUIT_TIMES 2

enum editorKey {
	BACKSPACE = 127,
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};


typedef struct erow {
	int size;
	int rsize;
	char *chars;
	char *render;
} erow;

struct editorConfig {
	int cx, cy;
	int rx;
	int rowoff;
	int coloff;
	int screenrows;
	int screencols;
	int numrows;
	erow *row;
	int dirty;
	char *filename;
	char statusmsg[80];
	time_t statusmsg_time;
	struct termios orig_termios;

};

struct editorConfig E;

void enableRawMode();
void disableRawMode();
void die(const char *s);
int editorReadKey(void); ///editorReadKey()’s job is to wait for one keypress, and return it. Later, we’ll expand this function to handle escape sequences, which involves reading multiple bytes that represent a single keypress, as is the case with the arrow keys.
void editorProcessKeypress(void); ///editorProcessKeypress() waits for a keypress, and then handles it. Later, it will map various Ctrl key combinations and other special keys to different editor functions, and insert any alphanumeric and other printable keys’ characters into the text that is being edited.
void editorRefreshScreen(void);
void editorDrawRows(struct abuf *); //editorDrawRows() will handle drawing each row of the buffer of text being edited. For now it draws a tilde in each row, which means that row is not part of the file and can’t contain any text.
int getWindowSize (int *, int*);
int getCursorPosition(int *, int *);
void abAppend(struct abuf *, const char *, int);
void abFree(struct abuf *);
void editorMoveCursor(int );
void editorOpen(char *);
void editorInsertRow(int , char *, size_t );
void editorScroll(void);
void editorUpdateRow(erow *);
int editorRowCxToRx(erow *, int);
void editorDrawStatusBar(struct abuf *);
void editorDrawMessageBar(struct abuf *);
void editorRowInsertChar(erow *, int , int );
void editorInsertChar(int );
char *editorRowsToString(int *);
void editorSave(void);
void editorRowDelChar(erow *, int );
void editorDelChar(void);
void editorFreeRow(erow *);
void editorDelRow(int );
void editorRowAppendString(erow *, char *, size_t );
void editorInsertNewline(void);
char *editorPrompt(char *);

void editorSetStatusMessage(const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
	va_end(ap);
	E.statusmsg_time = time(NULL);
 }

void initEditor() {
	E.cx=0;
	E.cy=0;
	E.rx = 0;
	E.rowoff = 0;
	E.coloff = 0;
	E.numrows = 0;
	E.row=NULL;
	E.dirty = 0;
	E.filename = NULL;
	E.statusmsg[0] = '\0';
	E.statusmsg_time = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
	E.screenrows -= 2;
}



int main(int argc, char *argv[]){
	enableRawMode();
	initEditor();
	if (argc >= 2) {
		editorOpen(argv[1]);
	}

	editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");
	
	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}
	return 0;
}

void enableRawMode(){
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
	atexit(disableRawMode);
		
	struct termios raw = E.orig_termios;
	
	raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");

}

void disableRawMode(){
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}

int editorReadKey(){
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("read");
	}

	if (c == '\x1b'){
		char seq[3];

		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '['){
			if (seq[1] >= '0' && seq[1] <= '9'){
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				if (seq[2] == '>') {
					switch (seq[1]) {
						case '1': return HOME_KEY;
						case '3': return DEL_KEY;
						case '4': return END_KEY;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			}
			else {
				switch (seq[1]){
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;

				}
			}
		}
		else if (seq[0] == '0') {
			switch (seq[1]) {
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}

		return '\x1b';
	}
	else {
	return c;
	}
}

char *editorPrompt(char *prompt) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  buf[0] = '\0';
  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();
    int c = editorReadKey();
	if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
		if (buflen != 0) buf[--buflen] = '\0';
	}
	else if (c == '\x1b') {
		editorSetStatusMessage("");
		free(buf);
		return NULL;
	}
    else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
  }
}


void editorMoveCursor(int key) {
	erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

	switch (key){
		case ARROW_LEFT:
			if (E.cx != 0){
				E.cx--;
			}
			else if (E.cy > 0) {
				E.cy--;
				E.cx = E.row[E.cy].size;
			}
			break;
		case ARROW_RIGHT:
			if (row && E.cx < row->size) {
				E.cx++;
			}
			else if (row && E.cx == row->size) {
				E.cy++;
				E.cx = 0;
			}
			break;
		case ARROW_UP:
			if (E.cy != 0) {
				E.cy--;
			}
			break;
		case ARROW_DOWN:
			if (E.cy < E.numrows)  {
				E.cy++;
			}
			break;
	}

	row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
	int rowlen = row ? row->size : 0;
	if (E.cx > rowlen) {
		E.cx = rowlen;
	}
}

void editorProcessKeypress(){
	static int quit_times = SKIDVIM_QUIT_TIMES;

	int c = editorReadKey();

	switch (c) {
		case '\r':
			editorInsertNewline();
			break;

		case CTRL_KEY('q'):
			if (E.dirty && quit_times > 0) {
				editorSetStatusMessage("WARNING!!! File has unsaved changes. " "Press Ctrl-Q %d more times to quit.", quit_times);
				quit_times--;
				return;
			}
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;

		case CTRL_KEY('s'):
			editorSave();
			break;

		case HOME_KEY:
			E.cx = 0;
			break;

		case END_KEY:
			if (E.cy < E.numrows)
				E.cx = E.row[E.cy].size;
			break;

		case BACKSPACE:
		case CTRL_KEY('h'):
		case DEL_KEY:
			if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
			editorDelChar();
			break;

		case PAGE_UP:
		case PAGE_DOWN:
		{
			if (c == PAGE_UP){
				E.cy = E.rowoff;
			}
			else if (c == PAGE_DOWN){
				E.cy = E.rowoff + E.screenrows - 1;
				if (E.cy > E.numrows) E.cy = E.numrows;
			}
			int times = E.screenrows;
			while (times--)
				editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
		}
		break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;

		case CTRL_KEY('l'):
		case '\x1b':
			break;


		default:
			editorInsertChar(c);
			break;
	}
	quit_times  = SKIDVIM_QUIT_TIMES;
}
void die(const char *s){
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(s);
	exit(1);
}

void editorScroll(){
	E.rx = 0;
	if (E.cy < E.numrows) {
		E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
	}
	if (E.cy < E.rowoff) {
		E.rowoff = E.cy;
	}
	if (E.cy >= E.rowoff + E.screenrows){
		E.rowoff = E.cy - E.screenrows + 1;
	}
	if (E.rx < E.coloff) {
    E.coloff = E.rx;
	}
	if (E.rx >= E.coloff + E.screencols) {
		E.coloff = E.rx - E.screencols + 1;
	}
}

void editorDrawRows(struct abuf *ab){
	for (int y = 0; y<E.screenrows; y++) {
		int filerow = y + E.rowoff;
		if (filerow >= E.numrows) {
			if (E.numrows == 0 && y == E.screenrows / 3) {
				char welcome[80];
				int welcomelen = snprintf(welcome, sizeof(welcome), "SkidVim -- version %s", SKIDVIM_VERSION);
				if (welcomelen > E.screencols) welcomelen = E.screencols;
				int padding = (E.screencols - welcomelen) / 2;
				if (padding){
					abAppend(ab, ">", 1);
					padding--;
				}
				while (padding--) abAppend(ab, " ", 1);
				abAppend(ab, welcome, welcomelen);
			}
			else {
			abAppend(ab, ">", 1);
			}
		}
		else{
			int len = E.row[filerow].rsize - E.coloff;
			if (len < 0) len = 0;
			if (len > E.screencols) len = E.screencols;
			abAppend(ab, &E.row[filerow].render[E.coloff], len);
		}
		abAppend(ab, "\x1b[K", 3);
		abAppend(ab, "\r\n", 2);
		}
}

void editorDrawStatusBar(struct abuf *ab) {
	abAppend(ab, "\x1b[7m", 4);
	char status[80], rstatus[80];
	int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "");
	int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);
	if (len > E.screencols) len = E.screencols;
	abAppend(ab, status, len);
	while (len < E.screencols) {
		if (E.screencols - len == rlen){
			abAppend(ab, rstatus, rlen);
			break;
		}
		else{
			abAppend(ab, " ", 1);
			len++;
		}
	}
	abAppend(ab, "\x1b[m", 3);
	abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab){
	abAppend(ab, "\x1b[K", 3);
	int msglen = strlen(E.statusmsg);
	if (msglen > E.screencols) msglen = E.screencols;
	if (msglen && time(NULL) - E.statusmsg_time < 5)
		abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
	editorScroll();

	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);

	editorDrawRows(&ab);
	editorDrawStatusBar(&ab);
	editorDrawMessageBar(&ab);

	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) +1);
	abAppend(&ab, buf, strlen(buf));
	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);

}
///The 4 in our write() call means we are writing 4 bytes out to the terminal. The first byte is \x1b, which is the escape character, or 27 in decimal. (Try and remember \x1b, we will be using it a lot.) The other three bytes are [2J.
//We are writing an escape sequence to the terminal. Escape sequences always start with an escape character (27) followed by a [ character. Escape sequences instruct the terminal to do various text formatting tasks, such as coloring text, moving the cursor around, and clearing parts of the screen.
//We are using the J command (Erase In Display) to clear the screen.   In this case the argument is 2, which says to clear the entire screen. <esc>[1J would clear the screen up to where the cursor is, and <esc>[0J would clear the screen from the cursor up to the end of the screen.

int getCursorPosition(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;

	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

	while (i < sizeof(buf) - 1) {
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return -1;
}


int getWindowSize(int *rows, int*cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
		return -1;
		return getCursorPosition(rows, cols);
	}
	else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

int editorRowCxToRx(erow *row, int cx) {
	int rx = 0;
	for (int j = 0; j < cx; j++){
		if (row->chars[j] == '\t')
			rx += (SKIDVIM_TAB_STOP - 1) - (rx % SKIDVIM_TAB_STOP);
		rx++;

	}
	return rx;
}

void editorUpdateRow(erow *row) {
	int tabs = 0;
	for (int j = 0; j<row->size; j++){
		if (row->chars[j] == '\t') tabs++;
	}
	free(row->render);
	row->render = malloc(row->size + tabs*(SKIDVIM_TAB_STOP - 1) + 1);

	int idx = 0;
	for (int j = 0; j < row->size; j++){
		if (row->chars[j] == '\t'){
			row->render[idx++] = ' ';
			while (idx % SKIDVIM_TAB_STOP != 0) row->render[idx++] = ' ';
		} else {
			row->render[idx++] = row->chars[j];
		}
	}
	row->render[idx] = '\0';
	row->rsize = idx;
}

void editorInsertRow(int at, char *s, size_t len) {
	if (at < 0 || at > E.numrows) return;

	E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
	memmove(&E.row[at + 1], &E.row[at], sizeof(erow)*(E.numrows - at));


	E.row[at].size = len;
	E.row[at].chars = malloc(len + 1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';

	E.row[at].rsize = 0;
	E.row[at].render = NULL;
	editorUpdateRow(&E.row[at]);

	E.numrows++;
	E.dirty++;
}

void editorFreeRow(erow *row){
	free(row->render);
	free(row->chars);
}

void editorDelRow(int at){
	if (at < 0 || at >= E.numrows) return;
	editorFreeRow(&E.row[at]);
	memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
	E.numrows--;
	E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c){
	if (at < 0 || at > row->size) at = row->size;
	row->chars = realloc(row->chars, row->size + 2);
	memmove(&row->chars[at+1], &row->chars[at], row->size-at+1);
	row->size++;
	row->chars[at] = c;
	editorUpdateRow(row);
	E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
	row->chars = realloc(row->chars, row->size + len + 1);
	memcpy(&row->chars[row->size], s, len);
	row->size += len;
	row->chars[row->size] = '\0';
	editorUpdateRow(row);
	E.dirty++;
}

void editorRowDelChar(erow *row, int at){
	if (at < 0 || at >= row->size) return;
	memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
	row->size--;
	editorUpdateRow(row);
	E.dirty++;
}

void editorInsertChar(int c) {
	if (E.cy == E.numrows) {
		editorInsertRow(E.numrows, "", 0);
	}
	editorRowInsertChar(&E.row[E.cy], E.cx, c);
	E.cx++;
}

void editorInsertNewline(){
	if (E.cx == 0) {
		editorInsertRow(E.cy, "", 0);
  } else {
		erow *row = &E.row[E.cy];
		editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
		row = &E.row[E.cy];
		row->size = E.cx;
		row->chars[row->size] = '\0';
		editorUpdateRow(row);
  }
	E.cy++;
	E.cx = 0;
}

void editorDelChar(){
	if (E.cy == E.numrows) return;

	erow *row = &E.row[E.cy];
	if (E.cx > 0) {
		editorRowDelChar(row, E.cx - 1);
		E.cx--;
	}
	else {
		E.cx = E.row[E.cy - 1].size;
		editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
		editorDelRow(E.cy);
		E.cy--;
	}
}

char *editorRowsToString(int *buflen) {
	int totlen = 0;
	for (int j = 0; j < E.numrows; j++)
		totlen += E.row[j].size + 1;
	*buflen = totlen;

	char *buf = malloc(totlen);
	char *p = buf;
	for (int j = 0; j < E.numrows; j++){
		memcpy(p, E.row[j].chars, E.row[j].size);
		p += E.row[j].size;
		*p = '\n';
		p++;
	}

	return buf;
}

void editorOpen(char *filename) {
	free(E.filename);
	E.filename = strdup(filename);
	FILE *fp = fopen(filename, "r");
	if (!fp) die("fopen");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp)) != -1){
		while (linelen > 0 && (line[linelen -1] == '\n' || line[linelen -1] == '\r'))
			linelen--;
		editorInsertRow(E.numrows, line, linelen);
	}
	free(line);
	fclose(fp);
	E.dirty=0;
}


void editorSave() {
  if (E.filename == NULL){
		E.filename = editorPrompt("Save as: %s (ESC to cancel)");
		if (E.filename == NULL) {
			editorSetStatusMessage("Save aborted");
			return;
		}
	}
  int len;
  char *buf = editorRowsToString(&len);
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1){
	if (ftruncate(fd, len) != -1) {
		if (write(fd, buf, len) == len ) {
			close(fd);
			free(buf);
			E.dirty = 0;
			editorSetStatusMessage("%d bytes written to disk", len);
			return;
		}
	}
	close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL) return;
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

//To append a string s to an abuf, the first thing we do is make sure we allocate enough memory to hold the new string. We ask realloc() to give us a block of memory that is the size of the current string plus the size of the string we are appending. realloc() will either extend the size of the block of memory we already have allocated, or it will take care of free()ing the current block of memory and allocating a new block of memory somewhere else that is big enough for our new string.
//Then we use memcpy() to copy the string s after the end of the current data in the buffer, and we update the pointer and length of the abuf to the new values.

//abFree() is a destructor that deallocates the dynamic memory used by an abuf.

void abFree(struct abuf *ab) {
	free(ab->b);
}

//We are sending two escape sequences one after the other. The C command (Cursor Forward) moves the cursor to the right, and the B command (Cursor Down) moves the cursor down. The argument says how much to move it right or down by. We use a very large value, 999, which should ensure that the cursor reaches the right and bottom edges of the screen.

//An append buffer consists of a pointer to our buffer in memory, and a length. We define an ABUF_INIT constant which represents an empty buffer. This acts as a constructor for our abuf type.
