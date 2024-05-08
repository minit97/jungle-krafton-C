/* 
    Part 3
    - 프록시에 캐싱 추가
    - 최근에 액세스한 웹 콘텐츠의 간단한 메인 메모리 캐시사용
*/
#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000      // 최대 캐시 사이즈 1MiB, 메타 데이터 등 불필요한 바이트는 무사
#define MAX_OBJECT_SIZE 102400      // 최대 객체 사이즈 100KiB

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void *thread(void *vargp);
void doit(int connfd);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void make_header(char *final_header, char *hostname, char *path, rio_t *client_rio);
int server_connection(char *hostname, int port);

/* For Cache */
// 최근 사용 횟수가 가장 적은 페이지를 삭제하는 방법인 LRU를 사용 (Least Recently Used)
#define LRU_PRIORITY 9999
#define MAX_OBJECT_SIZE_IN_CACHE 10

void cache_init();
int check_cache(char *url, char *uri, int connfd);
int cache_find(char *url);
void read_before(int index);
void read_after(int index);

void cache_uri(char *uri, char *buf);
int cache_eviction();
void cache_LRU(int index);



/*
    [Synchronization]
    : 캐시에 대한 접근은 thread-safe 해야 함
    : Pthreads readers-writers locks, Readers-Writers with semaphores 옵션 등이 있음
*/

typedef struct {
    char cache_object[MAX_OBJECT_SIZE];
    char cache_url[MAXLINE];

    // 우선순위가 높을수록 오래된 페이지, 즉 수가 낮을수록 삭제될 가능성이 높음
    int priority;
    int is_empty;

    int read_count;
    sem_t wmutex;
    sem_t rdcntmutex;
} cache_block;

typedef struct {
    cache_block cache_items[MAX_OBJECT_SIZE_IN_CACHE];
} Cache;

Cache cache;

int main(int argc, char **argv) {
    pthread_t tid;

    cache_init();

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
        
        int *connfd_ptr = (int *)malloc(sizeof(int));
        *connfd_ptr = connfd;
        Pthread_create(&tid, NULL, thread, (void *)connfd_ptr);
    }
}

void *thread(void *vargp) {
    int connfd = *((int *)vargp);
    free(vargp);

    Pthread_detach(Pthread_self());

    doit(connfd);
    Close(connfd);

    return NULL;    
}

void doit(int connfd) {
    int serverfd, port;
    char server_header[MAXLINE];
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    char cachebuf[MAX_OBJECT_SIZE], url[MAXLINE];
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

    // check_cache 함수를 이용해서 캐시
    if (!(check_cache(url, uri, connfd))) {
        return;
    }

    parse_uri(uri, hostname, path, &port);
    make_header(server_header, hostname, path, &client_rio);

    serverfd = server_connection(hostname, port);

    Rio_readinitb(&server_rio, serverfd);
    Rio_writen(serverfd, server_header, strlen(server_header));

    size_t response;
    int bufsize = 0;
    while ((response = Rio_readlineb(&server_rio, buf, MAXLINE)) != 0) {
        bufsize += response;

        // 최대 개체 사이즈보다 작으면, 받은 응답을 캐시에 저장
        if (bufsize < MAX_OBJECT_SIZE) {
            strcat(cachebuf, buf);
        }

        Rio_writen(connfd, buf, response);
    }

    Close(serverfd);

    if (bufsize < MAX_OBJECT_SIZE) {
        // URL에 cachebuf 저장
        cache_uri(url, cachebuf);
    }
}

void cache_init() {
    for (int i = 0; i < MAX_OBJECT_SIZE_IN_CACHE; i++) {
        cache.cache_items[i].priority = 0;
        cache.cache_items[i].is_empty = 1;
        cache.cache_items[i].read_count = 0;

        /*
          첫번째: 초기화할 세마포어의 포인터 지정
          두번째: 스레드끼리 세마포어를 공유하려면 0, 프로세스끼리 공유하려면 다른 숫자로 설정
          세번째: 초기값 설정

          두번째와 관련,
          세마포어는 프로세스를 사용하기 때문에, 얘를 뮤텍스로 쓰고 싶을 때 두번째 파라미터를 0으로 설정하여 스레드끼리 공유하도록 함

          wmutex: 캐시에 접근하는 걸 방지
          rdcntmutex: 리드카운트에 접근하는 걸 방지
        */
        Sem_init(&cache.cache_items[i].wmutex, 0, 1);
        Sem_init(&cache.cache_items[i].rdcntmutex, 0, 1);
    }
}

int check_cache(char *url, char *uri, int connfd) {
    strcpy(url, uri);

    // cache_find 함수를 통해 search, -1이 아니라면 캐시에 저장되어 있다는 의미
    int index;

    if ((index = cache_find(url)) != -1) {
        read_before(index);     // read_before 함수를 통해 캐시 뮤텍스를 열어줌
        // 캐시에서 찾은 값을 connf에 쓰고, 바로 보냄
        Rio_writen(connfd, cache.cache_items[index].cache_object, strlen(cache.cache_items[index].cache_object));
        read_after(index);      // read_after 함수를 통해 캐시 뮤텍스를 닫아줌

        return 0;
    }
    return 1;
}

int cache_find(char *url) {
    for (int i = 0; i < MAX_OBJECT_SIZE_IN_CACHE; i++) {
        read_before(i);

        // 캐시가 비어있고, 해당 url이 이미 캐시에 들어있을 경우
        if (cache.cache_items[i].is_empty == 0 && strcmp(url, cache.cache_items[i].cache_url) == 0) {
            read_after(i);
            return i;
        }

        read_after(i);
    }  
    return -1;
}

/*
    P()
    : sem_wait(), 세마포어 값을 1 감소시킴
    : 0 이상이면 즉시 리턴, 음수가 되면 wait 상태가 되며 sem_post()가 호출될 때까지 wait함

    V()
    : sem_post(), 세마포어 값을 1 증가시킴
    : 이로 인해 값이 1 이상이 된다면 wait 중인 스레드 중 하나를 깨움
*/
void read_before(int index) {
    // P 함수를 통해 rdcntmutex에 접근 가능하게 해줌
    P(&cache.cache_items[index].rdcntmutex);

    // 사용하지 않는 item이라면 0에서 1로 바뀌었을 것
    cache.cache_items[index].read_count += 1;

    // 1일 때만 캐쉬에 접근 가능, 누가 쓰고 있는 item이라면 2가 되기 때문에 if문으로 들어올 수 없음
    if (cache.cache_items[index].read_count == 1) {
        // wmutex에 접근 가능하게 해줌, 즉 캐시에 접근
        P(&cache.cache_items[index].wmutex);
    }

    // V 함수를 통해 접근 불가능하게 해줌
    V(&cache.cache_items[index].rdcntmutex);
}

void read_after(int index) {
    P(&cache.cache_items[index].rdcntmutex);
    cache.cache_items[index].read_count -= 1;

    if (cache.cache_items[index].read_count == 0) {
        V(&cache.cache_items[index].wmutex);
    }

    V(&cache.cache_items[index].rdcntmutex);
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

// cache_uri : 빈 캐시를 찾아 값을 넣어주고 나머지 캐시의 우선순위를 내려주는 함수
void cache_uri(char *uri, char *buf) {
    // cache_eviction 함수를 이용하여 빈 캐시 블럭을 찾음
    int index = cache_eviction();

    P(&cache.cache_items[index].wmutex);

    strcpy(cache.cache_items[index].cache_object, buf);
    strcpy(cache.cache_items[index].cache_url, uri);

    // 방금 채웠으니, 우선순위는 9999로
    cache.cache_items[index].is_empty = 0;
    cache.cache_items[index].priority = 9999;

    // 방금 채웠으니, 나머지 캐시의 우선순위를 모두 올려야 함
    cache_LRU(index);

    V(&cache.cache_items[index].wmutex);
}

// cache_eviction : 캐시에 공간이 필요하여 데이터를 지우는 함수 
int cache_eviction() {
    int min = LRU_PRIORITY;
    int index = 0;

    for (int i = 0; i < MAX_OBJECT_SIZE_IN_CACHE; i++) {
        read_before(i);

        if (cache.cache_items[i].is_empty == 1) {
            index = i;
            read_after(i);
            break;
        }

        if (cache.cache_items[i].priority < min) {
            index = i;
            min = cache.cache_items[i].priority;
            read_after(i);
            continue;
        }

        read_after(i);
    }
    return index;
}

// cache_LRU: 현재 캐시의 우선순위를 모두 올림, 최근 캐시 들어갔으므로
void cache_LRU(int index) {
    for (int i = 0; i < MAX_OBJECT_SIZE_IN_CACHE; i++) {
        if (i == index) {
            continue;
        }

        P(&cache.cache_items[i].wmutex);

        if (cache.cache_items[i].is_empty == 0) {
            cache.cache_items[i].priority -= 1;
        }

        V(&cache.cache_items[i].wmutex);
    }
}