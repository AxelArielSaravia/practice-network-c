#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    struct ifaddrs* addresses = {0};
    if (getifaddrs(&addresses)) {
        perror("getifaddrs");
        exit(-1);
    }
    struct ifaddrs* address = addresses;
    while (address) {
        int family = address->ifa_addr->sa_family;
        char ap[100] = {0};
        if (family == AF_INET) {
            getnameinfo(
                address->ifa_addr,
                sizeof (struct sockaddr_in),
                ap,
                sizeof ap,
                0,
                0,
                NI_NUMERICHOST
            );
            printf("%s\t", address->ifa_name);
            printf("IPv4\t");
            printf("\t%s\n", ap);
        } else if (family == AF_INET6) {
            getnameinfo(
                address->ifa_addr,
                sizeof (struct sockaddr_in6),
                ap,
                sizeof ap,
                0,
                0,
                NI_NUMERICHOST
            );
            printf("%s\t", address->ifa_name);
            printf("IPv6\t");
            printf("\t%s\n", ap);
        }
        address = address->ifa_next;
    }
    freeifaddrs(addresses);
    return 0;
}
