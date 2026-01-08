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

#define VERSION "alpha-0.1.1"

extern const char* __progname;

/* DEBUG enables debug prints
   SILENT hides all logs (excluding debug)
   DEV is to be used for extra debug logs (only in devel) - deprecated, use DEBUG
   HEAD displays headers
   REDIRECTS enables redirects support
   BLOCKS enables managing blocked connections */
bool DEBUG = false;
bool SILENT = false;
bool HEAD = true;
bool COLOR = true;
bool REDIRECTS = true;
bool BLOCKS = true;

#define BLOCKS_FILE "blocks.conf"

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
int chkblock(char* ua, char* ip);
bool blocker(char* ua, char* ip, int* peer);
void genblocklist();

int sock, client_fd, opened_fd;
FILE* fp = NULL;
char buffer[1024] = {};
int port = 8080;
bool validPort = false;
int validated;
const int one = 1; /* yes */
const char conf_file[] = "server.conf";
bool cfg_found = false;
char* cfg_buffer;
bool restart = false, frk = false, colors = true;

struct redirect {
    char* source;
    char* destination;
};


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
	else if (strcmp(argument, "--noblock") == 0) BLOCKS = false;
	else if (strcmp(argument, "--genblock") == 0) { genblocklist(); return 0; }
	else if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
	    printf("valid arguments:\n");
	    printf(" -h, --help	this message\n");
	    printf(" <port>         self explanatory\n");
	    printf(" -d		debug\n");
	    printf(" -s		silent\n");
	    printf(" -v		version/build info\n");
	    printf(" --nocolor	removes all colors from output\n");
	    printf(" --noheader	don't display any headers\n");
	    printf(" --noblock      disables all blocking rules\n");
	    printf(" --genblock     override blocklist with default (create if doesn't exist)\n");
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

	char* peer_ua = strstr(buffer, "User-Agent: ");
	if (peer_ua != NULL) {
	    peer_ua = peer_ua+12;
	    *strchr(peer_ua, '\r') = 0;
	    dbg_print("grabbed useragent\n");
	    dbg_print("peer useragent: ");
	    if (DEBUG) printf("%s\n", peer_ua);
	}

	if (BLOCKS) {
	    print_log(0, "checking blocklist\n");
	    if (blocker(peer_ua, peer_name, &client_fd)) {
		print_log(0, "connection closed\n");
		continue;
	    }
	}

	bool req_HEAD = false;
	if (strncmp(buffer, "GET", 3) != 0) {
	    dbg_print("request is not GET, checking for other supported\n");
	    if (strncmp(buffer, "HEAD", 4) == 0) {
		print_log(0, "request is HEAD\n");
		req_HEAD = true;
	    }
	    else if (strncmp(buffer, "OPTIONS", 7) == 0) {
		print_log(0, "request is OPTIONS\n");
		char options[1024] = "HTTP/1.1 200 OK\nAllow: GET, HEAD, OPTIONS, TRACE\nServer: zawserver (";
		strcat(options, VERSION);
		strcat(options, ")\n\r\n");

		if (HEAD) printf("--- BEGIN HTTP HEADER ---\n%s\n---- END HTTP HEADER ----\n", options);
		send(client_fd, options, strlen(options), 0);
		print_log(0, "sent respone\n");
		close(client_fd);
		print_log(0, "connection closed\n");
		continue;
		
	    }
	    else if (strncmp(buffer, "TRACE", 5) == 0) {
		print_log(0, "request is TRACE\n");

		char header[1024] = "HTTP/1.1 200 OK\nContent-Length: ";
		char len[8] = {};
		sprintf(len, "%d", (int)strlen(buffer));
		strcat(header, len);
		strcat(header, "\n\r\n");
		strcat(header, buffer);

		if (HEAD) printf("--- BEGIN HTTP HEADER ---\n%s\n---- END HTTP HEADER ----\n", header);
		send(client_fd, header, strlen(header), 0);
		print_log(0, "sent response\n");
		close(client_fd);
		print_log(0, "connection closed\n");
		continue;

	    }
	    else if (strncmp(buffer, "POST", 4) == 0 || strncmp(buffer, "PUT", 3) == 0 || strncmp(buffer, "DELETE", 6) == 0 || strncmp(buffer, "CONNECT", 7) == 0 || strncmp(buffer, "PATCH", 5) == 0) {
		print_log(2, "method was found, but is not supported\n");
		print_log(0, "throwing 501 Not Implemented\n");
		send(client_fd, "HTTP/1.1 501 Not Implemented\r\n", 30, 0);
		close(client_fd);
		print_log(0, "connection closed\n");
		continue;
	    }
	    else {
		/* it's unlikely this will ever happen but it's here just in case */
		print_log(2, "no valid request method from buffer\n");
		print_log(0, "throwing 400 Bad Request\n");
		send(client_fd, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
		close(client_fd);
		print_log(0, "connection closed\n");
		continue;
	    }

	}
	else print_log(0, "request is GET\n");



	char* file = buffer + 5;
	if (req_HEAD) file = buffer + 6;
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
	

	char length[16];
	sprintf(length, "%d", (int)st.st_size);

	char header[] = "HTTP/1.1 200 OK\nContent-Type: ";
	strcat(header, get_type(ext));
	strcat(header, "\nContent-Length: ");
	strcat(header, length);
	strcat(header, "\nConnection: close\r\n\r\n");

	print_log(0, "formed response header\n");
	if (HEAD) printf("--- BEGIN HTTP HEADER ---\n%s\n---- END HTTP HEADER ----\n", header);
	send(client_fd, header, strlen(header), 0);
	print_log(0, "sent header\n");

	if (!req_HEAD) {
	print_log(0, "sending data now\n");
	errno = 0;
	int c = sendfile(client_fd, opened_fd, 0, st.st_size);
	print_log(0, "finished uploading\n");
	if (DEBUG) { dbg_print("uploading returned "); printf("%d, errno: %d\n", c, errno); }
	}

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

int chkblock(char* ua, char* ip) {

    if (ua == NULL || ip == NULL) {
	print_log(2, "at least one argument was null\n");
	return -1;
    }

    int fd = open(BLOCKS_FILE, O_RDONLY);
    if (fd == -1) {
	print_log(1, "blocklist file is invalid\n");
	return -1;
    }

    struct stat st;
    stat(BLOCKS_FILE, &st);
    if (!S_ISREG(st.st_mode)) {
	print_log(1, "blocklist file exists, but is wrong type\n");
	return -1;
    } else {
	FILE* blocks = fopen(BLOCKS_FILE, "r");
	if (blocks == NULL) {
	    print_log(1, "failed to open blocklist file. possibly missing permissions\n");
	    return -1;
	}

	char* buffer = malloc(st.st_size);
	if (buffer == NULL || errno == ENOMEM) {
	    print_log(2, "out of memory\n");
	    close(fd);
	    fclose(blocks);
	    abort();
	}

	fgets(buffer, st.st_size, blocks);
	while (strncmp(buffer, "#", 1) == 0)
	    fgets(buffer, st.st_size, blocks);

	int response;
	if (strncmp(buffer, "response: ", 10) == 0) {
	    dbg_print("response: ");
	    if (DEBUG) printf("%s\n", buffer+10);

	    if (strncmp(buffer+10, "drop", 4) == 0) response = 1;
	    else if (strncmp(buffer+10, "400", 3) == 0) response = 400;
	    else if (strncmp(buffer+10, "401", 3) == 0) response = 401;
	    else if (strncmp(buffer+10, "403", 3) == 0) response = 403;
	    else if (strncmp(buffer+10, "404", 3) == 0) response = 404;
	    else if (strncmp(buffer+10, "410", 3) == 0) response = 410;
	    else if (strncmp(buffer+10, "418", 3) == 0) response = 418;
	    else if (strncmp(buffer+10, "451", 3) == 0) response = 451;
	} else {
	    print_log(2, "failed parsing list - reponse must appear before the list\n");
	    close(fd);
	    fclose(blocks);
	    free(buffer);
	    return -1;
	}

	bool whitelist;
	do fgets(buffer, st.st_size, blocks);
	while (strncmp(buffer, "whitelist: ", 11) == 0);

	if (*buffer == EOF) {
	    print_log(2, "failed parsing list - 'whitelist' must appear after 'response' and before the list\n");
	    close(fd);
	    fclose(blocks);
	    free(buffer);
	    return -1;
	} else {
	    if (strncmp(buffer+11, "1", 1) == 0) whitelist = true;
	    else if (strncmp(buffer+11, "0", 1) == 0) whitelist = false;
	    else {
		dbg_print("whitelist: ");
		if (DEBUG) printf("%s\n", buffer+11);
		print_log(2, "failed parsing list - 'whitelist' can equal only 1 or 0\n");
		close(fd);
		fclose(blocks);
		free(buffer);
		return -1;
	    }
	}

	

	if (whitelist) print_log(0, "whitelist mode is enabled on blocklist\n");

	/* save cpu cycles by using a constant instead of getting the length every time */
	const size_t ua_len = strlen(ua), ip_len = strlen(ip);

	while (fgets(buffer, st.st_size, blocks)) {
	    if (*buffer == EOF) {
		dbg_print("blocklist is valid, but there are no blocks defined\n");
		close(fd);
		fclose(blocks);
		free(buffer);
		return 0;
	    }
	    if (whitelist) {
		    if (strncmp(buffer, ua, ua_len) == 0 || strncmp(buffer, ip, ip_len) == 0) {
			close(fd);
			fclose(blocks);
			free(buffer);
			return 0;
		    }
		    return response;
	    } else {
		if ((strncmp(buffer, ua, ua_len) == 0) || strncmp(buffer, ip, ip_len) == 0) {
		    close(fd);
		    fclose(blocks);
		    free(buffer);
		    return response;
		}
	    }
	}
	close(fd);
	fclose(blocks);
	free(buffer);
    }
    
    return 0;
}

bool blocker(char* ua, char* ip, int* peer) {
    int response = chkblock(ua, ip);

    if (response == -1) {
	print_log(2, "blocklist check returned error\n");
	return false;
    } else if (response == 0)
	return false;
    else if (response == 1) {
	print_log(0, "peer is in blocklist and the request is configured to be dropped\n");
	close(*peer);
	return true;
    } else
	print_log(1, "peer was found to be in the blocklist\n");

    errno = 0;
    switch (response) {
	case 400:
	    print_log(0, "throwing 400 Bad Request\n");
	    send(*peer, "HTTP/1.1 400 Bad Request\r\n", 26, 0);
	    break;
	case 401:
	    print_log(0, "throwing 401 Unauthorized\n");
	    send(*peer, "HTTP/1.1 401 Unauthorized\r\n", 27, 0);
	    break;
	case 403:
	    print_log(0, "throwing 403 Forbidden\n");
	    send(*peer, "HTTP/1.1 403 Forbidden\r\n", 24, 0);
	    break;
	case 404:
	    print_log(0, "throwing 404 Not Found\n");
	    send(*peer, "HTTP/1.1 404 Not Found\r\n", 24, 0);
	    break;
	case 410:
	    print_log(0, "throwing 410 Gone\n");
	    send(*peer, "HTTP/1.1 410 Gone\r\n", 19, 0);
	    break;
	case 418:
	    print_log(0, "throwing 418 I'm a Teapot\n");
	    send(*peer, "HTTP/1.1 418 I'm a Teapot\r\n", 27, 0);
	    break;
	case 451:
	    print_log(0, "throwing 451 Unavailable for Legal Reasons\n");
	    send(*peer, "HTTP/1.1 451 Unavailable for Legal Reasons\r\n", 44, 0);
	    break;
	case 1:
	    print_log(0, "dropping connection\n");
	    break;
    }

    dbg_print("errno: ");
    if (DEBUG) printf("%d\n", errno);

    close(*peer);

    return true;
}

void genblocklist() {
    const char* defaultconf = "# lines starting with '#' are ignored\n# available responses: drop, 400, 401, 403, 404, 410, 418, 451\nresponse: 418\nwhitelist: 0\n\ncurl/8.17.0\n";

    FILE* new = fopen(BLOCKS_FILE, "w");
    fputs(defaultconf, new);
    print_log(0, "new file should be located in the directory\n");

    fclose(new);
}
