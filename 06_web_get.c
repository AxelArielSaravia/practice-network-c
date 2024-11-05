#if defined(_WIN32)
    #if !defined(_WIN32_WINNT)
        #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    #define IS_VALID_SOCKET(s) ((s) != INVLAID_SOCKET)
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_SOCKET_ERRNO() WSAGetLastError()
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>

    typedef int SOCKET;
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define CLOSE_SOCKET(s) close(s)
    #define GET_SOCKET_ERRNO() (errno)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HC_TIMEOUT 5.0

struct parse_url {
    char const* url;
    char const* hostname;
    size_t hostname_len;
    char const* port;
    size_t port_len;
    char const* path;
    size_t path_len;
};

typedef enum parse_url_return parse_url_return;
enum parse_url_return {
    PU_NONE,
    PU_ERR_PROTOCOL
};

parse_url_return parse_url(struct parse_url* orig) {
    struct parse_url pu = *orig;
    printf("URL: %s\n", pu.url);
    char const* p = strstr(pu.url, "://");

    char const* protocol = 0;
    int protocol_len = 0;
    if (p) {
        protocol = pu.url;
        protocol_len = p - pu.url;
        p += 3;
    } else {
        p = pu.url;
    }
    if (protocol_len) {
        if (strncmp(protocol, "http", 4)) {
            fprintf(
                stderr,
                "Unknown protocol '%.*s'. Only 'http' is suported\n",
                protocol_len,
                protocol
            );
            return PU_ERR_PROTOCOL;
        }
    }
    if (*p == '[') { //IPv6
        p += 1;
        pu.hostname = p;
        for (;;) {
            if (!*p || *p == ']') {
                pu.hostname_len = p - pu.hostname;
                p += 1;
                break;
            }
            p += 1;
        }
    } else {
        pu.hostname = p;
        while (*p && *p != ':' && *p != '/' && *p != '#') {
            p += 1;
        }
        pu.hostname_len = p - pu.hostname;
    }

    if (*p == ':') {
        p += 1;
        pu.port = p;
        while (*p && *p != '/' && *p != '#') {
            p += 1;
        }
        pu.port_len = p - pu.port;
    } else {
        pu.port = "80";
        pu.port_len = 2;
    }

    if (*p == '/') {
        pu.path = p;
        while (*p && *p != '#') {
            p += 1;
        }
        pu.path_len = p - pu.path;
    } else {
        pu.path = "/";
        pu.path_len = 1;
    }

    *orig = pu;

    return PU_NONE;
}

int send_request(SOCKET s, struct parse_url* pu) {
    char buffer[2048];
    int r = sprintf(
        buffer,
        "GET %.*s HTTP/1.1\r\n",
        (int)pu->path_len,
        pu->path
    );
    r += sprintf(
        buffer + r,
        "Host: %.*s:%.*s\r\n",
        (int)pu->hostname_len,
        pu->hostname,
        (int)pu->port_len,
        pu->port
    );
    r += sprintf(buffer + r, "Connection: close\r\n");
    r += sprintf(buffer + r, "User-Agent: honpwc web_get 1.0\r\n");
    r += sprintf(buffer + r, "\r\n");

    int res = send(s, buffer, r, 0);
    printf("Send: \n%.*s", r, buffer);

    return res;
}

/**
 * return -1 if fails
 */
SOCKET connect_to_host(char const* hostname, char const* port) {
    struct addrinfo* peer_addr = 0;
    if (getaddrinfo(
        hostname,
        port,
        &(struct addrinfo){.ai_socktype = SOCK_STREAM},
        &peer_addr
    )) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GET_SOCKET_ERRNO());
        return -1;
    }
    printf("Remote address is: ");
    char addr_buf[100];
    char service_buf[100];
    getnameinfo(
        peer_addr->ai_addr,
        peer_addr->ai_addrlen,
        addr_buf,
        100,
        service_buf,
        100,
        NI_NUMERICHOST
    );
    printf("%s %s\n\n", addr_buf, service_buf);

    SOCKET server = socket(
        peer_addr->ai_family,
        peer_addr->ai_socktype,
        peer_addr->ai_protocol
    );

    if (!IS_VALID_SOCKET(server)) {
        fprintf(stderr, "socket() failed. (%d)\n", GET_SOCKET_ERRNO());
        goto error;
    }
    if (connect(server, peer_addr->ai_addr, peer_addr->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", GET_SOCKET_ERRNO());
        goto error;
    }

    freeaddrinfo(peer_addr);
    return server;

    error:
    freeaddrinfo(peer_addr);
    return -1;
}

enum Encoding {
    ENCODING_LEN,
    ENCODING_CHUNKED,
    ENCODING_CONNECTION
};

int main(int argc, char const* argv[argc]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <url>\n", argv[0]);
        return 1;
    }
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2,2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }
    #endif

    struct parse_url pu = {.url = argv[1]};
    if (parse_url(&pu) != PU_NONE) {
        return 1;
    }
    if (pu.port_len > 253) {
        fprintf(stderr, "ERROR: hostname is greater than 254 chars\n");
        return 1;
    }
    char hostname[254] = {0};
    strncpy(hostname, pu.hostname, pu.hostname_len);

    if (pu.port_len > 4) {
        fprintf(stderr, "ERROR: portnumber is grater than 4\n");
        return 1;
    }
    char port[5] = {0};
    strncpy(port, pu.port, pu.port_len);

    SOCKET server = connect_to_host(hostname, port);

    send_request(server, &pu);

    clock_t const start_time = clock();

    #define HC_RESPONSE_SIZE 8192
    char response[HC_RESPONSE_SIZE+1];
    char* p = response;
    char* end = response + HC_RESPONSE_SIZE;
    char* body = 0;

    enum Encoding encoding = 0;
    int remaining = 0;

    while (1) {
        if ((clock() - start_time) / CLOCKS_PER_SEC > (int)HC_TIMEOUT) {
            fprintf(stderr, "timeout after %.2f seconds\n", HC_TIMEOUT);
            return 1;
        }
        if (p == end) {
            fprintf(stderr, "out of buffer space\n");
            return 1;
        }

        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(server, &reads);

        struct timeval timeout = {.tv_usec = 200000};

        if (select(server + 1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GET_SOCKET_ERRNO());
            return 1;
        }
        if (!FD_ISSET(server, &reads)) {
            continue;
        }
        int bytes_recv = recv(server, p, end - p, 0);
        if (bytes_recv < 1) {
            if (encoding == ENCODING_CONNECTION && body) {
                printf("%.*s", (int)(end - body), body);
            }
            printf("\nConnection closed by peer.\n");
            break;
        }
        p += bytes_recv;
        *p = 0;
        if (!body) {
            body = strstr(response, "\r\n\r\n");
            if (!body) {
                //break??
                continue;
            }
            *body = 0;
            body += 4;
            printf("Received Headers: \n%s\n", response);
            char* q = strstr(response, "\nContent-Length: ");
            if (q) {
                encoding = ENCODING_LEN;
                q = strchr(q, ' ');
                q += 1;
                remaining = strtol(q, 0, 10);
            } else {
                q = strstr(response, "\nTransfer-Encoding: chunked");
                if (q) {
                    encoding = ENCODING_CHUNKED;
                    remaining = 0;
                } else {
                    encoding = ENCODING_CONNECTION;
                }
            }
            printf("\nReceived Body:\n");
        }
        if (body) {
            if (encoding == ENCODING_LEN) {
                if (p - body >= remaining) {
                    printf("%.*s", remaining, body);
                    break;
                }
            } else if (encoding == ENCODING_CHUNKED) {
                for (;;) {
                    if (remaining == 0) {
                        char* q = strstr(body, "\r\n");
                        if (q) {
                            remaining = strtol(body, 0, 16);
                            if (!remaining) {
                                goto finish;
                            }
                            body = q + 2;
                        } else {
                            break;
                        }
                    }
                    if (remaining) {
                        if (p - body >= remaining) {
                            printf("%.*s", remaining, body);
                            body += remaining + 2;
                            remaining = 0;
                        } else {
                            break;
                        }
                    }
                }
            }
        }
    }
    finish:
    CLOSE_SOCKET(server);
    #if defined(_WIN32)
        WSACleanup();
    #endif
    return 0;
}
