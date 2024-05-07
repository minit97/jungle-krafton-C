/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"
int main(void) {
    
    char *buf, *p, *method = getenv("METHOD");              // 문자열을 담을 포인터 변수 선언
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];    // 쿼리에서 추출할 두 인자와 응답 본문을 저장할 배열 선언
    int n1 = 0, n2 = 0;                                     // 정수 저장할 변수 초기화

    /* Extract the two arguments */
    // 환경 변수에서 쿼리 문자열 가져오기
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        p = strchr(buf, '&');
        *p = '\0';                    // 문자열을 두 부분으로 나눔
        strcpy(arg1, buf);            // 첫 번째 인자를 복사
        strcpy(arg2, p + 1);          // 두 번째 인자를 복사
        *p = '&';                     // 문자열을 다시 붙임

        // solved problem 11.10 : 전처리
        char *value1, *ptr1 = strchr(arg1, '=');
        if(ptr1 != NULL) {
          value1 = ptr1 + 1;
        }

        char *value2, *ptr2 = strchr(arg2, '=');
        if(ptr2 != NULL) {
          value2 = ptr2 + 1;
        }

        if (value1 != NULL && value2 != NULL) {
          n1 = atoi(value1);
          n2 = atoi(value2);
        } else {
          n1 = atoi(arg1);              // 첫 번째 인자를 정수로 변환
          n2 = atoi(arg2);              // 두 번째 인자를 정수로 변환
        }
    }
    
    /* Make the response body */
    /* content 배열에 HTML 형식의 문자열 순차적으로 추가*/
    sprintf(content, "QUERY_STRING=%s \r\n", buf);                                                // HTML 형식으로 생성
    sprintf(content + strlen(content), "Welcome to add.com: ");
    sprintf(content + strlen(content), "THE Internet addition portal.\r\n<p>");                   // 포털 설명을 추가
    sprintf(content + strlen(content), "The answer is: %d + %d = %d\r\n<p>", n1, n2, n1 + n2);    // 계산 결과를 추가
    sprintf(content + strlen(content), "Thanks for visiting!\r\n");                               // 방문 감사 메시지를 추가
 
    /* Generate the HTTP response */
    /* HTTP 응답 헤더를 출력하고, 생성된 HTML 콘텐츠를 클라이언트에게 전송*/
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));   // 컨텐츠 길이를 계산하여 헤더에 추가
    printf("Content-type: text/html\r\n\r\n");

    // method가 GET일 경우에만 response body 보냄
    if(strcasecmp(method, "GET") == 0) { 
      printf("%s", content);
    }

    fflush(stdout); // 출력 버퍼를 비움
    exit(0);
}
/* $end adder */