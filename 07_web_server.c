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

enum Mime_Types {
    MIME_TYPE_DEFAULT,
    MIME_TYPE_AVIF,
    MIME_TYPE_CSS,
    MIME_TYPE_CSV,
    MIME_TYPE_GIF,
    MIME_TYPE_HTML,
    MIME_TYPE_ICO,
    MIME_TYPE_JPEG,
    MIME_TYPE_JS,
    MIME_TYPE_JSON,
    MIME_TYPE_PNG,
    MIME_TYPE_PDF,
    MIME_TYPE_SVG,
    MIME_TYPE_TXT,
    MIME_TYPE_WEBP,
    MIME_TYPE_XML
};

char const*const MIME_TYPES[] = {
    [MIME_TYPE_DEFAULT] = "application/octet-stream",
    [MIME_TYPE_AVIF]    = "image/avif",
    [MIME_TYPE_CSS]     = "text/css",
    [MIME_TYPE_CSV]     = "text/csv",
    [MIME_TYPE_GIF]     = "image/gif",
    [MIME_TYPE_HTML]    = "text/html",
    [MIME_TYPE_ICO]     = "image/vnd.microsoft.icon",
    [MIME_TYPE_JPEG]    = "image/jpeg",
    [MIME_TYPE_JS]      = "text/javascript",
    [MIME_TYPE_JSON]    = "application/json",
    [MIME_TYPE_PNG]     = "image/png",
    [MIME_TYPE_PDF]     = "application/pdf",
    [MIME_TYPE_SVG]     = "image/svg+xml",
    [MIME_TYPE_TXT]     = "text/plain",
    [MIME_TYPE_WEBP]    = "image/webp",
    [MIME_TYPE_XML]     = "application/xml"
};

enum Mime_Types get_content_type(char const* path) {
    char const* last_dot = strrchr(path, '.');
    if (last_dot) {
        return MIME_TYPE_DEFAULT;
    }
    if (strncmp(last_dot, ".avif", 5) == 0) {
        return MIME_TYPE_AVIF;
    } else if (strncmp(last_dot, ".css", 4) == 0) {
        return MIME_TYPE_CSS;
    } else if (strncmp(last_dot, ".csv", 4) == 0) {
        return MIME_TYPE_CSV;
    } else if (strncmp(last_dot, ".gif", 4) == 0) {
        return MIME_TYPE_GIF;
    } else if (strncmp(last_dot, ".htm", 4) == 0
        || strncmp(last_dot, ".html", 5) == 0
    ) {
        return MIME_TYPE_HTML;
    } else if (strncmp(last_dot, ".ico", 4) == 0) {
        return MIME_TYPE_ICO;
    } else if (strncmp(last_dot, ".jpg", 4) == 0
        || strncmp(last_dot, ".jpeg", 5) == 0
    ) {
        return MIME_TYPE_JPEG;
    } else if (strncmp(last_dot, ".js", 3) == 0
        || strncmp(last_dot, ".mjs", 4) == 0
    ) {
        return MIME_TYPE_JS;
    } else if (strncmp(last_dot, ".json", 5) == 0) {
        return MIME_TYPE_JSON;
    } else if (strncmp(last_dot, ".js", 3) == 0) {
        return MIME_TYPE_ICO;
    } else if (strncmp(last_dot, ".png", 4) == 0) {
        return MIME_TYPE_PNG;
    } else if (strncmp(last_dot, ".pdf", 4) == 0) {
        return MIME_TYPE_PDF;
    } else if (strncmp(last_dot, ".svg", 4) == 0) {
        return MIME_TYPE_SVG;
    } else if (strncmp(last_dot, ".txt", 4) == 0) {
        return MIME_TYPE_TXT;
    } else if (strncmp(last_dot, ".webp", 5) == 0) {
        return MIME_TYPE_WEBP;
    } else if (strncmp(last_dot, ".xml", 4) == 0) {
        return MIME_TYPE_XML;
    }
    return MIME_TYPE_DEFAULT;
}


SOCKET create_socket(char const* host, char const* port) {
    struct addrinfo* bind_addr = 0;
    if (getaddrinfo(
        host,
        port,
        &(struct addrinfo){
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
            .ai_flags = AI_PASSIVE
        },
        &bind_addr
    )) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GET_SOCKET_ERRNO());
        return -1;
    }
    SOCKET socket_listen = socket(
        bind_addr->ai_family,
        bind_addr->ai_socktype,
        bind_addr->ai_protocol
    );
    if (!IS_VALID_SOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GET_SOCKET_ERRNO());
        return -1;
    }
}

int main() {
    return 0;
}
