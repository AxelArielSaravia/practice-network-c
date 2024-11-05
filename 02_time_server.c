#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_SOCKET_ERRNO() (WSAGetLastError())
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>

    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define CLOSE_SOCKET(s) close(s)
    #define GET_SOCKET_ERRNO() (errno)

    typedef int SOCKET;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main() {
    #ifdef _WIN32
        WSADATA d = 0;
        if (WSAStartup(MAKEWORD(2, 2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            exit(1);
        }
    #endif
    //Configuring local address
    struct addrinfo* bind_addr = 0;
    if (getaddrinfo(
        0,
        "8080",
        &(struct addrinfo){
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_flags = AI_PASSIVE,
        },
        &bind_addr
    )) {
        fprintf(stderr, "Failed to getaddrinfo\n");
        exit(1);
    }

    //Creating socket
    SOCKET socket_listen = socket(
        bind_addr->ai_family,
        bind_addr->ai_socktype,
        bind_addr->ai_protocol
    );
    if (!IS_VALID_SOCKET(socket_listen)) {
        fprintf(stderr, "socket() faild. (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }

    //binding socket to local address
    if (bind(socket_listen, bind_addr->ai_addr, bind_addr->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }
    freeaddrinfo(bind_addr);

    //listening
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }

    //waiting for connection
    struct sockaddr_storage client_addr = {0};
    socklen_t client_len = sizeof client_addr;

    SOCKET socket_client = accept(
        socket_listen,
        (struct sockaddr*)&client_addr,
        &client_len
    );
    if (!IS_VALID_SOCKET(socket_client)) {
        fprintf(stderr, "accept() failed. (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }

    //client is connected
    char addr_buf[100] = {0};
    getnameinfo(
        (struct sockaddr*)&client_addr,
        client_len,
        addr_buf,
        sizeof addr_buf,
        0,
        0,
        NI_NUMERICHOST
    );
    printf("%s\n", addr_buf);

    //reading request
    char request[1024];
    int nbytes = recv(socket_client, request, 1024, 0);
    printf("Feceived %d bytes.\n", nbytes);

    //sending response
    char const* response =
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Local time is: ";
    nbytes = send(socket_client, response, strlen(response), 0);
    printf("Send %d of %d bytes.\n", nbytes, (int)strlen(response));

    time_t timer = 0;
    time(&timer);
    char* time_msg = ctime(&timer);
    nbytes = send(socket_client, time_msg, strlen(time_msg), 0);

    printf("Send %d of %d bytes.\n", nbytes, (int)strlen(time_msg));

    //closing connection
    CLOSE_SOCKET(socket_client);

    #ifdef _WIN32
        WSACleanup();
    #endif

    return 0;
}
