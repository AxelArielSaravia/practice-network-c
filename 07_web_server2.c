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

char const* get_content_type(char const* path) {
    char const* last_dot = strrchr(path, '.');
    if (last_dot) {
        if (strcmp(last_dot, ".avif") == 0) {
            return "image/avif";
        }
        if (strcmp(last_dot, ".css") == 0) {
            return "text/css";
        }
        if (strcmp(last_dot, ".csv") == 0) {
            return "text/csv";
        }
        if (strcmp(last_dot, ".gif") == 0) {
            return "image/gif";
        }
        if (strcmp(last_dot, ".html") == 0
            || strcmp(last_dot, ".htm") == 0
        ) {
            return "text/html";
        }
        if (strcmp(last_dot, ".ico") == 0) {
            return "image/vnd.microsoft.icon";
        }
        if (strcmp(last_dot, ".jpg") == 0
            || strcmp(last_dot, ".jpeg") == 0
        ) {
            return "image/jpeg";
        }
        if (strcmp(last_dot, ".js") == 0
            || strcmp(last_dot, ".mjs") == 0
        ) {
            return "text/javascript";
        }
        if (strcmp(last_dot, ".json") == 0) {
            return "application/json";
        }
        if (strcmp(last_dot, ".png") == 0) {
            return "image/png";
        }
        if (strcmp(last_dot, ".pdf") == 0) {
            return "application/pdf";
        }
        if (strcmp(last_dot, ".svg") == 0) {
            return "image/svg+xml";
        }
        if (strcmp(last_dot, ".txt") == 0) {
            return "text/plain";
        }
        if (strcmp(last_dot, ".webp") == 0) {
            return  "image/webp";
        }
    }
    return "application/octet-stream";
}

SOCKET listen_tcp(char const* host, char const* port) {
    printf("Configuring local address...\n");
    int blocklog = 10;

    struct addrinfo* bind_addr = 0;
    int r = getaddrinfo(
        host,
        port,
        &(struct addrinfo){
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_flags = AI_PASSIVE
        },
        &bind_addr
    );
    if (r) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }
    SOCKET socket_listen = socket(
        bind_addr->ai_family,
        bind_addr->ai_socktype,
        bind_addr->ai_protocol
    );
    if (!IS_VALID_SOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }
    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_addr->ai_addr, bind_addr->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }
    freeaddrinfo(bind_addr);

    printf("Listening...\n");
    if (listen(socket_listen, blocklog) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }
    return socket_listen;
}

#define MAX_REQUEST_SIZE 2047

struct requests {
char req[MAX_REQUEST_SIZE + 1];
};
struct client_arr {
    unsigned len;
    unsigned cap;
};

struct client_arr clients = {0};
SOCKET* sockets = 0;
struct sockaddr_storage* addresses = 0;
struct requests* requests = 0;
int* received = 0;


#define WSIZE 8
void init_clients() {
    sockets = calloc(WSIZE, sizeof *sockets);
    addresses = calloc(WSIZE, sizeof *addresses);
    requests = calloc(WSIZE, sizeof *requests);
    received = calloc(WSIZE, sizeof *received);
    if (!sockets || !addresses || !requests || !received) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    clients.cap = WSIZE;
}

void resize_clients(unsigned cap) {
        SOCKET* sockets_n = realloc(sockets, (sizeof *sockets) * cap);
        struct sockaddr_storage* addresses_n = realloc(
            addresses,
            (sizeof *addresses) * cap
        );
        struct requests* requests_n = realloc(requests, (sizeof *requests) * cap);
        int* received_n = realloc(received, (sizeof *received) * cap);
        if (!sockets_n || !addresses_n || !requests_n || !received_n) {
            fprintf(stderr, "Out of memory\n");
            exit(1);
        }
        sockets = sockets_n;
        addresses = addresses_n;
        requests = requests_n;
        received = received_n;

}

unsigned new_client() {
    if (clients.len == clients.cap) {
        resize_clients(clients.cap + WSIZE);
        clients.cap = clients.cap + WSIZE;
    }
    clients.len += 1;
    return clients.len - 1;
}

void drop_client(unsigned cli_i) {
    if (cli_i > clients.len || clients.len == 0) {
        fprintf(stderr, "drop_client not found.\n");
        return;
    }

    CLOSE_SOCKET(sockets[cli_i]);
    unsigned const last = clients.len - 1;
    if (cli_i < last) {
        sockets[cli_i] = sockets[last];
        addresses[cli_i] = addresses[last];
        received[cli_i] = received[last];
        memcpy(&requests[cli_i], &requests[last], strlen(requests[last].req));
    }
    sockets[last] = 0;
    addresses[last] = (struct sockaddr_storage){0};
    received[last] = 0;
    requests[last].req[0] = 0;

    clients.len -= 1;

    if (clients.len < clients.cap - (WSIZE * 2)) {
        resize_clients(clients.cap - WSIZE);
        clients.cap = clients.cap - WSIZE;
    }
}

fd_set wait_on_clients(SOCKET server) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(server, &reads);
    SOCKET max_socket = server;
    for (int i = 0; i < clients.len; i += 1) {
        FD_SET(sockets[i], &reads);
        if (sockets[i] > max_socket) {
            max_socket = sockets[i];
        }
    }
    if (select(max_socket + 1, &reads, 0, 0, 0) < 0) {
        fprintf(stderr, "select() failed, (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }
    return reads;
}

char const* get_client_address(unsigned cli_i) {
    if (cli_i > clients.len) {
        fprintf(stderr, "get_client_address() failed, client index error");
        return 0;
    }
    static char addr_buff[100];
    socklen_t addr_len = sizeof addresses[cli_i];
    getnameinfo(
        (struct sockaddr*)&addresses[cli_i],
        addr_len,
        addr_buff,
        sizeof (addr_buff),
        0,
        0,
        NI_NUMERICHOST
    );
    return addr_buff;
}

void send_400(unsigned cli_i) {
    char const* c400 = (
        "HTTP/1.1 400 Bad Request\r\n"
        "Conecction: close\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "Bad Request"
    );
    send(sockets[cli_i], c400, strlen(c400), 0);
    drop_client(cli_i);
}

void send_404(unsigned cli_i) {
    char const* c404 = (
        "HTTP/1.1 404 Not Found\r\n"
        "Conecction: close\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Not Found"
    );
    send(sockets[cli_i], c404, strlen(c404), 0);
    drop_client(cli_i);
}

void serve_resource(char const* path, unsigned cli_i) {
    if (cli_i > clients.len) {
        return;
    }
    printf("serve resource %s %s\n", get_client_address(cli_i), path);
    if (strcmp(path, "/") == 0) {
        path = "/index.html";
    }
    if (strlen(path) > 100) {
        send_400(cli_i);
        return;
    }
    if (strstr(path, "..")) {
        send_404(cli_i);
        return;
    }

    char full_path[128];
    sprintf(full_path, "public%s", path);
    #if defined(_WIN32)
        char* p = full_path;
        while (*p) {
            if (*p == '/') {
                *p = '\\';
            }
            ++p;
        }
    #endif

    FILE* fp = fopen(full_path, "rb");
    if (!fp) {
        send_404(cli_i);
        return;
    }
    fseek(fp, 0l, SEEK_END);
    size_t cl = ftell(fp);
    rewind(fp);

    char const* ct = get_content_type(path);
    #define BSIZE 1024
    char buffer[BSIZE];
    SOCKET socket = sockets[cli_i];
    sprintf(buffer, "HTTP/1.1 200 OK\r\n");
    send(socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Connection: close\r\n");
    send(socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Length: %lu\r\n", cl);
    send(socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Type: %s\r\n", ct);
    send(socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "\r\n");
    send(socket, buffer, strlen(buffer), 0);

    int r = 0;
    while (1) {
        r = fread(buffer, 1, BSIZE, fp);
        if (r) {
    send(socket, buffer, r, 0);
        }
    }
    fclose(fp);
    drop_client(cli_i);
}


int main() {
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2,2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }
    #endif

    SOCKET server = listen_tcp("127.0.0.1", "8080");
    init_clients();

    while (1) {
        fd_set reads = wait_on_clients(server);
        if (FD_ISSET(server, &reads)) {
            unsigned cli_i = new_client();
            socklen_t addr_len = sizeof addresses[0];
            SOCKET socket = accept(
                server,
                (struct sockaddr*)&addresses[cli_i],
                &addr_len
            );
            if (!IS_VALID_SOCKET(socket)) {
                fprintf(stderr, "acceot() failed. (%d)\n", GET_SOCKET_ERRNO());
                return 1;
            }
            sockets[cli_i] = socket;
            printf("New connection from %s.\n", get_client_address(cli_i));
        }

        unsigned i = 0;
        while (i < clients.len) {
            SOCKET socket = sockets[i];

            if (FD_ISSET(socket, &reads)) {
                if (MAX_REQUEST_SIZE == received[i]) {
                    send_400(i);
                    continue;
                }

                int r = recv(
                    socket,
                    requests[i].req + received[i],
                    MAX_REQUEST_SIZE - received[i],
                    0
                );
                if (r < 1) {
                    printf(
                        "Unexpected disconnect from %s.\n",
                        get_client_address(i)
                    );
                    drop_client(i);
                } else {
                    received[i] += r;
                    requests[i].req[received[i]] = 0;
                    char* q = strstr(requests[i].req, "\r\n\r\n");
                    if (q) {
                        if (strncmp("GET /", requests[i].req, 5)) {
                            send_400(i);
                        } else {
                            char* path = requests[i].req + 4;
                            char* end_path = strstr(path, " ");
                            if (!end_path) {
                                send_400(i);
                            } else {
                                *end_path = 0;
                                serve_resource(path, i);

                                i += 1;
                                continue;
                            }
                        }
                    }
                }
            } else {
                i += 1;
            }
        }
    }

    return 0;
}
