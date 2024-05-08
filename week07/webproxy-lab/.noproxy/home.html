/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

#define original

void doit(int fd);                                                                    // 클라이언트의 HTTP 요청을 처리하는 함수
void read_requesthdrs(rio_t *rp);                                                     // HTTP 요청 헤더를 읽고 출력
int parse_uri(char *uri, char *filename, char *cgiargs);                              // URL을 분석하여 정적 컨텐츠를 위한 파일 이름과 동적 컨텐츠를 위한 CGI 인자를 추출
void serve_static(int fd, char *filename, int filesize, char *method);                              // 정적 컨텐츠를 클라이언트에게 제공
void get_filetype(char *filename, char *filetype);                                    // 파일의 확장자를 보고 MIME 타입을 설정
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);              // CGI를 이용해서 클라이언트에게 동적 컨텐츠를 제공
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);   // 클라이언트가 잘못된 요청을 보냈을 때 클라이언트에게 오류 메세지 전송

int main(int argc, char **argv) {
  
  int listenfd, connfd;                                           // 서버의 리스닝 소켓과 연결 소켓을 위한 파일 디스크립터
  char hostname[MAXLINE], port[MAXLINE];                          // 클라이언트의 호스트네임과 포트 번호를 저장할 배열
  socklen_t clientlen;                                            // 클라이언트 주소 구조체의 크기
  struct sockaddr_storage clientaddr;                             // 클라이언트의 주소 정보를 저장할 구조체

  if (argc != 2) {
    // 프로그램 인자로 포트 번호를 받지 않으면 에러 메시지 출력 후 종료
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);                                // 입력받은 포트 번호로 리스닝 소켓을 생성하고 파일 디스크립터 반환
  while (1) {                                                       // 무한 루프로 클라이언트의 연결 요청을 계속해서 수락
    clientlen = sizeof(clientaddr);                                 // 클라이언트 주소 구조체의 크기 설정
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);       // 클라이언트의 연결 요청을 수락하고, 연결 소켓의 파일 디스크립터 반환
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);   // 클라이언트의 연결 요청을 수락하고, 연결 소켓의 파일 디스크립터 반환
    printf("Accepted connection from (%s, %s)\n", hostname, port);  // 클라이언트의 호스트네임과 포트 번호를 출력
    doit(connfd);
    Close(connfd);
  }
}

void doit(int fd) {
    int is_static;                                                          // 요청이 정적인지 동적인지 판단하는 변수
    struct stat sbuf;                                                       // 파일의 상태 정보를 저장하는 구조체
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];     // 요청 라인과 URI 파싱을 위한 문자열 버퍼들
    char filename[MAXLINE], cgiargs[MAXLINE];                               // 정적 파일의 이름과 CGI 인자를 저장하는 버퍼
    rio_t rio;                                                              // Robust I/O의 rio 구조체
    
    Rio_readinitb(&rio, fd);                          // 요청 라인과 헤더를 읽습니다, rio 구조체를 초기화
    Rio_readlineb(&rio, buf, MAXLINE);                // 요청 라인을 읽어 buf에 저장
    printf("Request headers:\n");                     // 요청 헤더 출력 시작을 알림
    printf("%s", buf);                                // 요청 라인 출력
    sscanf(buf, "%s %s %s", method, uri, version);    // buf에서 HTTP 메소드, URI, 버전을 파싱
    if (strcasecmp(method, "GET")) {                  // HTTP 메소드가 GET이 아닌 경우 오류 처리
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }
    
    read_requesthdrs(&rio);                           // 나머지 요청 헤더를 읽음
    is_static = parse_uri(uri, filename, cgiargs);    // GET 요청의 URI를 파싱, URI를 분석하여 정적/동적 요청을 판단하고 필요한 정보를 추출
    if (stat(filename, &sbuf) < 0) {                  // 파일 상태를 확인하여 존재하지 않는 경우 오류 처리
        clienterror(fd, filename, "404", "Not found", "Tiny couldn’t find this file");
        return;
    }
    
    // 정적 컨텐츠 처리
    if (is_static) {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 파일이 일반 파일이 아니거나 읽기 권한이 없는 경우 오류 처리
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size, method); // 정적 파일 제공
    }
    // 동적 컨텐츠 처리
    else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 파일이 일반 파일이 아니거나 실행 권한이 없는 경우 오류 처리
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs, method); // 동적 컨텐츠 제공
    }
}

// 클라이언트에게 에러 메시지를 보내는 함수 정의
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];                                  // HTTP 응답을 위한 버퍼 선언

    /* HTTP 응답 바디 구성 시작 */
    sprintf(body, "<html><title>Tiny Error</title>");                 // HTML 문서의 시작과 타이틀 설정
    sprintf(body, "%s<body bgcolor=\"ffffff\">\r\n", body);           // 바디의 배경색 설정
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);            // 에러 번호와 짧은 메시지 추가
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);           // 긴 메시지와 에러 원인 추가
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);    // 웹 서버 이름과 수평선 추가

    /* HTTP 응답 보내기 시작 */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);             // HTTP 상태 줄 구성
    Rio_writen(fd, buf, strlen(buf));                                 // 상태 줄 클라이언트에 전송
    sprintf(buf, "Content-type: text/html\r\n");                      // 콘텐츠타입 헤더 설정
    Rio_writen(fd, buf, strlen(buf));                                 // 콘텐츠타입 헤더 전송
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));    // 콘텐츠 길이 헤더 설정
    Rio_writen(fd, buf, strlen(buf));                                 // 콘텐츠 길이 헤더 전송
    Rio_writen(fd, body, strlen(body));                               // 실제 HTML 바디 전송
}


void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];                          // 최대 길이가 MAXLINE인 문자 배열 buf 선언
    Rio_readlineb(rp, buf, MAXLINE);            // rp를 통해 한 줄을 읽어 buf에 저장. 첫 번째 헤더 라인을 읽음.
    while(strcmp(buf, "\r\n")) {                // buf가 "\r\n" (헤더의 끝을 나타내는 빈 줄)이 아닐 때까지 반복
        Rio_readlineb(rp, buf, MAXLINE);        // 다음 헤더 라인을 읽어 buf에 저장
        printf("%s", buf);                      // buf에 저장된 헤더 라인을 출력
    }
    return;
}


/* URI를 파싱하여 파일 이름과 CGI 인자를 추출하는 함수 */
int parse_uri(char *uri, char *filename, char *cgiargs) {
    char *ptr;
    /* 정적 컨텐츠의 경우 */
    if (!strstr(uri, "cgi-bin")) {
        strcpy(cgiargs, "");                // CGI 인자를 비움
        strcpy(filename, ".");              // 파일 이름의 시작을 현재 디렉토리로 지정  
        strcat(filename, uri);              // URI를 파일 이름에 추가
        if (uri[strlen(uri)-1] == '/') {    // URI가 '/'로 끝나면
          strcat(filename, "home.html");    // 기본 파일 이름을 추가
        }
        return 1;                           // 정적 컨텐츠를 나타내는 1 반환
    }
    /* 동적 컨텐츠의 경우 */
    else {
        ptr = index(uri, '?');              // URI에서 '?' 위치를 찾음
        if (ptr) {
          strcpy(cgiargs, ptr+1);           // '?' 이후 문자열을 CGI 인자로 복사
          *ptr = '\0';                      // URI를 '?' 위치에서 분리
        } else {
          strcpy(cgiargs, "");              // CGI 인자를 비움
        }
        strcpy(filename, ".");              // 파일 이름의 시작을 현재 디렉토리로 지정
        strcat(filename, uri);              // 변경된 URI를 파일 이름에 추가
        return 0;                           // 동적 컨텐츠를 나타내는 0 반환
    }
}

void serve_static(int fd, char *filename, int filesize, char *method) {
    /* 변수 선언: 파일 디스크립터, 메모리 매핑 포인터, 파일 타입, 버퍼 */
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* 클라이언트에게 응답 헤더 전송 */
    get_filetype(filename, filetype);                               // 파일 이름으로부터 파일 타입 결정
    sprintf(buf, "HTTP/1.0 200 OK\r\n");                            // 상태 줄
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);             // 서버 헤더
    sprintf(buf, "%sConnection: close\r\n", buf);                   // 연결 헤더
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);        // 컨텐츠 길이 헤더
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);      // 컨텐츠 타입 헤더
    Rio_writen(fd, buf, strlen(buf));                               // 헤더 버퍼를 클라이언트에게 전송
    printf("Response headers:\n");
    printf("%s", buf);                                              // 응답 헤더 출력

  /* HTTP HEAD 메소드 처리 */
  if (strcasecmp(method, "HEAD") == 0) {
      return;     // 응답 바디를 전송하지 않음
  }

  /* 클라이언트에게 응답 바디 전송 */
  #ifdef original
    srcfd = Open(filename, O_RDONLY, 0);                            // 파일 오픈
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);     // 파일을 메모리에 매핑
    Close(srcfd);                                                   // 파일 디스크립터 닫기
    Rio_writen(fd, srcp, filesize);                                 // 메모리 매핑된 데이터를 클라이언트에게 전송
    Munmap(srcp, filesize);                                         // 메모리 매핑 해제
  #else
    srcfd = Open(filename, O_RDONLY, 0);
    
    // solved problem 11.9
    srcp = malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);                         // 파일 디스크립터 닫기
    Rio_writen(fd, srcp, filesize);
    free(srcp);
  #endif

}

/* get_filetype - 파일 이름으로부터 파일 타입 결정 */
void get_filetype(char *filename, char *filetype) {
    /* 파일 확장자로 파일 타입 결정 */
  if(strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if(strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if(strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  } else if(strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpg");
  } else if(strstr(filename, ".mp4")) {
    strcpy(filetype, "video/mp4");
  } else if(strstr(filename, ".mpg")) {
    strcpy(filetype, "video/mpeg");
  } else {
    strcpy(filetype, "text/plain");
  }
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) {
    char buf[MAXLINE], *emptylist[] = { NULL };           // 버퍼 선언 및 CGI 프로그램에 전달할 빈 인자 목록 초기화

    /* 클라이언트에게 HTTP 응답의 첫 부분을 반환 */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));                     // 상태 줄을 클라이언트에게 보냄
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));                     // 서버 헤더를 클라이언트에게 보냄

    if (Fork() == 0) {                                    /* 자식 프로세스 */
        /* 실제 서버는 여기서 모든 CGI 환경 변수를 설정할 것임 */
        setenv("QUERY_STRING", cgiargs, 1);               // CGI 프로그램에게 쿼리 문자열 환경 변수 설정
        setenv("METHOD", method, 1);                      // CGI 프로그램에게 쿼리 문자열 환경 변수 설정
        Dup2(fd, STDOUT_FILENO);                          /* 클라이언트로의 stdout을 리다이렉트함 */
        Execve(filename, emptylist, environ);             /* CGI 프로그램 실행 */
    }
    
     /* 부모는 자식 프로세스가 종료되기를 기다리고 회수함 */
    Wait(NULL);
}