/* 
    Web Proxy
    : Web browser와 End server 사이에서 중개자 역할을 하는 프로그램
    : 브라우저는 프록시에 연결, 프록시가 서버로 대신 연결하여 요청 전달
    : 서버가 프록시에 응답, 프록시가 브라우저로 응답 전달

    1) 방화벽: 브라우저가 프록시를 통해서만 방화벽 너머의 서버에 연결할 수 있음
    2) 익명화: 프록시는 브라우저의 모든 식별 정보를 제거하여 전달
    3) 캐시: 프록시는 서버 개체의 복사본을 저장, 다시 통신하지 않고 캐시에서 읽어 향후 요청에 응답

    Part 1
    : 들어온 연결 수락
    : 요청 분석 및 웹 서버로 전달
    : 응답 분석 및 클라이언트로 전달
*/
#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000      // 최대 캐시 사이즈 1MiB, 메타 데이터 등 불필요한 바이트는 무사
#define MAX_OBJECT_SIZE 102400      // 최대 객체 사이즈 100KiB

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void doit(int connfd);              
void parse_uri(char *uri, char *hostname, char *path, int *port);
void make_header(char *final_header, char *hostname, char *path, rio_t *client_rio);
int server_connection(char *hostname, int port);

int main(int argc, char **argv) {
    int listenfd, connfd;                                      
    char hostname[MAXLINE], port[MAXLINE]; 
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;                         
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }   

    listenfd = Open_listenfd(argv[1]);                  

    while (1) {                                                
        clientlen = sizeof(clientaddr);                          
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
}

// 한 개의 트랜잭션을 수행
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

     // URI를 파싱하여 hostname, port, path를 얻고, 조건에 부합하는 헤더 생성
    parse_uri(uri, hostname, &port, path);
    make_header(server_header, hostname, path, &client_rio);

    // 서버와의 연결 (인라인 함수)
    serverfd = server_connection(hostname, port);

    Rio_readinitb(&server_rio, serverfd);
    Rio_writen(serverfd, server_header, strlen(server_header));

    // 서버로부터 응답을 받고 클라이언트로 보내줌
    size_t response;
    while ((response = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        Rio_writen(connfd, buf, response);
    }

    Close(serverfd);
}

/*
    [HTTP 요청 포트]
    포트가 주어지면 해당 포트로 요청,
    포트가 주어지지 않는다면 기본 포트 80 포트 사용
*/
// parse_uri: URI의 파라미터를 파싱하여, hostname, port, path를 얻는 함수
void parse_uri(char *uri, char *hostname, char *path, int *port) {
    char *first = strstr(uri, "//");

    // 프로토콜 제거
    first = first != NULL ? first + 2 : uri;

    // 포트 - (:) 위치 확인
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

/*
    Request Header는 아래 다섯가지 사항을 포함해야 함
    1) GET / HTTP/1.1
    2) Host: www.cmu.edu (호스트명)
    3) User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3
    4) Connection: close
    5) Proxy-Connection: close
*/
void make_header(char *final_header, char *hostname, char *path, rio_t *client_rio) {
    char buf[MAXLINE];
    char request_header[MAXLINE], host_header[MAXLINE], etc_header[MAXLINE];
    
    sprintf(request_header, "GET %s HTTP/1.0\r\n", path);
    
    while (Rio_readlineb(client_rio, buf, MAXLINE) > 0) {
        // 빈 줄이 들어오면 헤더가 끝났다는 뜻임으로,
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
            // 위 세가지 헤더 이외의 다른 헤더가 요청되었을 때, 따로 저장하여 전달
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

/*
    inline 함수: 호출 시 호출한 자리에서 인라인 함수 코드 자체가 안으로 들어감
    inline 함수로 선언한 이유: 서버와 커넥션하는 동안 다른 간섭을 받지 않기 위해
    
    + 성능 향상: 호출한 곳에 코드가 직접 쓰여진 것과 같은 효과, 함수 호출에 대한 오버헤드가 줄어듬
*/
// server_connection: 서버와 연결을 하기 위한 함수
inline int server_connection(char *hostname, int port){
    char portStr[100];

    // Open_clientfd 함수는 port를 문자 파라미터로 넣어야 함
    sprintf(portStr, "%d", port);

    return Open_clientfd(hostname, portStr);
}