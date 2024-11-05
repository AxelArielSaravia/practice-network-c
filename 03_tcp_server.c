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
#include <string.h>

int main() {
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2,2), &d)) {
            fprintf(stderr, "Failed to initialize\n");
            return 1;
        }
    #endif
    //Configuring local address
    struct addrinfo* bind_addr = 0;
    if (getaddrinfo(
        0,      //hostname
        "8080", //port
        &(struct addrinfo){
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_flags = AI_PASSIVE,
        },
        &bind_addr
    )) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    SOCKET socket_listen = socket(
        bind_addr->ai_family,
        bind_addr->ai_socktype,
        bind_addr->ai_protocol
    );
    if (!IS_VALID_SOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    //Binding socket to local address
    if (bind(socket_listen, bind_addr->ai_addr, bind_addr->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    freeaddrinfo(bind_addr);
    //Listening
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    SOCKET max_socket = socket_listen;
    //Waiting for connections
    while (1) {
        fd_set reads = master;
        if (select(max_socket + 1, &reads, 0, 0, 0) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GET_SOCKET_ERRNO());
            return 1;
        }
        for (SOCKET i = 1; i <= max_socket; i += 1) {
            if (FD_ISSET(i, &reads)) {
                if (i == socket_listen) {
                    struct sockaddr_storage cli_addr = {0};
                    socklen_t cli_len = sizeof cli_addr;
                    SOCKET socket_cli = accept(
                        socket_listen,
                        (struct sockaddr*)&cli_addr,
                        &cli_len
                    );
                    if (!IS_VALID_SOCKET(socket_cli)) {
                        fprintf(stderr, "accept() failed. (%d)\n", GET_SOCKET_ERRNO());
                        return 1;
                    }
                    FD_SET(socket_cli, &master);
                    if (socket_cli > max_socket) {
                        max_socket = socket_cli;
                    }
                    char addr_buf[100] = {0};
                    getnameinfo(
                        (struct sockaddr*)&cli_addr,
                        cli_len,
                        addr_buf,
                        sizeof addr_buf,
                        0,
                        0,
                        NI_NUMERICHOST
                    );
                    printf("New connection from %s\n", addr_buf);
                } else {
                    char read[1024] = {0};
                    int bytes_received = recv(i, read, 1024, 0);
                    if (bytes_received < 1) {
                        FD_CLR(i, &master);
                        CLOSE_SOCKET(i);
                        continue;
                    }
                    for (SOCKET j = 1; j <= max_socket; j += 1) {
                        if (FD_ISSET(j, &master)) {
                            if (j == socket_listen || j == i) {
                                continue;
                            } else {
                                send(j, read, bytes_received, 0);
                            }
                        }
                    }
                }
            }
        }
    }
    CLOSE_SOCKET(socket_listen);
    #if defined(_WIN32)
        WSACleanup();
    #endif
    return 0;
}
