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

struct client_info {
    socklen_t address_len;
    struct sockaddr_storage address;
    SOCKET socket;
    char request[MAX_REQUEST_SIZE + 1];
    int received;
    struct client_info* next;
};

static struct client_info* clients = 0;

struct client_info* get_client(SOCKET s) {
    struct client_info* ci = clients;
    while (1) {
        if (!ci) {
            break;
        }
        if (ci->socket == s) {
            return ci;
        }
        ci = ci->next;
    }
    struct client_info* n = calloc(1, sizeof *n);
    if (!n) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }
    *n = (struct client_info){
        .address_len = sizeof n->address,
        .next = clients,
    };
    clients = n;
    return n;
}

void drop_client(struct client_info* client) {
    CLOSE_SOCKET(client->socket);
    struct client_info** p = &clients;
    while (*p) {
        if (*p == client) {
            *p = client->next;
            free(client);
            return;
        }
        p = &(*p)->next;
    }
    fprintf(stderr, "drop_client not found.\n");
    exit(1);
}

char const* get_client_address(struct client_info* ci) {
    static char addr_buff[100];
    getnameinfo(
        (struct sockaddr*)&ci->address,
        ci->address_len,
        addr_buff,
        sizeof (addr_buff),
        0,
        0,
        NI_NUMERICHOST
    );
    return addr_buff;
}

fd_set wait_on_clients(SOCKET server) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(server, &reads);
    SOCKET max_socket = server;
    struct client_info* ci = clients;
    while (ci) {
        FD_SET(ci->socket, &reads);
        if (ci->socket > max_socket) {
            max_socket = ci->socket;
        }
        ci = ci->next;
    }
    if (select(max_socket + 1, &reads, 0, 0, 0) < 0) {
        fprintf(stderr, "select() fauled, (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }
    return reads;
}

void send_400(struct client_info* client) {
    char const* c400 = (
        "HTTP/1.1 400 Bad Request\r\n"
        "Conecction: close\r\n"
        "Content-Length: 11\r\n"
        "\r\n"
        "Bad Request"
    );
    send(client->socket, c400, strlen(c400), 0);
    drop_client(client);
}

void send_404(struct client_info* client) {
    char const* c404 = (
        "HTTP/1.1 404 Not Found\r\n"
        "Conecction: close\r\n"
        "Content-Length: 9\r\n"
        "\r\n"
        "Not Found"
    );
    send(client->socket, c404, strlen(c404), 0);
    drop_client(client);
}

void serve_resource(struct client_info* client, char const* path) {
    printf("serve resource %s %s\n", get_client_address(client), path);
    if (strcmp(path, "/") == 0) {
        path = "/index.html";
    }
    if (strlen(path) > 100) {
        send_400(client);
        return;
    }
    if (strstr(path, "..")) {
        send_404(client);
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
        send_404(client);
        return;
    }
    fseek(fp, 0l, SEEK_END);
    size_t cl = ftell(fp);
    rewind(fp);

    char const* ct = get_content_type(full_path);

    #define BSIZE 1024
    char buffer[BSIZE];
    sprintf(buffer, "HTTP/1.1 200 OK\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Connection: close\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Length: %lu\r\n", cl);
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Type: %s\r\n", ct);
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    int r = 0;
    while (1) {
        r = fread(buffer, 1, BSIZE, fp);
        if (r) {
            send(client->socket, buffer, r, 0);
        }
    }
    fclose(fp);
    drop_client(client);
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
    while (1) {
        fd_set reads = wait_on_clients(server);
        if (FD_ISSET(server, &reads)) {
            struct client_info* client = get_client(-1);
            SOCKET socket = accept(
                server,
                (struct sockaddr*)&(client->address),
                &(client->address_len)
            );
            if (!IS_VALID_SOCKET(socket)) {
                fprintf(stderr, "accept() failed. (%d)\n", GET_SOCKET_ERRNO());
                return 1;
            }
            client->socket = socket;
            printf("New connection from %s.\n", get_client_address(client));
        }
        struct client_info* client = clients;
        while (client) {
            struct client_info* next = client->next;
            if (FD_ISSET(client->socket, &reads)) {
                if (MAX_REQUEST_SIZE == client->received) {
                    send_400(client);
                    continue;
                }
                int r = recv(
                    client->socket,
                    client->request + client->received,
                    MAX_REQUEST_SIZE - client->received,
                    0
                );
                if (r < 1) {
                    printf(
                        "Unexpected disconnect from %s.\n",
                        get_client_address(client)
                    );
                    drop_client(client);
                } else {
                    client->received += r;
                    client->request[client->received] = 0;
                    char* q = strstr(client->request, "\r\n\r\n");
                    if (q) {
                        if (strncmp("GET /", client->request, 5)) {
                            send_400(client);
                        } else {
                            char* path = client->request + 4;
                            char* end_path = strstr(path, " ");
                            if (!end_path) {
                                send_400(client);
                            } else {
                                *end_path = 0;
                                serve_resource(client, path);
                            }
                        }
                    }
                }
            }
            client = next;
        }
    }
    printf("\nClosing socket...\n");
    #if defined(_WIN32)
        WSACleanup();
    #endif

    printf("Finished.\n");
    return 0;
}
