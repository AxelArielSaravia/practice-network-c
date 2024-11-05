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

#if !defined(AI_ALL)
    #define AI_ALL 0x0100
#endif

int main(int argc, char const* argv[argc]) {
    if (argc < 2) {
        printf("Usage: %s <hostname>\n", argv[0]);
        exit(0);
    }
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2,2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }
    #endif

    char const*const hostname = argv[1];
    printf("Resolving hostname '%s'\n", hostname);

    struct addrinfo* peer_addr = 0;
    if (getaddrinfo(
        hostname,
        0,
        &(struct addrinfo){.ai_flags=AI_ALL},
        &peer_addr
    )) {
        fprintf(stderr, "getaddrinfo() faild. (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }
    printf("Remote address is: \n");
    {
        struct addrinfo const* addr = peer_addr;
        char addr_buf[100];
        while (addr) {
            getnameinfo(
                addr->ai_addr,
                addr->ai_addrlen,
                addr_buf,
                100,
                0,
                0,
                NI_NUMERICHOST
            );
            printf("\t%s\n", addr_buf);
            addr = addr->ai_next;
        }
    }
    freeaddrinfo(peer_addr);
    #if defined(_WIN32)
        WSACleanup();
    #endif
    return 0;
}
