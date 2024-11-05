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

int main() {
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2,2), &d)) {
            fprintf(stderr, "Failed to initialize\n");
            return 1;
        }
    #endif
    struct addrinfo* bind_addr = 0;
    if (getaddrinfo(
        0,
        "8080",
        &(struct addrinfo){
            .ai_family = AF_INET,
            .ai_socktype = SOCK_DGRAM,
            .ai_flags = AI_PASSIVE,
        },
        &bind_addr
    )) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    //creating socket
    SOCKET socket_listen = socket(
        bind_addr->ai_family,
        bind_addr->ai_socktype,
        bind_addr->ai_protocol
    );
    if (!IS_VALID_SOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    //binding socket to local address
    if (bind(socket_listen, bind_addr->ai_addr, bind_addr->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    freeaddrinfo(bind_addr);

    struct sockaddr_storage cli_addr;
    socklen_t cli_len = sizeof cli_addr;
    char read[1024];
    int bytes_received = recvfrom(
        socket_listen,
        read,
        1024,
        0,
        (struct sockaddr*)&cli_addr,
        &cli_len
    );
    printf("Received (%d bytes): %.*s\n", bytes_received, bytes_received, read);

    printf("Remote address is: ");
    char addr_buf[100];
    char service_buf[100];
    getnameinfo(
        (struct sockaddr*)&cli_addr,
        cli_len,
        addr_buf,
        sizeof addr_buf,
        service_buf,
        sizeof service_buf,
        NI_NUMERICHOST | NI_NUMERICSERV
    );
    printf("%s %s\n", addr_buf, service_buf);

    CLOSE_SOCKET(socket_listen);
    #if defined(_WIN32)
        WSACleanup();
    #endif
    return 0;
}
