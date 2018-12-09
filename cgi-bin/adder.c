//
// Created by dganzh on 18-12-2.
//
#include "server.h"


int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], response[MAXLINE];
    int n1 = 0, n2 = 0;

    if ((buf = getenv("QUERY_STRING")) != NULL) {
        p = strchr(buf, '&');
        *p = '\0';
        strcpy(arg1, buf);
        strcpy(arg2, p + 1);

        p = strchr(arg1, '=');
        n1 = atoi(p + 1);

        p = strchr(arg2, '=');
        n2 = atoi(p + 1);
    }

    // 构造response body
    sprintf(response, "Welcome to add.com\r\n");
    sprintf(response, "%sThe calculation and answer is: %d + %d = %d\r\n",
            response, n1, n2, n1 + n2);
    sprintf(response, "%s Thanks for visiting!\r\n", response);

    // 输出response
    printf("Content-type: text/html\r\n");
    printf("Content-length: %d\r\n\r\n", (int)strlen(response));
    printf("%s", response);
    fflush(stdout);
    exit(0);
}


