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

#include "types.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define VERSION "infdev_2.4"

/* DEBUG enables debug prints
   SILENT hides all logs (excluding debug)
   DUMP prints variables on crash - deprecated, use DEBUG
   DEV is to be used for extra debug logs (only in devel) - deprecated, use DEBUG
   HEAD displays headers */
bool DEBUG = false;
bool SILENT = false;
bool DUMP = false;
bool DEV = false;
bool HEAD = true;


/* ROADMAP ***
 basic GET functionality - done
 GET file mime types - done
 common redirects
 config file
 more customisability
 logging rework
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
bool validate_string(char* str);

int sock, client_fd, opened_fd;
FILE* fp = NULL;
char buffer[1024] = {};
int port = 8080;
bool validPort = false;
int validated;

int main(int argc, char* argv[]) {
    signal(SIGSEGV, segfault_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, sigpipe_handler);

    if (DEBUG) DEV = true; else DEV = false;

    if (DEV) printf("argc: %d\nargv[0]: %s\nargv[1]: %s\nargv[2]: %s\n", argc, argv[0], argv[1], argv[2]);

    int i = 1;
    while (i < argc) {
    if (argv[i]) {
	char* argument = argv[i];
	if (strcmp(argument, "-s") == 0) { SILENT = true; dbg_print("silent mode enabled !!\n"); }
	else if (strcmp(argument, "-d") == 0) { DEBUG= true, DEV = true; dbg_print("debug enabled !!\n"); }
	else if (strcmp(argument, "-v") == 0) {
	    printf("%sZAWServer%s %s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET, VERSION); 
	    printf("%s compiled on %s %s\n", __FILE__, __DATE__, __TIME__);
	    return 0;
	}
	else if (strcmp(argument, "-f") == 0) {
	    printf("%sZAWServer%s\n", ANSI_COLOR_CYAN, ANSI_COLOR_RESET);
	    print_log(0, "this is a log\n");
	    print_log(1, "this is a success\n");
	    print_log(2, "this is a warning\n");
	    print_log(3, "this is an error\n");
	    dbg_print("wow debug is here too\n");

	    return 0;
	}
	else if (strcmp(argument, "-h") == 0) {
	    printf("valid arguments:\n[port]	self explanatory\n-h	this message\n-v      version\n-f      fun\n-s      silent\n");
	    return 0;
	}
	else {
	    if (atoi(argv[i]) != 0) { validPort = true; validated = atoi(argv[i]); }
	    if (!validPort) { print_log(3, "invalid argument\n"); return 4; }
	}
    } i++; }
    if (!SILENT) printf("\nThis software is provided \"as is\" with absolutely no waranty\n\n");
    printf("To quit press \x1b[32mCtrl+C\x1b[0m\n\n");

    
    if (validPort) port = validated;
     
    print_log(0, "using port ");
    printf("%d\n", port);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = 0;

    dbg_print("initialized socket\n");
    int b = bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    print_log(0, "starting server\n");
    listen(sock, 1);

    while (1) {
	int client_fd = accept(sock, 0, 0);

    	/* socklen_t addr_len = sizeof(addr);
	getpeername(client_fd, (struct sockaddr*)&addr, &addr_len);
	struct sockaddr_in* peer = (struct sockaddr_in*)&addr;
	struct in_addr peer_addr = peer->sin_addr;
	char peer_name[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &peer_addr, peer_name, INET_ADDRSTRLEN); */

	/* rev 2 */
	socklen_t addr_len = sizeof(addr);
	getpeername(client_fd, (struct sockaddr*)&addr, &addr_len);
	char peer_name[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, (struct in_addr*)&addr.sin_addr, peer_name, INET_ADDRSTRLEN);

	print_log(0, "incoming request from ");
	printf("%s\n", peer_name);

	errno = 0;
	char buffer[1024] = {};
	recv(client_fd, buffer, 1024, 0);

	if (DEV) printf("errno: %d\n", errno);
	if (errno == 104) {
	    print_log(0, "connection reset by peer\n");
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}

	print_log(0, "recieved buffer\n");
	if (HEAD) printf("--- BEGIN HTTP HEADER ---\n%s\n---- END HTTP HEADER ----\n", buffer);
	
	if (DEV) printf("errno: %d\n", errno);
	if ((buffer[0] = 0)) {
	    print_log(2, "buffer empty\n");
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}

	if (DEV) printf("errno: %d\n", errno);
	if (buffer[1023] > 0) {
	    print_log(2, "buffer reached limit\n");
	    send(client_fd, "HTTP/1.1 431 Request Header Fields Too Large\r\n", 46, 0);
	    print_log(0, "sent response (431 Request Header Fields Too Large)\n");
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}

	if (DEV) printf("errno: %d\n", errno);
	/* according to man pages, strcnmp returns 0 if strings are equal
	   so why the fuck is this oppositve here? */
	if (strncmp(buffer, "GET", 4) == 0 || !isalnum(buffer[1])) {
	    print_log(2, "request is not GET\n");
	    print_log(0, "throwing 400 Bad Request\n");
	    send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}

	if (DEV) printf("errno: %d\n", errno);
	char* file = buffer + 5;
	*strchr(file, ' ') = 0;
	if (DEV) printf("errno: %d\n", errno);

 	if (!validate_string(file)) {
	    dbg_print("string didn't get validated\n");
	    print_log(3, "invalid file name\n");
	    print_log(0, "throwing 400 Bad Request\n");
	    send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}   

	if (strcmp(file, "") == 0) {
	    dbg_print("peer probably requested '/'\n");
	    print_log(3, "peer didn't request a file\n");
	    print_log(0, "throwing 400 Bad Request\n");
	    send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}

	if (DEV) printf("file: %s\nbinary: %s\n", file, get_binary(argv));
	if (strcmp(file, get_binary(argv)) == 0) {
	    if (DEV) printf("file: %s\nbin_name: %s\n", file, get_binary(argv));
	    dbg_print("requested file is equal to server binary\n");

	    print_log(3, "peer requested server's binary\n");
	    print_log(0, "throwing 400 Bad Request\n");
	    send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}

	struct stat path;
	stat(file, &path);
	if (!S_ISREG(path.st_mode)) {		/*  <- see inode(7) */
	    print_log(3, "peer didn't request a valid file\n");
	    print_log(0, "throwing 400 Bad Request\n");
	    send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}
	if (DEV) printf("errno: %d\n", errno);

	char* ext = strrchr(file, '.') + 1;
	if (strcmp(ext, "\0") == 0) { print_log(2, "extension is null\n"); ext = NULL; }
	if (DEV) if (ext) printf("found extension: %s\n", ext);
	if (ext) print_log(0, "found extension\n");
	if (DEV) printf("errno: %d\n", errno);

	if (errno == 2) {
	    dbg_print("we have stopped yet another segfault probably from an i/o error. let's return 404\n");
	    print_log(3, "i/o error\n");
	    print_log(0, "throwing 404 Not Found\n");
	    send(client_fd, "HTTP/1.1 404 Not Found\r\n", 24, 0);
	    close(opened_fd);
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}

	int opened_fd = open(file, O_RDONLY);
	dbg_print("opened file descriptor\n");
	if (DEV) printf("errno: %d\n", errno);
	
	errno = 0;
	FILE* fp = fopen(file, "r");
	if (fp == NULL) {
	    dbg_print("file is null!!\n");
	    print_log(3, "failed to open file\n");
	    print_log(0, "throwing 500 Internal Server Error\n");
	    send(client_fd, "HTTP/1.1 500 Internal Server Error\r\n", 36, 0);
	    close(opened_fd);
	    close(client_fd);
	    print_log(1, "connection closed\n");
	    continue;
	}
	dbg_print("opened file\n");
	
	if (errno != 0) {
	    print_log(3, "file not found\n");
	    send(client_fd, "HTTP/1.1 404 Not Found\r\n", 24, 0);
	    print_log(1, "sent response (404 Not Found)\n");
	    /* fclose(fp); */
	    close(opened_fd);
	    close(client_fd);
	    print_log(0, "closed connection\n");
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

	print_log(0, "formed response header\n");
	if (HEAD) printf("--- BEGIN HTTP HEADER ---\n%s\n---- END HTTP HEADER ----\n", header2);
	send(client_fd, header2, strlen(header2), 0);
	
	print_log(1, "sent header\n");


	print_log(0, "sending data now\n");
	sendfile(client_fd, opened_fd, 0, length);
	print_log(1, "finished uploading\n");

	fclose(fp);
	close(opened_fd);
	close(client_fd);
	print_log(1, "connection closed\n");
    }
}


void sigint_handler(int s) {
    putchar('\n');
    dbg_print("caught SIGINT\n");
    quit(0);
}

void segfault_handler(int s) {
    dbg_print("caught SIGSEGV\n");
    print_log(3, "error - segmentation fault\n");
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
    print_log(0, "shutting down\n");
    if (fp != NULL) fclose(fp);
    close(opened_fd);
    close(client_fd);
    shutdown(sock, SHUT_RDWR);
    print_log(1, "quitting\n");
    exit(c);
}

void sigpipe_handler(int s) {
    dbg_print("caught SIGPIPE\n");
    print_log(3, "attempted to send data to an invalid client\n");
    if (DEBUG) dump();
    if (fp != NULL) fclose(fp);
    close(opened_fd);
    close(client_fd);
    print_log(1, "cleaned up\n");
}

void sigterm_handler(int s) { quit(2); }

void dbg_print(const char* log) { if (DEBUG) printf("[%s   DEBG   %s] %s", ANSI_COLOR_MAGENTA, ANSI_COLOR_RESET, log); }

void print_log(const unsigned short int t, const char* str) {
    if (SILENT) return;
    switch (t) {
	case 0: printf("[%s   INFO   %s] %s", ANSI_COLOR_BLUE, ANSI_COLOR_RESET, str); break;
	case 1: printf("[%s    OK    %s] %s", ANSI_COLOR_GREEN, ANSI_COLOR_RESET, str); break;
	case 2: printf("[%s   WARN   %s] %s", ANSI_COLOR_YELLOW, ANSI_COLOR_RESET, str); break;
	case 3:	printf("[%s   FAIL   %s] %s", ANSI_COLOR_RED, ANSI_COLOR_RESET, str); break;
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

const char* get_binary(char* argv[]) {
    char bin_name2[256]; strcpy(bin_name2, argv[0]);
    if (strrchr(argv[0], '/') != NULL) { strcpy(bin_name2, strchr(argv[0], '/') + 1); }
    const char* bin_name = bin_name2;
    if (DEV) printf("\nbin_name: %s\n", bin_name);

    return bin_name;
}

bool validate_string(char* str) {
    int i;
    for (i = 0; i < strlen(str); i++)
	if (!isalnum(str[i])) return false;

    return true;
}
