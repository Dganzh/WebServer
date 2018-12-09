//
// Created by dganzh on 18-12-1.
//

#ifndef WEBSERVER_SERVER_H
#define WEBSERVER_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>


typedef struct sockaddr SA;

#define RIO_BUFSIZE 8192
typedef struct {
	int rio_fd;
	int rio_cnt;
	char *rio_bufptr;
	char rio_buf[RIO_BUFSIZE];
} rio_t;

extern char **environ;


#define MAXLINE 8192
#define MAXBUF 8192
#define LISTENQ	1024

void unix_error(char *msg);
void gai_error(int code, char *msg);
void app_error(char *msg);
void dns_error(char *msg);
void sio_error(char *msg);

void Sio_error(char *msg);
ssize_t sio_puts(char s[]);
int sio_strlen(char s[]);

char *Fgets(char *ptr, int n, FILE *stream);
void Fputs(const char *ptr, FILE *stream);
void Close(int fd);

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen);
struct hostent *Gethostbyaddr(const char *addr, int len, int type);

ssize_t Rio_readn(int fd, void *usrbuf, size_t n);
void Rio_writen(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);
void Rio_readinitb(rio_t *rp, int fd);
ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

int open_listenfd(char *port);
int open_clientfd(char *hostname, char *port);
int Open_listenfd(char *port);
int Open_clientfd(char *hostname, char *port);

void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
   				size_t hostlen, char *serv, size_t servlen, int flags);

int Dup2(int fd1, int fd2);
pid_t Fork(void);
void Execve(const char *filename, char *const argv[], char *const envp[]);
pid_t Wait(int *status);

void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
void Munmap(void *start, size_t length);

void *Malloc(size_t size);
void Free(void *ptr);

int Open(const char *pathname, int flags, mode_t mode);
ssize_t Write(int fd, const void *buf, size_t count);

typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);
void signal_handler(int sig);

#endif //WEBSERVER_SERVER_H
