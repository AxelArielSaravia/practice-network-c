#include <cctype>
#if defined(_WIN32)
    #if !defined(_WIN32_WINNT)
        #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_SOCKET_ERRNO() (WSAGetLastError())
#else
    //#include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>

    #define SOCKET int
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define CLOSE_SOCKET(s) close(s)
    #define GET_SOCKET_ERRNO() (errno)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>

#define MAX_INPUT 512
#define MAX_RESPONSE 1024

void get_input(char const* prompt, char* buffer) {
    printf("%s", prompt);
    buffer[0] = 0;
    fgets(buffer, MAX_INPUT, stdin);
    int const read = strlen(buffer);
    if (read > 0) {
        buffer[read - 1] = 0;
    }
}

void send_format(SOCKET server, char const* text, ...) {
    char buffer[1024];
    va_list args;
    va_start (args, text);

    vsprintf(buffer, text, args);

    va_end(args);

    send(server, buffer, strlen(buffer), 0);
    printf("C: %s", buffer);
}

int parse_response(char const* response) {
    char const* k = response;
    if (!k[0] || !k[1] || !k[2]) {
        return 0;
    }
    while (k[3]) {
        if ((k == response || k[-1] == '\n')
            && (isdigit(k[0]) && isdigit(k[1]) && isdigit(k[2]))
            && (k[3] != '-')
            && (strstr(k, "\r\n"))
        ) {
            return strtol(k, 0, 10);
        }
        ++k;
    }
    return 0;
}

void wait_on_response(SOCKET server, int expecting) {
    char response[MAX_RESPONSE + 1];
    char* p = response;
    char* end = response + MAX_RESPONSE;
    int code = 0;
    while (1) {
        int bytes_recv = recv(server, p, end-p, 0);
        if (bytes_recv < 1) {
            fprintf(stderr, "Connection dropped.\n");
            exit(1);
        }
        p += bytes_recv;
        *p = 0;
        if (p == end) {
            fprintf(stderr, "Server response too large:\n%s", response);
            exit(1);
        }
        code = parse_response(response);
        if (code != 0) {
            break;
        }
    }
    if (code != expecting) {
        fprintf(stderr, "Error from server:\n%s", response);
        exit(1);
    }
    printf("S: %s", response);
}
