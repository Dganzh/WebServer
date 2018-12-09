#include "server.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
void read_requestbody(rio_t *rp, char *cgiargs);
void save_requesthdrs(rio_t *rp);
void write_requesthdrs(int fd);
int parse_uri(char *uri, char *filename);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);
void Im_rio_writen(int fd, void *usrbuf, size_t n);


char requesthdrs[MAXBUF];


void Im_rio_writen(int fd, void *usrbuf, size_t n) {
    if (rio_writen(fd, usrbuf, n) != n) {
        if (errno == EPIPE)
            fprintf(stderr, "EPIPE error");
        fprintf(stderr, "%s ", strerror(errno));
        unix_error("client side has ended connection");
    }

}


int main(int argc, char **argv) {
    int listenfd, connfd, clientlen;
    struct sockaddr_in clientaddr;
    char hostname[MAXLINE], port[MAXLINE];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    if (Signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        unix_error("mask signal pipe error");

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
}


/* 处理请求 */
void doit(int fd) {
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* 读请求体 */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET") &&
            strcasecmp(method, "HEAD") &&
            strcasecmp(method, "POST")) {
        clienterror(fd, method, "501", "Not Implemented",
                "This server does not implement this method");
        return;
    }

    save_requesthdrs(&rio);
    if (strcasecmp(method, "POST") == 0) {
        printf("start read request body:\n");
        read_requestbody(&rio, cgiargs);
        printf("%s\n", cgiargs);
        fflush(stdout);
    }

    /* 解析uri */
    is_static = parse_uri(uri, filename);
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found",
                "Server could't find this file");
        return;
    }

    if (is_static) {        /* 处理静态内容的请求 */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                    "Server couldn't read this file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size, method);
    }
    else {                  /* 处理动态内容的请求 */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                    "Server couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}


/* 定义错误的响应 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* 构造响应体body */
    sprintf(body, "<html><title>Server Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Web Server</em>\r\n", body);

    /* 生成响应头，并输出 */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Im_rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Im_rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));  /* 请求头结束，要增加一个空文本行 */
    Im_rio_writen(fd, buf, strlen(buf));

    /* 输出响应body */
    Im_rio_writen(fd, body, strlen(body));
}


// 读取请求头，但不做处理
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while (strcmp(buf, "\r\n")) {
        printf("%s", buf);
        Rio_readlineb(rp, buf, MAXLINE);
    }
    fflush(stdout);
    return;
}


// 读取请求头，但不做处理
void read_requestbody(rio_t *rp, char *cgiargs) {
    char buf[MAXLINE];
    Rio_readnb(rp, buf, rp->rio_cnt);
    strcpy(cgiargs, buf);
    return;
}


// 保存请求头
void save_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    strcpy(requesthdrs, buf);
	while(strcmp(buf, "\r\n")) {
		Rio_readlineb(rp, buf, MAXLINE);
		sprintf(requesthdrs, "%s%s", requesthdrs, buf);
	}
}

// 原样输出请求头
void write_requesthdrs(int fd) {
    Rio_writen(fd, requesthdrs, strlen(requesthdrs));
    requesthdrs[0] = '\0';
}


/*
 * 解析uri为一个文件名和一个cgi参数
 * 根据uri是否包含字符串"cgi-bin"来判断请求内容是否是动态内容
 * 如果是动态内容，解析cgi参数到cgiargs，并把剩余的uri字符转为可执行文件的相对路径，返回0
 * 如果是静态内容，则清除cgi字符串，并把uri转为静态文件的相对路径，返回1
 */
int parse_uri(char *uri, char *filename) {
    char *ptr;

    if (strstr(uri, "cgi-bin")) {   /* 请求动态内容 */
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
    else {          /* 请求静态内容 */
        strcpy(filename, ".");
        strcat(filename, uri);

        if (uri[strlen(uri) - 1] == '/')        /* 以/结尾，则说明请求的是主页 */
            strcat(filename, "static/home.html");      /* 拼接主页地址 */
        return 1;
    }
}


/* 处理动态内容的请求 */
void serve_dynamic(int fd, char *filename, char *cgiargs) {
    if (signal(SIGCHLD, signal_handler) == SIG_ERR)
        unix_error("signal error");

    char buf[MAXLINE], *emptylist[] = {NULL};

    /* 输出响应header */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Im_rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Web Server 1.0\r\n");
    Im_rio_writen(fd, buf, strlen(buf));

    write_requesthdrs(fd);

    if (Fork() == 0) {
        setenv("QUERY_STRING", cgiargs, 1);
        Dup2(fd, STDOUT_FILENO);
        Execve(filename, emptylist, environ);
    }
    Wait(NULL);
}


/* 处理静态内容的请求 */
void serve_static(int fd, char *filename, int filesize, char *method) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXLINE];

    get_filetype(filename, filetype);

    /* 发送响应header */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Web Server 1.0\r\n", buf);

    sprintf(buf, "%sContent-type: %s\r\n", buf, filetype);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);  /* 请求头结束，要增加一个空文本行 */
    Im_rio_writen(fd, buf, strlen(buf));
    printf("response headers:\n");
    printf("%s", buf);

    write_requesthdrs(fd);

    if(!strcasecmp(method, "GET")) {    // GET请求返回响应体
        /* 发送响应body */
        srcfd = Open(filename, O_RDONLY, 0);

        srcp = Malloc(filesize);
        Rio_readn(srcfd, srcp, filesize);
        Im_rio_writen(fd, srcp, filesize);

        Free(srcp);
    }
}

void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mpg"))
        strcpy(filetype, "video/mpg");
    else
        strcpy(filetype, "text/plain");
}


void signal_handler(int sig) {
    int old_errno = errno;

    while (waitpid(-1, NULL, 0) > 0) {
        continue;
    }
    if (errno != ECHILD)
        Sio_error("waitpid error");

    errno = old_errno;
}

handler_t *Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}


