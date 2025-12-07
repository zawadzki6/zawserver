#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "types.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define VERSION "infdev"

/* DEBUG enables debug prints
   SILENT hides all logs (excluding debug)
   DUMP prints variables on crash
   DEV is to be used for extra debug logs (only in devel) 
   HEAD displays headers */
bool DEBUG = true;
bool SILENT = false;
bool DUMP = true;
bool DEV = false;
bool HEAD = true;

/* ROADMAP ***
 basic GET functionality - done
 GET file mime types - done
 POST requests - soon(tm) */

void print_log(const unsigned short int t, const char* str);
void dbg_print(const char* s);
void sigint_handler(int s);
void segfault_handler(int s);
void sigterm_handler(int s);
void sigpipe_handler(int s);
void dump();
void quit(int c);
char* type(char* ext);
const char* get_type(const char* ext);

int sock, client_fd, opened_fd;
FILE* fp = NULL;
char buffer[512] = {};
int port = 8080;
bool validPort = false;

int main(int argc, char* argv[]) {
    signal(SIGSEGV, segfault_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, sigpipe_handler);

    if (DEV) printf("argc: %d\nargv[0]: %s\nargv[1]: %s\nargv[2]: %s\n", argc, argv[0], argv[1], argv[2]);
    
    if (argv[1]) {
	const char* argument = argv[1];
	if (strcmp(argument, "-s") == 0) { SILENT = true; dbg_print("silent mode enabled !!"); }
	else if (strcmp(argument, "-d") == 0) { DEBUG = true; dbg_print("debug enabled !!"); }
	else if (strcmp(argument, "-v") == 0) { printf("%s\n", VERSION); return 0; }
	else if (strcmp(argument, "-f") == 0) {
	    printf("%sZAWServer%s %s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET, VERSION);
	    print_log(0, "this is a log");
	    print_log(1, "this is a success");
	    print_log(2, "this is a warning");
	    print_log(3, "this is an error");
	    dbg_print("wow debug is here too");

	    return 0;
	}
	else if (strcmp(argument, "-h") == 0) {
	    printf("valid arguments:\n[port]	self explanatory\n-h	this message\n-v      version\n-f      fun\n-s      silent\n");
	    return 0;
	}
	else {
	    if (atoi(argv[1]) != 0) validPort = true;
	    if (!validPort) { print_log(3, "invalid argument"); return 4; }
	}
    }
    if (!SILENT) printf("This software is provded \"as is\" with absolutely no waranty\n\n");
    print_log(0, "To quit press \x1b[32mCtrl+C\x1b[0m\n");

    if (validPort) {
	const int port_fix = atoi(argv[1]);
	dbg_print("argument is digits!!");
	port = port_fix;
    	print_log(1, "set custom port");
    }
    else print_log(0, "using default port (8080)");

    sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = 0;

    dbg_print("initialized socket");
    int b = bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    print_log(1, "starting listen on port");
    listen(sock, 1);

    while (1) {
	int client_fd = accept(sock, 0, 0);
	print_log(0, "incoming request");

	errno = 0;
	char buffer[512] = {};
	recv(client_fd, buffer, 512, 0);
	
	if (errno == 104) {
	    print_log(0, "connection reset by peer");
	    close(client_fd);
	    continue;
	}
	
	if ((buffer[0] = 0)) {
	    print_log(2, "buffer empty");
	    close(client_fd);
	    print_log(1, "connection closed");
	    continue;
	}
	if (buffer[511] > 0) {
	    print_log(2, "buffer reached limit");
	    send(client_fd, "431 Request Header Fields Too Large\r\n", 40, 0);
	    print_log(0, "sent response (431 Request Header Fields Too Large)");
	    close(client_fd);
	    continue;
	}

	if (strncmp(buffer, "POST", 4) == 0) {
	    print_log(2, "received a POST request");
	    print_log(0, "throwing 501 Not Implemented");
	    send(client_fd, "501 Not Implemented\r\n", 24, 0);
	    close(client_fd);
	    continue;
	}

	/* GET file.txt ... */
	if (HEAD) print_log(0, buffer);
	char* file = buffer + 5;
	*strchr(file, ' ') = 0;

	char* ext = strrchr(file, '.') + 1 ;
	if (DEV) if (ext) printf("found extension: %s\n", ext);
	if (ext) print_log(0, "found extension");
	if (!ext) { print_log(2, "extension is null"); ext = ""; }


	int opened_fd = open(file, O_RDONLY);
	
	errno = 0;
	FILE* fp = fopen(file, "r");
	
	if (errno != 0) {
	    print_log(3, "file not found");
	    send(client_fd, "HTTP/1.1 404 Not Found\r\n", 24, 0);
	    print_log(1, "sent response (404 Not Found)");
	    /* fclose(fp); */
	    close(opened_fd);
	    close(client_fd);
	    print_log(0, "closed connection");
	    continue;
	} 

	struct stat st;
	stat(file, &st);
	unsigned int length = st.st_size;

	char guhhhh[16];
	sprintf(guhhhh, "%d", length);

	char header2[] = "HTTP/1.1 200 OK\r\n""Content-Type: ";
	strcat(header2, get_type(ext));
	strcat(header2, "\r\n""Content-Length: ");
	strcat(header2, guhhhh);
	strcat(header2, "\r\n""Connection: close\r\n""\r\n");

	if (HEAD) print_log(0, header2);
	send(client_fd, header2, strlen(header2), 0);

	sendfile(client_fd, opened_fd, 0, length);
	print_log(1, "sent response");

	fclose(fp);
	close(opened_fd);
	close(client_fd);
	print_log(0, "closed connection");
    }
}


void sigint_handler(int s) {
    dbg_print("caught SIGINT");
    quit(0);
}

void segfault_handler(int s) {
    dbg_print("caught SIGSEGV\a");
    print_log(3, "error - segmentation fault");
    if (DUMP) dump();
    quit(134);
    /* abort(); */
}

void dump() {
    print_log(0, "dumping variables");
    printf("errno: %d\n", errno);
    printf("fp: ");
    int bf;
    if (fp != NULL) while ((bf = (fgetc(fp))) != EOF) { putchar(bf); }
    putchar('\n');
    printf("sock: %d\nclient_fd: %d\nopened_fd: %d\n", sock, client_fd, opened_fd);
}

void quit(int c) {
    print_log(0, "shutting down");
    if (fp != NULL) fclose(fp);
    shutdown(sock, SHUT_RDWR);
    close(client_fd);
    print_log(1, "quitting");
    exit(c);
}

void sigpipe_handler(int s) {
    dbg_print("caught SIGPIPE");
    dump();
    quit(3);
}

void sigterm_handler(int s) {
    quit(2);
}

char* type(char *ext) {
    if (strcmp(ext, "html")) return "text/html";
    return "text/raw";
} 

void dbg_print(const char* log) { if (DEBUG) printf("[%s   DEBG   %s] %s\n", ANSI_COLOR_MAGENTA, ANSI_COLOR_RESET, log); }

void print_log(const unsigned short int t, const char* str) {
    if (SILENT) return;
    switch (t) {
	case 0: printf("[%s   INFO   %s] %s\n", ANSI_COLOR_BLUE, ANSI_COLOR_RESET, str); break;
	case 1: printf("[%s    OK    %s] %s\n", ANSI_COLOR_GREEN, ANSI_COLOR_RESET, str); break;
	case 2: printf("[%s   WARN   %s] %s\n", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET, str); break;
	case 3:	printf("[%s   FAIL   %s] %s\n", ANSI_COLOR_RED, ANSI_COLOR_RESET, str); break;
    }
}

const char* get_type(const char* ext) {
    int i;
    for (i = 0; i < types_amount; i++) {
	if (strcmp(media[i].ext, ext) == 0)
	    return media[i].type;
    }
    return media[0].type;
}
