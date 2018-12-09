//
// Created by dganzh on 18-12-1.
//

#include "server.h"


//ssize_t read(int fd, void *buf, size_t n);
//ssize_t write(int fd, const void *buf, size_t n);
//int close(int fd);

/****************************************
 * 个性化输出错误信息
 ****************************************/

void unix_error(char *msg) {
	fprintf(stderr, "%s: %s\n", msg, strerror(errno));
	exit(0);
}

void app_error(char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(0);
}

void dns_error(char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(0);
}

void gai_error(int code, char *msg){
	fprintf(stderr, "%s: %s\n", msg, gai_strerror(code));
	exit(0);
}

void sio_error(char *msg) {
    sio_puts(msg);
    _exit(1);
}

void Sio_error(char *msg) {
    sio_error(msg);
}


ssize_t sio_puts(char s[]) {
    return write(STDOUT_FILENO, s, sio_strlen(s));
}

int sio_strlen(char s[]) {
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}

/****************************************
 * 封装文件读写
 ****************************************/

char *Fgets(char *ptr, int n, FILE *stream) {
	char *rptr;

	/*	fgets()从stream读取一行(最多n-1个字符),存到ptr,并返回字符数组的首地址	*/
	if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
	    app_error("Fgets error");
	return rptr;
}


void Fputs(const char *ptr, FILE *stream) {
	if (fputs(ptr, stream) == EOF)
	  unix_error("Fputs erros");
}


void Close(int fd) {
	int rc;

	if ((rc = close(fd)) < 0)
	  unix_error("Close error");
}


/****************************************
 * The Rio package - Robust I/O functions
 ****************************************/

/*
 * rio_readn - Robustly read n bytes (unbuffered)
 */
/* $begin rio_readn */
ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nread = read(fd, bufp, nleft)) < 0) {
	    if (errno == EINTR) /* Interrupted by sig handler return */
		nread = 0;      /* and call read() again */
	    else
		return -1;      /* errno set by read() */
	}
	else if (nread == 0)
	    break;              /* EOF */
	nleft -= nread;
	bufp += nread;
    }
    return (n - nleft);         /* Return >= 0 */
}
/* $end rio_readn */

/*
 * rio_writen - Robustly write n bytes (unbuffered)
 */
/* $begin rio_writen */
ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* Interrupted by sig handler return */
			nwritten = 0;    /* and call write() again */
	    else
			return -1;       /* errno set by write() */
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}
/* $end rio_writen */


/*
 * rio_read - This is a wrapper for the Unix read() function that
 *    transfers min(n, rio_cnt) bytes from an internal buffer to a user
 *    buffer, where n is the number of bytes requested by the user and
 *    rio_cnt is the number of unread bytes in the internal buffer. On
 *    entry, rio_read() refills the internal buffer via a call to
 *    read() if the internal buffer is empty.
 */
/* $begin rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) {  /* Refill if buf is empty */
	rp->rio_cnt = read(rp->rio_fd, rp->rio_buf,
			   sizeof(rp->rio_buf));
	if (rp->rio_cnt < 0) {
	    if (errno != EINTR) /* Interrupted by sig handler return */
		return -1;
	}
	else if (rp->rio_cnt == 0)  /* EOF */
	    return 0;
	else
	    rp->rio_bufptr = rp->rio_buf; /* Reset buffer ptr */
    }

    /* Copy min(n, rp->rio_cnt) bytes from internal buf to user buf */
    cnt = n;
    if (rp->rio_cnt < n)
	cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}
/* $end rio_read */

/*
 * rio_readinitb - Associate a descriptor with a read buffer and reset buffer
 */
/* $begin rio_readinitb */
void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}
/* $end rio_readinitb */

/*
 * rio_readnb - Robustly read n bytes (buffered)
 */
/* $begin rio_readnb */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nread = rio_read(rp, bufp, nleft)) < 0)
            return -1;          /* errno set by read() */
	else if (nread == 0)
	    break;              /* EOF */
	nleft -= nread;
	bufp += nread;
    }
    return (n - nleft);         /* return >= 0 */
}
/* $end rio_readnb */

/*
 * rio_readlineb - Robustly read a text line (buffered)
 */
/* $begin rio_readlineb */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) {
        if ((rc = rio_read(rp, &c, 1)) == 1) {
	    *bufp++ = c;
	    if (c == '\n') {
                n++;
     		break;
            }
	} else if (rc == 0) {
	    if (n == 1)
		return 0; /* EOF, no data read */
	    else
		break;    /* EOF, some data was read */
	} else
	    return -1;	  /* Error */
    }
    *bufp = 0;
    return n-1;
}
/* $end rio_readlineb */

/**********************************
 * Wrappers for robust I/O routines
 **********************************/
ssize_t Rio_readn(int fd, void *ptr, size_t nbytes)
{
    ssize_t n;

    if ((n = rio_readn(fd, ptr, nbytes)) < 0)
	unix_error("Rio_readn error");
    return n;
}

/* begin rio */

void Rio_readinitb(rio_t *rp, int fd) {
	rio_readinitb(rp, fd);
}

ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) {
	ssize_t rc;
	if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
		unix_error("Rio_readlineb error");
	return rc;
}

void Rio_writen(int fd, void *usrbuf, size_t n) {
	if (rio_writen(fd, usrbuf, n) != n)
		unix_error("Rio_writen error");
}

ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t maxlen) {
	ssize_t rc;

	if ((rc = rio_readnb(rp, usrbuf, maxlen)) < 0)
		unix_error("Rio_readnb error");
	return rc;
}

/* end rio */

/****************************************
 * 封装socket相关函数
 ****************************************/

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
	int rc;

	if ((rc = accept(s, addr, addrlen)) < 0)
		unix_error("Accept error");
	return rc;
}

struct hostent *Gethostbyaddr(const char *addr, int len, int type) {
	struct hostent *p;
	if ((p = gethostbyaddr(addr, len, type)) == NULL)
		dns_error("Gethostbyaddr error");
	return p;
}


int open_listenfd(char *port) {
	struct addrinfo hints, *listp, *p;
	int listenfd, rc, optval = 1;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
	hints.ai_flags |= AI_NUMERICSERV;
	if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0) {
		fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
		return -2;
	}

	for (p = listp; p; p = p->ai_next) {
		if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue;
		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
				   (const void *)&optval, sizeof(int));

		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
			break;
		if (close(listenfd) < 0) {
			fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
			return -1;
		}
	}

	freeaddrinfo(listp);
	if (!p)
		return -1;

	if (listen(listenfd, LISTENQ)  < 0) {
		close(listenfd);
		return -1;
	}
	return listenfd;
}

int open_clientfd(char *hostname, char *port) {
	int clientfd, rc;
	struct addrinfo hints, *listp, *p;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;
    hints.ai_flags |= AI_ADDRCONFIG;
	if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0) {
		fprintf(stderr, "getaddrinfo failed (%s: %s): %s\n", hostname, port, gai_strerror(rc));
		return -2;
	}

	for (p = listp; p; p = p->ai_next) {
		if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
			continue;

		if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
			break;
		if (close(clientfd) < 0) {
			fprintf(stderr, "open_clientfd: close faield: %s\n", strerror(errno));
			return -1;
		}
	}
	freeaddrinfo(listp);
	if (!p)
		return -1;
	else
		return clientfd;
}

int Open_clientfd(char *hostname, char *port) {
	int rc;

	if ((rc = open_clientfd(hostname, port)) < 0)
		unix_error("Open_clientfd error");
	return rc;
}


int Open_listenfd(char *port) {
	int rc;

	if ((rc = open_listenfd(port)) < 0) {
		unix_error("Open_listenfd error");
	return rc;
	}
}


void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
			size_t hostlen, char *serv, size_t servlen, int flags) {
	int rc;
	if ((rc = getnameinfo(sa, salen, host, hostlen, serv,
						servlen, flags)) != 0)
	  gai_error(rc, "Getnameinfo error");
}


int Dup2(int fd1, int fd2) {
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
        unix_error("Dup2 error");
    return rc;
}


pid_t Fork(void) {
    pid_t pid;
    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}


void Execve(const char *filename, char *const argv[], char *const envp[]) {
    if (execve(filename, argv, envp) < 0)
        unix_error("Execve error");
}


pid_t Wait(int *status) {
    pid_t pid;
    if ((pid = wait(status)) < 0)
        unix_error("Wait error");
    return pid;
}

int Open(const char *pathname, int flags, mode_t mode) {
    int rc;

    if ((rc = open(pathname, flags, mode)) < 0)
        unix_error("Open error");
    return rc;
}

ssize_t Write(int fd, const void *buf, size_t count) {
	ssize_t rc;

	if ((rc = write(fd, buf, count)) < 0)
		unix_error("Write Error");
	return rc;
}


void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    void *ptr;

    if ((ptr = mmap(addr, len, prot, flags, fd, offset)) == ((void *) -1))
	unix_error("mmap error");
    return(ptr);
}

void Munmap(void *start, size_t length)
{
    if (munmap(start, length) < 0)
	    unix_error("munmap error");
}

void *Malloc(size_t size) {
	void *p;

	if ((p = malloc(size)) == NULL)
		unix_error("Malloc error");
	return p;
}


void Free(void *ptr) {
	free(ptr);
}

