#include "csapp.h"

int main(int argc, char **argv) {
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port> \n", argv[0]);
        exit(0);
    }
    host = argv[1];
    port = argv[2];

    clientfd = open_clientfd(host, port);
    Rio_readinitb(&rio, clientfd);                  // 버퍼 I/O 위한 초기화

    while (Fgets(buf, MAXLINE, stdin) != NULL) {    // 표준 입력 스트림에서 문자열을 읽음
        Rio_writen(clientfd, buf, strlen(buf));     // 버퍼 I/O 사용하여 데이터 쓰기 (버퍼에서 데이터를 읽어 파일에 쓰기 수행)

        Rio_readlineb(&rio, buf, MAXLINE);          // 버퍼 I/O 사용하여 데이터 읽기
        Fputs(buf, stdout);                         // 표준 출력 스트림에 문자열을 출력
    }
    Close(clientfd);
    exit(0);
}