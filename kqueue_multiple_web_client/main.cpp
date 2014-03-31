//
//  main.cpp
//  socket
//
//  Created by shenyixin on 14-3-24.
//  Copyright (c) 2014年 shenyixin. All rights reserved.
//
#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>  /* basic socket definitions */

#define MAXFILES    20
#define SERV        "80"    /* port number or service name */

struct file {
    char  *f_name;            /* filename */
    char  *f_host;            /* hostname or IPv4/IPv6 address */
    int    f_fd;              /* descriptor */
    int    f_flags;           /* F_xxx below */
} file[MAXFILES];

#define F_CONNECTING    1   /* connect() in progress */
#define F_READING       2   /* connect() complete; now reading */
#define F_DONE          4   /* all done */

//需要加host
#define GET_CMD     "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: Close\r\n\r\n"

/* globals */
int     nconn, nfiles, nlefttoconn, nlefttoread, maxfd;
fd_set  rset, wset;

/* function prototypes */
void    home_page(const char *, const char *);
void    start_connect(struct file *);
void    write_get_cmd(struct file *);


void
home_page(const char *host, const char *fname)
{
	int		fd, n;
	char	line[MAXLINE];

	fd = Tcp_connect(host, SERV);	/* blocking connect() */
    
	n = snprintf(line, sizeof(line), GET_CMD, fname, host);
	Writen(fd, line, n);
    
	for ( ; ; ) {
        memset(line, 0, sizeof(char) * MAXLINE);
		if ( (n = Read(fd, line, MAXLINE-1)) == 0)
			break;		/* server closed connection */
        line[MAXLINE - 1] = '\0';
		printf("read %d bytes of home page\n", n);
        printf("%s\n", line);
		/* do whatever with data */
	}
	printf("end-of-file on home page\n");
	Close(fd);
}

void
write_get_cmd(struct file *fptr)
{
    int     n;
    char    line[MAXLINE];
    
    n = snprintf(line, sizeof(line), GET_CMD, fptr->f_name, fptr->f_host);
    Writen(fptr->f_fd, line, n);
    printf("wrote %d bytes for %s\n", n, fptr->f_name);
    
    fptr->f_flags = F_READING;          /* clears F_CONNECTING */
    
    FD_SET(fptr->f_fd, &rset);          /* will read server's reply */
    if (fptr->f_fd > maxfd)
        maxfd = fptr->f_fd;
}


void
start_connect(struct file *fptr)
{
    int             fd, flags, n;
    struct addrinfo *ai;
    
    ai = Host_serv(fptr->f_host, SERV, 0, SOCK_STREAM);
    
    fd = Socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    fptr->f_fd = fd;
    printf("start_connect for %s, fd %d\n", fptr->f_name, fd);
    
    /* 4Set socket nonblocking */
    flags = Fcntl(fd, F_GETFL, 0);
    Fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    
    /* 4Initiate nonblocking connect to the server. */
    if ( (n = connect(fd, ai->ai_addr, ai->ai_addrlen)) < 0) {
        if (errno != EINPROGRESS)
            err_sys("nonblocking connect error");
        fptr->f_flags = F_CONNECTING;
        FD_SET(fd, &rset);          /* select for reading and writing */
        FD_SET(fd, &wset);
        if (fd > maxfd)
            maxfd = fd;
        
    } else if (n >= 0)              /* connect is already done */
        write_get_cmd(fptr);    /* write() the GET command */
}


int main(int argc, char **argv) {
    
    int ret_val = 0;

    
    
	int		i, fd, n, maxnconn, flags, error, ret;
	char	buf[MAXLINE];
	fd_set	rs, ws;
    
    
	int errcode = 0;
	int errcode_size = sizeof(int);
	ret_val = getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode, (socklen_t *)&errcode_size);
    
	if (argc < 5)
		err_quit("usage: web <#conns> <hostname> <homepage> <file1> ...");
	maxnconn = atoi(argv[1]);
    
	nfiles = min(argc - 4, MAXFILES);
	for (i = 0; i < nfiles; i++) {
		file[i].f_name = argv[i + 4];
		file[i].f_host = argv[2];
		file[i].f_flags = 0;
	}
	printf("nfiles = %d\n", nfiles);
    
	home_page(argv[2], argv[3]);
    
	FD_ZERO(&rset);
	FD_ZERO(&wset);
	maxfd = -1;
	nlefttoread = nlefttoconn = nfiles;
	nconn = 0;
    /* end web1 */
    /* include web2 */
	while (nlefttoread > 0)
    {
		while (nconn < maxnconn && nlefttoconn > 0) {
            /* 4find a file to read */
			for (i = 0 ; i < nfiles; i++)
				if (file[i].f_flags == 0)
					break;
			if (i == nfiles)
				err_quit("nlefttoconn = %d but nothing found", nlefttoconn);
			start_connect(&file[i]);
			nconn++;
			nlefttoconn--;
		}
        
        rs = rset;
		ws = wset;
		n = Select(maxfd+1, &rs, &ws, NULL, NULL);
        
		for (i = 0; i < nfiles; i++) {
			flags = file[i].f_flags;
			if (flags == 0 || flags & F_DONE)
				continue;
			fd = file[i].f_fd;
			if (flags & F_CONNECTING &&
				(FD_ISSET(fd, &rs) || FD_ISSET(fd, &ws))) {
				n = sizeof(error);
				if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&n) < 0 ||
					error != 0) {
					err_ret("nonblocking connect failed for %s",
							file[i].f_name);
				}
                /* 4connection established */
				printf("connection established for %s\n", file[i].f_name);
				FD_CLR(fd, &wset);		/* no more writeability test */
				write_get_cmd(&file[i]);/* write() the GET command */
                
			} else if (flags & F_READING && FD_ISSET(fd, &rs)) {
                memset(buf, 0, MAXLINE);
				if ( (n = Read(fd, buf, sizeof(buf)-1)) == 0) {
					printf("end-of-file on %s\n", file[i].f_name);
					Close(fd);
					file[i].f_flags = F_DONE;	/* clears F_READING */
					FD_CLR(fd, &rset);
					nconn--;
					nlefttoread--;
				} else {
					printf("read %d bytes from %s\n", n, file[i].f_name);
                    printf("%s\n", buf);

				}
			}
		}

        
    }
    /*
    //todo
    //struct timespec ts;
    bool hasSend = false;
    bool hasClosed = false;
    int kq = kqueue();
    
    //register
    int ret = 0;
    struct kevent changes[1];
    EV_SET(&changes[0], sockfd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
    ret = kevent(kq, changes, 1, NULL, 0, NULL);
    
    EV_SET(&changes[0], sockfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    ret = kevent(kq, changes, 1, NULL, 0, NULL);
    
    
	while(true)
    {
        if (hasClosed) {
            return 0;
        }
        struct kevent events[1];
        
        ret = kevent(kq, NULL, 0, events, 1, NULL);
        if (ret < 0)
        {
            
        }
        else
        {
            for (int i = 0; i < ret; i++)
            {
                struct kevent event = events[i];
                
                int fd = events[i].ident;
                int avail_bytes = events[i].data;
                
                if (event.filter == EVFILT_WRITE)
                {
                    //send
                    build_http_request(HOST, http_request, sizeof(http_request)/sizeof(int));
                    if (send(sockfd,http_request,strlen(http_request), 0) < 0)
                    {
                        err_sys("send");
                    }
                    EV_SET(&changes[0], sockfd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
                    ret = kevent(kq, changes, 1, NULL, 0, NULL);
                    if (ret == -1)
                    {
                        perror("kevent()");
                    }
                }
                
                if (event.filter == EVFILT_READ)
                {
                    memset(buf, 0, BUFFSIZE);
                    int n = read(sockfd, buf, BUFFSIZE-1);
                    if (n == 0 || n == -1)
                    {
                        hasClosed = true;
                        //close(sockfd);
                        //kevent(): Bad file descriptor
                        //EV_SET(&changes[0], sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
                    }
                    else
                    {
                        printf("%s",buf);
                    }
                }
            }
        }
        
    }*/
	return 0;
}


