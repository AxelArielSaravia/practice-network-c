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


#if defined(_WIN32)
    #include <conio.h>
#endif

int main(int argc, char* argv[argc]) {
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2,2), &d)) {
            fprintf(stderr, "Failed to initialize\n");
            return 1;
        }
    #endif
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        return 1;
    }
    //Configuring remote address
    struct addrinfo* peer_addr = 0;
    if (getaddrinfo(
        argv[1], //hostname
        argv[2], //port
        &(struct addrinfo){.ai_socktype = SOCK_STREAM},
        &peer_addr
    )) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    //Creating socket
    SOCKET socket_peer = socket(
        peer_addr->ai_family,
        peer_addr->ai_socktype,
        peer_addr->ai_protocol
    );
    if (!IS_VALID_SOCKET(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    //Connecting
    if (connect(socket_peer, peer_addr->ai_addr, peer_addr->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", GET_SOCKET_ERRNO());
        return 1;
    }
    freeaddrinfo(peer_addr);
    peer_addr = 0;

    while (1) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);
        #if !defined(_WIN32)
            FD_SET(fileno(stdin), &reads);
        #endif
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 100000,
        };
        if (select(socket_peer+1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GET_SOCKET_ERRNO());
            return 1;
        }
        if (FD_ISSET(socket_peer, &reads)) {
            char read[4096];
            int bytes_received = recv(socket_peer, read, 4096, 0);
            if (bytes_received < 1) {
                printf("Connection closed by peer\n");
                break;
            }
            printf("Received (%d bytes): %.*s", bytes_received, bytes_received, read);
        }
        #if defined(_WIN32)
            #define HAS_INPUT() _kbhit()
        #else
            #define HAS_INPUT() FD_ISSET(0, &reads)
        #endif
        if (HAS_INPUT()) {
            char read[4096];
            if (!fgets(read, 4096, stdin)) {
                break;
            }
            printf("Sending: %s", read);
            int bytes_sent = send(socket_peer, read, strlen(read), 0);
            printf("Sent %d bytes\n", bytes_sent);
        }
        #undef HAS_INPUT
    }
    CLOSE_SOCKET(socket_peer);
    #if defined(_WIN32)
        WSACleanup();
    #endif

    return 0;
}
