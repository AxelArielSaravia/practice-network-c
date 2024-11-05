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

typedef unsigned char   uchar;
typedef unsigned int    uint;
typedef unsigned long   ulong;


#define TYPE_A       1
#define TYPE_AAAA   28
#define TYPE_MX     15
#define TYPE_TXT    16
#define TYPE_CNAME   5
#define TYPE_ANY   255

void printerr(char const*const msg) {
    fprintf(stderr, "%s - line %d. %s\n", __FILE__, __LINE__, msg);
}

uchar const* print_name(
    uchar const* msg,
    uchar const* p,
    uchar const* end
) {
    //name min 4 bytes
    if (p + 2 > end) {
        printerr("End of message");
        exit(1);
    }
    //check name pointer (0xc0 === 0b1100_0000)
    if ((*p & 0xc0) == 0xc0) {
        //extract the name pointer
        int const k = ((*p & 0x3f) << 8) + p[1];
        p += 2;
        printf(" (pointer %d) ", k);
        print_name(msg, msg+k,end);
        return p;
    } else {
        int const len = *p;
        p += 1;
        if (p + len + 1 > end) {
            printerr("End of message");
            exit(1);
        }
        printf("%.*s",len,p);
        p += len;
        if (*p) {
            printf(".");
            return print_name(msg, p, end);
        } else {
            return p + 1;
        }
    }
}

#define OPCODE_QUERY_LEN 4
char const*const opcode_query[OPCODE_QUERY_LEN] = {
    [0]="standar",
    [1]="reverse",
    [2]="status",
    [3]="?"
};

#define RCODE_QUERY_LEN 7
char const*const rcode_query[RCODE_QUERY_LEN] = {
    [0]="success",
    [1]="format error",
    [2]="server failure",
    [3]="name error",
    [4]="not implemented",
    [5]="refused",
    [6]="?"
};

void print_dns_message(int len, char const message[len]) {
    if (len < 12) {
        printerr("Message is too short to be valid");
        exit(1);
    }
    uchar const* msg = (uchar const*)message;

    /*
    printf("Raw DNS message: \n");
    for (int i = 0; i < len; ++i) {
        unsigned char r = msg[i];
        printf("%02d:   %02x %03d '%c'\n", i, r, r, r);
    }
    printf("---\n\n");
    */
    //HEADER
    printf("ID = %0x %0x\n", msg[0], msg[1]);

    int const qr = (msg[2] & 0x80) >> 7;
    printf("QR = %d ", qr);
    if (qr) {
        printf("response\n");
    } else {
        printf("query\n");
    }

    uint const opcode = (msg[2] & 0x78) >> 3;
    if (opcode < OPCODE_QUERY_LEN - 1) {
        printf("OPCODE = %d %s\n", opcode, opcode_query[opcode]);
    } else {
        printf("OPCODE = %d %s\n", opcode, opcode_query[OPCODE_QUERY_LEN-1]);
    }

    int const aa = (msg[2] & 0x04) >> 2;
    printf("AA = %d ", aa);
    if (aa) {
        printf("authoritative\n");
    } else {
        printf("\n");
    }

    int const tc = (msg[2] & 0x02) >> 1;
    printf("TC = %d ", tc);
    if (tc) {
        printf("truncated\n");
    } else {
        printf("\n");
    }

    int const rd = msg[2] & 0x01;
    printf("RD = %d ", rd);
    if (rd) {
        printf("recursion desired\n");
    } else {
        printf("\n");
    }

    if (qr) {
        //there is an error
        int const rcode = msg[3] & 0x07;

        if (rcode < RCODE_QUERY_LEN-1) {
            printf("RCODE = %d %s\n", rcode, rcode_query[rcode]);
        } else {
            printf("RCODE = %d %s\n", rcode, rcode_query[RCODE_QUERY_LEN-1]);
        }
        if (rcode != 0) {
            return;
        }
    }

    int const qdcount = (msg[4] << 8) + msg[5];
    int const ancount = (msg[6] << 8) + msg[7];
    int const nscount = (msg[8] << 8) + msg[9];
    int const arcount = (msg[10] << 8) + msg[11];
    printf("QDCOUNT = %d\n", qdcount);
    printf("ANCOUNT = %d\n", ancount);
    printf("NSCOUNT = %d\n", nscount);
    printf("ARCOUNT = %d\n", arcount);

    printf("\n");
    unsigned char const* p = msg + 12;
    unsigned char const* end = msg + len;
    if (qdcount) {
        //QUESTION
        for (int i = 0; i < qdcount; i += 1) {
            if (p >= end) {
                printerr("End of message");
                exit(1);
            }
            printf("Query %2d\n", i + 1);
            printf("\tname: ");
            p = print_name(msg, p, end);
            printf("\n");
            if (p + 4 > end) {
                printerr("End of message");
                exit(1);
            }
            int const qtype = (p[0] << 8) + p[1];
            int const qclass = (p[2] << 8) + p[3];
            printf("\nQTYPE = %d\n", qtype);
            printf("\nQCLASS = %d\n", qclass);

            p = p + 4;
        }
    }
    if (ancount || nscount || arcount) {
        for (int i = 0; i < ancount + nscount + arcount; i += 1) {
            if (p >= end) {
                printerr("End of message");
                exit(1);
            }
            printf("Answer: %d\n", i + 1);
            p = print_name(msg, p, end);
            printf("\n");

            if (p + 10 > end) {
                printerr("End of message");
                exit(1);
            }
            int const type = (p[0] << 8) + p[1];
            int const class = (p[2] << 8) + p[3];
            int const ttl = (p[4] << 24) + (p[5] << 16) + (p[6] << 8) + p[7];
            uint const rdlen = (p[8] << 8) + p[9];

            printf("\tTYPE = %d\n", type);
            printf("\tCLASS = %d\n", class);
            printf("\tTTL = %u\n", ttl);
            printf("\tRDLEN = %d\n", rdlen);

            p = p + 10;
            if (p + rdlen > end) {
                printerr("End of message");
                exit(1);
            }


            if (type == TYPE_A && rdlen == 4) {
                printf("Address %d.%d.%d.%d\n", p[0], p[1], p[2], p[3]);
            } else if (type == TYPE_AAAA && rdlen == 28) {
                printf("Address ");
                for (int j = 0; j+1 < rdlen; j += 2) {
                    printf("%02x%02x", p[j], p[j+1]);
                    if (j+2 < rdlen) {
                        printf(":");
                    }
                }
                printf("\n");
            } else if (type == TYPE_MX && rdlen > 3) {
                int const preference = (p[0] << 8) + p[1];
                printf("pref: %d\n", preference);
                printf("MX: ");
                print_name(msg, p+2, end);
                printf("\n");
            } else if (type == TYPE_TXT) {
                printf("TXT: '%.*s'\n", rdlen-1, p+1);
            } else if (type == TYPE_CNAME) {
                printf("CNAME: ");
                print_name(msg, p, end);
                printf("\n");
            }
            p += rdlen;
        }
    }
    if (p != end) {
        printf("There is some unread data left over\n");
    }
    printf("\n");
}

int main(int argc, char* argv[argc]) {
    if (argc < 3) {
        printf("Usage:\n\t%s <hostname> <type>\n", argv[0]);
        printf("\nExample:\n\t%s example.com aaaa\n", argv[0]);
        exit(0);
    }
    if (strlen(argv[1]) > 255) {
        fprintf(stderr, "Hostname too long.");
        exit(1);
    }
    uchar type = 0;


    if (strcmp(argv[2], "a") == 0) {
        type = TYPE_A;
    } else if (strcmp(argv[2], "mx") == 0) {
        type = TYPE_MX;
    } else if (strcmp(argv[2], "txt") == 0) {
        type = TYPE_TXT;
    } else if (strcmp(argv[2], "any") == 0) {
        type = TYPE_ANY;
    } else if (strcmp(argv[2], "aaaa") == 0) {
        type = TYPE_AAAA;
    } else {
        printerr("");
        fprintf(
            stderr,
            "Unknown type '%s'. Use a, aaaa, txt, mx, or any",
            argv[2]
        );
        exit(1);
    }

    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2,2), &d)) {
            printerr("Failed to initialize");
            return 1;
        }
    #endif

    //Configuring remote address
    struct addrinfo* peer_addr = 0;
    if (getaddrinfo(
        "8.8.8.8",
        "53",
        &(struct addrinfo){.ai_socktype=SOCK_DGRAM},
        &peer_addr
    )) {
        printerr("");
        fprintf(stderr,"getaddrinfo() failed (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }
    //creating Socket
    SOCKET socket_peer = socket(
        peer_addr->ai_family,
        peer_addr->ai_socktype,
        peer_addr->ai_protocol
    );
    if (!IS_VALID_SOCKET(socket_peer)) {
        printerr("");
        fprintf(stderr, "socket() failed (%d)\n", GET_SOCKET_ERRNO());
        exit(1);
    }

    #define DNSQ_LEN 1024
    char dns_query[DNSQ_LEN] = {
        /*ID*/          0xab, 0xcd,
        /*Recursion*/   0x01, 0x00,
        /*QDCOUNT*/     0x00, 0x01,
        /*ANCOUNT*/     0x00, 0x00,
        /*NSCOUNT*/     0x00, 0x00,
        /*ARCOUNT*/     0x00, 0x00
    };

    char* hostname = argv[1];
    char* p = dns_query + 12;
    while (*hostname) {
        char* plen = p;
        p = p + 1;
        if (hostname != argv[1]) {
            hostname = hostname + 1;
        }
        while (*hostname && *hostname != '.') {
            *p = *hostname;
            p = p + 1;
            hostname = hostname + 1;
        }
        *plen = p - plen - 1;
    }
    *p = 0;
    p = p + 1;

    //QTYPE
    p[0] = 0x00;
    p[1] = type;
    p = p + 2;

    //QCLASS
    p[0] = 0x00;
    p[1] = 0x01;
    p = p + 2;

    int const query_size = p - dns_query;
    int bytes = sendto(
        socket_peer,
        dns_query,
        query_size,
        0,
        peer_addr->ai_addr,
        peer_addr->ai_addrlen
    );
    printf("Sent %d bytes.\n", bytes);
    print_dns_message(query_size, dns_query);

    char read[1024];
    bytes = recvfrom(socket_peer, read, 1024, 0, 0, 0);
    printf("Received %d bytes\n", bytes);

    print_dns_message(bytes, read);
    printf("\n");

    freeaddrinfo(peer_addr);
    CLOSE_SOCKET(socket_peer);
    #if defined(_WIN32)
        WSACleanup();
    #endif
    return 0;
}
