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
#include <ctype.h>
#include <stdbool.h>

#define LOG "\x1b[34m[  LOG  ]\x1b[0m"
#define VERSION "infdev"

/* DEBUG enables debug prints
   VERBOSE enables regular logs
   DUMP prints variables on crash */
bool DEBUG = true;
bool VERBOSE = true;
bool DUMP = true;

void print_log(const char log[]);
void sigint_handler(int s);
void segfault_handler(int s);
void dump();
void quit(int c);
void sigpipe_handler(int s);
char* type(char* ext);
void dbg_print(const char* s);

int sock, client_fd, opened_fd;
FILE* fp = NULL;
char buffer[256] = {0};
unsigned int port = 80;

int main(int argc, char* argv[]) {
    signal(SIGSEGV, segfault_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGPIPE, sigpipe_handler);
    printf("argc: %d\nargv[0]: %s\nargv[1]: %s\nargv[2]: %s\n", argc, argv[0], argv[1], argv[2]);

    printf("This software is provded \"as is\" with absolutely no waranty\nZAWServer - Zawadzki's Anti-Windows (Software) Server %s 2025\nTo quit press \x1b[32mCtrl+C\x1b[0m\n\n", VERSION);
     
    if (argv[1]) {
	for (int i = 0; argv[1][i] != '\0'; i++) {
	    if (!isdigit(argv[1][i])) return 2;
	}
	dbg_print("argument is digits!!");
	port = atoi(argv[1]);
	printf("%s port set to %d\n", LOG, port);
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
   
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = 0;

    print_log("initialized addr");
    int b = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    /* if (b != 0) return 1; */

    printf("%s listening on port %d\n", LOG, port);
    listen(sock, 1);

    while (1) {
	int client_fd = accept(sock, 0, 0);
	print_log("incoming request");

	errno = 0;
	char buffer[256] = {0};
	recv(client_fd, buffer, 256, 0);
	
	if (errno == 104) {
	    print_log("connection reset by peer");
	    close(client_fd);
	    continue;
	}
	
	if ((buffer[0] = 0)) {
	    print_log("buffer empty");
	    close(client_fd);
	    print_log("connection closed");
	    continue;
	}

	/* GET file.txt ... */
	char* file = buffer + 5;
	*strchr(file, ' ') = 0;

	char* ext = strrchr(file, '.') + 1;
	printf(">> %s\n", ext);

	print_log(buffer);

	int opened_fd = open(file, O_RDONLY);

	FILE* fp = fopen(file, "r");
	struct stat st;
	stat(file, &st);
	unsigned int length = st.st_size;

	char guhhhh[16];
	sprintf(guhhhh, "%d", length);
		
        char header[] = "HTTP/1.1 200 OK\r\n""Content-Type: text/raw\r\n""Content-Length: ";
	if (strcmp(ext, ".html")) { char header[] = "HTTP/1.1 200 OK\r\n""Content-Type: text/html\r\n""Content-Length: "; }

        strcat(header, guhhhh);
        strcat(header, "\r\n""Connection: close\r\n""\r\n");

	print_log(header);
	send(client_fd, header, strlen(header), 0);

	sendfile(client_fd, opened_fd, 0, length);
	print_log("sent response");

	fclose(fp);
	close(opened_fd);
	close(client_fd);
	print_log("closed connection");
    }
}

void print_log(const char log[]) { 
    if (VERBOSE)
	printf("\x1b[34m[  LOG  ]\x1b[0m %s\n", log);
}

void sigint_handler(int s) {
    if (VERBOSE) print_log("caught SIGINT");
    quit(0);
}

void segfault_handler(int s) {
    if (DEBUG) print_log("caught SIGSEGV\a");
    if (VERBOSE) print_log("error - segmentation fault");
    if (DUMP) dump();
    /* quit(1); */
    abort();
}

void dump() {
    print_log("dumping variables");
    printf("errno: %d\n", errno);
    printf("fp: ");
    int bf;
    if (fp != NULL) while ((bf = (fgetc(fp))) != EOF) { putchar(bf); }
    putchar('\n');
    printf("sock: %d\nclient_fd: %d\nopened_fd: %d\n", sock, client_fd, opened_fd);
}

void quit(int c) {
    if (fp != NULL) fclose(fp);
    close(opened_fd);
    close(client_fd);
    shutdown(sock, SHUT_RDWR);
    exit(c);
}

void sigpipe_handler(int s) {
    print_log("caught SIGPIPE");
    dump();
    quit(9);
}

char* type(char *ext) {
    if (strcmp(ext, "html")) return "text/html";
    return "text/raw";
} 

void dbg_print(const char* log) {
    if (DEBUG)
	printf("\x1b[31m[  DBG  ]\x1b[0m %s\n", log);
}
