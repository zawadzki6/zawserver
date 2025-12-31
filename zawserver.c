#include <asm-generic/socket.h>
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
#include <ctype.h>
#include <arpa/inet.h>
#include <time.h>

#include "types.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* *** alpha after infdev_3 (soon) *** */
#define VERSION "infdev_2.7"

extern const char* __progname;



/* DEBUG enables debug prints
   SILENT hides all logs (excluding debug)
   DEV is to be used for extra debug logs (only in devel) - deprecated, use DEBUG
   HEAD displays headers */
bool DEBUG = false;
bool SILENT = false;
bool HEAD = true;
bool COLOR = true;

/* ROADMAP ***
 basic GET functionality - done
 GET file mime types - done
 common redirects
 config file
 more customisability
 logging rework - done
 other request methods - soon(tm)
 HTTPS - soon(tm) */

void print_log(const unsigned short int t, const char* str);
void dbg_print(const char* s);
void sigint_handler(int s);
void segfault_handler(int s);
void sigterm_handler(int s);
void sigpipe_handler(int s);
void dump();
void quit(int c);
const char* get_type(const char* ext);
const char* get_binary(char* argv[]);
bool stralnum(char* str);
bool strascii(char* str);

int sock, client_fd, opened_fd;
FILE* fp = NULL;
char buffer[1024] = {};
int port = 8080;
bool validPort = false;
int validated;
const int one = 1; /* yes */

int main(int argc, char* argv[]) {
    signal(SIGSEGV, segfault_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, sigpipe_handler);

    dbg_print("I am the one who debugs\n");

    int i = 1;
    while (i < argc) {
    if (argv[i]) {
	char* argument = argv[i];
	if (strcmp(argument, "-s") == 0) { SILENT = true; dbg_print("silent mode enabled !!\n"); }
	else if (strcmp(argument, "-d") == 0) { DEBUG= true; dbg_print("debug enabled !!\n"); }
	else if (strcmp(argument, "-v") == 0) {
	    printf("%sZAWServer%s %s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET, VERSION); 
	    printf("%s compiled on %s %s\n", __FILE__, __DATE__, __TIME__);
	    return 0;
	}
	else if (strcmp(argument, "--nocolor") == 0) COLOR = false;
	else if (strcmp(argument, "--noheader") == 0) HEAD = false;
	else if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
	    printf("valid arguments:\n");
	    printf(" -h, --help	this message\n");
	    printf(" <port>         self explanatory\n");
	    printf(" -d		debug\n");
	    printf(" -s		silent\n");
	    printf(" -v		version/build info\n");
	    printf(" --nocolor	removes all colors from output\n");
	    printf(" --noheader	don't display any headers\n");
	    return 0;
	}
	else {
	    if (atoi(argv[i]) != 0) { validPort = true; validated = atoi(argv[i]); }
	    if (!validPort) { print_log(2, "invalid argument\n"); return -1; }
	}
    } i++; }
    if (!SILENT) printf("This software is provided \"as is\" with absolutely no waranty\n");
    if (COLOR) printf("To quit press \x1b[32mCtrl+C\x1b[0m\n\n");
    else printf("To quit press Ctrl+C\n\n");

    if (validPort) port = validated;
     
    print_log(0, "using port ");
    printf("%d\n", port);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = 0;

    dbg_print("initialized socket\n");

    int b = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (DEBUG) { dbg_print("binding socket returned "); printf("%d, errno: %d\n", b, errno); }

    if (b != 0) {
	print_log(2, "socket binding failed with code "); printf("%d\n", errno);
	quit(-1);
    }

    print_log(0, "starting server\n");
    listen(sock, 1);

    while (1) {
	int client_fd = accept(sock, 0, 0);

	socklen_t addr_len = sizeof(addr);
	getpeername(client_fd, (struct sockaddr*)&addr, &addr_len);
	char peer_name[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, (struct in_addr*)&addr.sin_addr, peer_name, INET_ADDRSTRLEN);

	print_log(0, "incoming request from ");
	printf("%s\n", peer_name);

	errno = 0;
	char buffer[1024] = {};
	recv(client_fd, buffer, 1024, 0);

	if (errno == 104) {
	    print_log(0, "connection reset by peer\n");
	    close(client_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}

	print_log(0, "recieved buffer\n");
	if (HEAD) printf("--- BEGIN HTTP HEADER ---\n%s\n---- END HTTP HEADER ----\n", buffer);
	
	if (strcmp(buffer, "") == 0 || strcmp(buffer, "\0") == 0 || strcmp(buffer, " ") == 0) {
	    print_log(1, "buffer empty\n");
	    close(client_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}

	if (buffer[1023] > 0) {
	    print_log(1, "buffer reached limit\n");
	    send(client_fd, "HTTP/1.1 431 Request Header Fields Too Large\r\n", 46, 0);
	    print_log(0, "sent response (431 Request Header Fields Too Large)\n");
	    close(client_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}

	if (!strascii(buffer)) {
	    dbg_print("buffer is not ascii compliant\n");
	    print_log(2, "buffer did not get validated\n");
	    print_log(0, "throwing 400 Bad Request\n");
	    send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    close(client_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}

	/* according to man pages, strcnmp returns 0 if strings are equal
	   so why the fuck is this oppositve here?
	    EDIT: what the fuck */
	if (strncmp(buffer, "GET", 3) != 0) { /* || !isalnum(buffer[1])) { */
	    print_log(2, "request is not GET\n");
	    print_log(0, "throwing 400 Bad Request\n");
	    send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    close(client_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}



	char* file = buffer + 5;
	*strchr(file, ' ') = 0;
	dbg_print("request: "); if (DEBUG) printf("%s\n", file);

    	if (file[0] == '\0') {
	    dbg_print("peer requested '/'\n");
	    print_log(2, "peer didn't request a file\n");
	    print_log(0, "throwing 400 Bad Request\n");
	    send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}


	char* ext = "";
	if (strchr(file, '.') != NULL) 
	    ext = strrchr(file, '.') + 1;

	if (strcmp(ext, "\0") == 0) { print_log(1, "extension is null\n"); }
	else if (DEBUG) if (ext) printf("found extension: %s\n", ext);
	if (DEBUG) { dbg_print(""); printf("errno: %d\n", errno); }

	if (strcmp(ext, "\0") != 0 && !stralnum(ext)) {
	    dbg_print("file name didn't get validated\n");
	    print_log(2, "invalid file name\n");
	    print_log(0, "throwing 400 Bad Request\n");
	    send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    close(client_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}

	if (DEBUG) { dbg_print(""); printf("file: %s, binary: %s\n", file, __progname); }
	if (strcmp(file, __progname) == 0) {

	    /* thou shall not request the server's binary. peer will be executed by firing squad. */
	    dbg_print("requested file is equal to server binary\n");

	    print_log(2, "peer requested server's binary\n");
	    print_log(0, "throwing 403 Forbidden\n");
	    send(client_fd, "HTTP/1.1 403 Forbidden\r\n", 24, 0);
	    close(client_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}


	int opened_fd = open(file, O_RDONLY);
	dbg_print("opened file descriptor\n");
	if (DEBUG) { dbg_print(""); printf("errno: %d\n", errno); }

	struct stat st;
	stat(file, &st);
	
	if (!S_ISREG(st.st_mode)) {
	    dbg_print("file is irregular, a directory or doesn't exist\n");
	    print_log(2, "request has no valid file\n");
	    print_log(0, "throwing 404 Not Found\n");
	    send(client_fd, "HTTP/1.1 404 Not Found\r\n", 24, 0);
	    close(client_fd);
	    close(opened_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}

	if (errno == 2) {
	    dbg_print("couldn't open the file. returning 404\n");
	    print_log(2, "i/o error\n");
	    print_log(0, "throwing 404 Not Found\n");
	    send(client_fd, "HTTP/1.1 404 Not Found\r\n", 24, 0);
	    close(client_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}

	if (access(file, R_OK) != 0) {
	    dbg_print("access() returned non-zero value. errno is "); if (DEBUG) printf("%d\n", errno);
	    print_log(2, "i/o error\n");
	    print_log(0, "throwing 500 Internal Server Error\n");
	    send(client_fd, "HTTP/1.1 500 Internal Server Error\r\n", 36, 0);
	    close(client_fd);
	    print_log(0, "connection closed\n");
	    continue;
	}
	
	errno = 0;
	FILE* fp = fopen(file, "r");
	if (fp == NULL) {
	    dbg_print("file is null, for some reason it could not be opened\n");
	    print_log(2, "failed to open file\n");
	    print_log(0, "throwing 500 Internal Server Error\n");
	    send(client_fd, "HTTP/1.1 500 Internal Server Error\r\n", 36, 0);
	    print_log(0, "connection closed\n");
	    continue;
	}
	dbg_print("opened file\n");
	
	unsigned int length = st.st_size;

	char guhhhh[16];
	sprintf(guhhhh, "%d", length);

	char header2[] = "HTTP/1.1 200 OK\r\n""Content-Type: ";
	strcat(header2, get_type(ext));
	strcat(header2, "\r\n""Content-Length: ");
	strcat(header2, guhhhh);
	strcat(header2, "\r\n""Connection: close\r\n""\r\n");

	print_log(0, "formed response header\n");
	if (HEAD) printf("--- BEGIN HTTP HEADER ---\n%s\n---- END HTTP HEADER ----\n", header2);
	send(client_fd, header2, strlen(header2), 0);
	print_log(0, "sent header\n");


	print_log(0, "sending data now\n");
	errno = 0;
	int c = sendfile(client_fd, opened_fd, 0, length);
	print_log(0, "finished uploading\n");
	if (DEBUG) { dbg_print("uploading returned "); printf("%d, errno: %d\n", c, errno); }

	fclose(fp);
	close(opened_fd);
	close(client_fd);
	print_log(0, "connection closed\n");
    }
}


void sigint_handler(int s) {
    putchar('\n');
    dbg_print("caught SIGINT\n");
    quit(0);
}

void segfault_handler(int s) {
    dbg_print("caught SIGSEGV\n");
    print_log(2, "error - segmentation fault\n");
    if (DEBUG) dump();
    if (fp != NULL) fclose(fp);
    shutdown(sock, SHUT_RDWR);
    close(client_fd);
    abort();
}

void dump() {
    print_log(0, "dumping variables\n");
    printf("errno: %d\n", errno);
    printf("fp: %s\n", fp == NULL ? "NULL" : "valid(?)");
    printf("sock: %d\nclient_fd: %d\nopened_fd: %d\n", sock, client_fd, opened_fd);
    printf("port: %d\n", port);
    printf("validPort: %s\n", validPort ? "true" : "false");
}

void quit(int c) {
    print_log(0, "cleaning up\n");
    if (fp != NULL) fclose(fp);
    close(opened_fd);
    close(client_fd);
    shutdown(sock, SHUT_RDWR);
    print_log(0, "quitting\n");
    exit(c);
}

void sigpipe_handler(int s) {
    dbg_print("caught SIGPIPE\n");
    print_log(2, "attempted to send data to an invalid client\n");
    if (DEBUG) dump();
    if (fp != NULL) fclose(fp);
    close(opened_fd);
    close(client_fd);
    print_log(0, "cleaned up\n");
}

void sigterm_handler(int s) { quit(0); }

void dbg_print(const char* log) {
    if (!DEBUG) return;
    if (COLOR) printf("%ld %sDEBG%s %s", time(NULL), ANSI_COLOR_MAGENTA, ANSI_COLOR_RESET, log);
    else printf("%ld DEBG %s", time(NULL), log);
}


void print_log(const unsigned short int t, const char* str) {
    if (SILENT) return;
    if (COLOR) {
	switch (t) {
	    case 0: printf("%ld %sINFO%s %s", time(NULL), ANSI_COLOR_BLUE, ANSI_COLOR_RESET, str); break;
	    case 1: printf("%ld %sWARN%s %s", time(NULL), ANSI_COLOR_YELLOW, ANSI_COLOR_RESET, str); break;
	    case 2: printf("%ld %sFAIL%s %s", time(NULL), ANSI_COLOR_RED, ANSI_COLOR_RESET, str); break;
	}
    }
    else {
	switch (t) {
	    case 0: printf("%ld INFO %s", time(NULL), str); break;
	    case 1: printf("%ld WARN %s", time(NULL), str); break;
	    case 2: printf("%ld FAIL %s", time(NULL), str); break;
	}
    }
}

const char* get_type(const char* ext) {
    if (ext == NULL) return media[0].type;

    int i;
    for (i = 0; i < types_amount; i++) {
	if (strcmp(media[i].ext, ext) == 0)
	    return media[i].type;
    }
    return media[0].type;
}

/* --- deprecated --- use __progname stupid */
const char* get_binary(char* argv[]) {
    char bin_name2[256]; strcpy(bin_name2, argv[0]);
    if (strrchr(argv[0], '/') != NULL) { strcpy(bin_name2, strchr(argv[0], '/') + 1); }
    const char* bin_name = bin_name2;
    if (DEBUG) {dbg_print(""); printf("\nbin_name: %s\n", bin_name); }

    return bin_name;
}

bool stralnum(char* str) {
    int i;
    for (i = 0; i < strlen(str); i++)
	if (!isalnum(str[i])) return false;

    return true;
}

bool strascii(char* str) {
    int i;
    for (i = 0; i < strlen(str); i++)
	if (!isascii(str[i])) {
	    dbg_print("check failed at str["); printf("%d], '%c'\n", i, str[i]);
	    return false;
	}

    return true;
}
