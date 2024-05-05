#include "csapp.h"

void echo(int connfd);
int reopen_listenfd(int *listenfd, int port);

int main(int argc, char **argv) {
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port> \n", argv[0]);
        exit(0);
    }

    listenfd = open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // connfd = reopen_listenfd(listenfd, argv[1]);
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        echo(connfd);
        Close(connfd);
    }
    exit(0);
}

int reopen_listenfd(int *listenfd, int port) {
    struct sockaddr_in serveraddr;

    // 이전에 닫힌 listenfd를 다시 열기 위해 소켓을 생성합니다.
    if ((*listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1; // 소켓 생성 오류
    }

    // 서버 주소를 초기화합니다.
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    // 소켓에 주소를 바인딩합니다.
    if (bind(*listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        close(*listenfd);
        return -1; // 바인딩 오류
    }

    // 소켓을 수신 대기 모드로 설정합니다.
    if (listen(*listenfd, LISTENQ) < 0) {
        close(*listenfd);
        return -1; // listen 오류
    }

    return 0; // 성공
}


void echo(int connfd) {
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("server received %d bytes\n", (int) n);
        printf("Received string: %s\n", buf);
        Rio_writen(connfd, buf, n);
    }
}

// int open_listenfd(char *port) {
//     struct addrinfo hints, *listp, *p;
//     int listenfd, optval = 1;

//     memset(&hints, 0, sizeof(struct addrinfo));
//     hints.ai_socktype = SOCK_STREAM;
//     hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
//     hints.ai_flags = AI_NUMERICSERV;
//     Getaddrinfo(NULL, port, &hints, &listp);

//     for (p = listp; p; p = p->ai_next) {
//         if((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
//             continue;
//         }

//         Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

//         if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
//             break;
//         }
//         Close(listenfd);
//     }

//     Freeaddrinfo(listp);
//     if (!p) {
//         return -1;
//     }
//     if (listen(listenfd, LISTENQ) < 0) {
//         Close(listenfd);
//         return -1;
//     }
//     return listenfd;
// }

