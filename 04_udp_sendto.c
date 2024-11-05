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
    struct addrinfo* peer_addr = 0;
    if (getaddrinfo(
        "127.0.0.1",
        "8080",
        &(struct addrinfo){.ai_socktype=SOCK_DGRAM},
        &peer_addr
    )) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    char addr_buf[100];
    char service_buf[100];
    getnameinfo(
        peer_addr->ai_addr,
        peer_addr->ai_addrlen,
        addr_buf,
        sizeof addr_buf,
        service_buf,
        sizeof service_buf,
        NI_NUMERICSERV | NI_NUMERICHOST
    );
    printf("%s %s\n", addr_buf, service_buf);
    //creating socket
    SOCKET socket_peer = socket(
        peer_addr->ai_family,
        peer_addr->ai_socktype,
        peer_addr->ai_protocol
    );
    if (!IS_VALID_SOCKET(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    char const* message = "Hello world";
    printf("Sending: %s\n", message);
    int bytes_sent = sendto(
        socket_peer,
        message,
        strlen(message),
        0,
        peer_addr->ai_addr,
        peer_addr->ai_addrlen
    );
    printf("Sent %d bytes\n", bytes_sent);
    freeaddrinfo(peer_addr);
    CLOSE_SOCKET(socket_peer);
    #if defined(_WIN32)
        WSACleanup();
    #endif
    return 0;
}
