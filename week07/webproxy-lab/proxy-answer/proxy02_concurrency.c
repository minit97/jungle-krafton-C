/* 
    Part 2
    - 여러 동시 연결 처리
*/
#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void *thread(void *vargp);

void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void make_header(char *final_header, char *hostname, char *path, rio_t *client_rio);
int server_connection(char *hostname, int port);

int main(int argc, char **argv) {
    pthread_t tid;  // 여러 개의 동시 연결을 처리하기 위해 스레드 사용

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
  
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        /*
          tid: 스레드 식별자, 스레드마다 식별 번호를 지정해주어야 함
          NULL: 스레드 option 지정, 기본이 NULL
          thread: 스레드가 해야 하는 일을 함수로 만들어, 그 함수명을 지정
          (void *)connfd: 스레드 함수의 매개변수를 지정, 즉 thread((void *)connfd)로 호출한 셈
        */
        // warning 제거를 위해 
        int *connfd_ptr = (int *)malloc(sizeof(int));
        *connfd_ptr = connfd;
        Pthread_create(&tid, NULL, thread, (void *)connfd_ptr);
    }
}

/*
    멀티 스레드: 여러 개의 동시 요청 처리

    동시 서버를 구현하는 가장 간단한 방법은 새 스레드를 생성하는 것
    각 스레드가 새 연결 요청을 처리함

    1) 메모리 누수를 방지하려면 스레드를 분리 모드로 실행
    2) getaddrinfo 함수를 사용하면 thread safe함
*/
void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    free(vargp);

    // 메인 스레드로부터 현재 스레드를 분리시킴
    // 분리시키는 이유: 해당 스레드가 종료되는 즉시 모든 자원을 반납할 것(free)을 보증, 분리하지 않으면 따로 pthread_join(pid)를 호출해야함
    Pthread_detach(Pthread_self());

    // 트랜잭션 수행 후 Connect 소켓 Close
    doit(connfd);
    Close(connfd);

    return NULL;  // warning 제거
}

void doit(int connfd) {
    int serverfd, port;
    char server_header[MAXLINE];
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    rio_t client_rio, server_rio;

    Rio_readinitb(&client_rio, connfd);
    Rio_readlineb(&client_rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {
        printf("Proxy does not implement the method.");
        return;
    }

    parse_uri(uri, hostname, path, &port);
    make_header(server_header, hostname, path, &client_rio);

    serverfd = server_connection(hostname, port);

    Rio_readinitb(&server_rio, serverfd);
    Rio_writen(serverfd, server_header, strlen(server_header));

    size_t response;
    while ((response = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        Rio_writen(connfd, buf, response);
    }

    Close(serverfd);
}

void parse_uri(char *uri, char *hostname, char *path, int *port) {
    char *first = strstr(uri, "//");

    first = first != NULL ? first + 2 : uri;

    char *next = strstr(first, ":");

    *port = 8080;
    if (next) {
        *next = '\0';
        sscanf(first, "%s", hostname);
        sscanf(next + 1, "%d%s", port, path);   // URI에 :이 포함되어 있으면 그 다음부터(next+1) 읽어들임
    } else {                                    // 포트가 없을 때
        next = strstr(first, "/");

        if(next) {                              // path가 있을 때
            *next = '\0';
            sscanf(first, "%s", hostname);

            *next = '/';                        // 다시 NULL을 /로 변경
            sscanf(next, "%s", path);
        } else {                                // path가 없을 때
            sscanf(first, "%s", hostname);
        }
    }
}

void make_header(char *final_header, char *hostname, char *path, rio_t *client_rio) {
    char buf[MAXLINE];
    char request_header[MAXLINE], host_header[MAXLINE], etc_header[MAXLINE];

    sprintf(request_header, "GET %s HTTP/1.0\r\n", path);

    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        if (strcmp(buf, "\r\n")==0) {
            break;
        }

        if (!strncasecmp(buf, "Host", strlen("Host"))) {
            strcpy(host_header, buf);
            continue;
        }

        if (strncasecmp(buf, "User-Agent", strlen("User-Agent"))
         && strncasecmp(buf, "Connection", strlen("Connection"))
         && strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection"))) {
            strcat(etc_header, buf);
        }
    }

    if(strlen(host_header) == 0) {
        sprintf(host_header, "Host: %s\r\n", hostname);
    }

    sprintf(final_header, "%s%s%s%s%s%s%s",
            request_header,
            host_header,
            "Connection: close\r\n",
            "Proxy-Connection: close\r\n",
            user_agent_hdr,
            etc_header,
            "\r\n");
}

inline int server_connection(char *hostname, int port){
    char portStr[100];
    sprintf(portStr, "%d", port);

    return Open_clientfd(hostname, portStr);
}