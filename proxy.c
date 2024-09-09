#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";


void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *hostname, char *port, char *path);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	doit(connfd);                                             //line:netp:tiny:doit
	Close(connfd);                                            //line:netp:tiny:close
    }
    printf("%s", user_agent_hdr);
    return 0;
}


void doit(int fd)
{
    //int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
    //char filename[MAXLINE], cgiargs[MAXLINE];
    char server[MAXLINE*3];
    rio_t rio, serrio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        printf("Proxy does not implement this method\r\n");
        return;
    }                                                    //line:netp:doit:endrequesterr
    //read_requesthdrs(&rio);                              //line:netp:doit:readrequesthdrs

    /* Parse URI from GET request */
    int ok = parse_uri(uri, hostname, port, path);       //line:netp:doit:staticcheck
    if(ok<0)
    {
        printf("Cannot parse uri.\n");
        return;
    }
    //构建server的请求头
    snprintf(server, sizeof(server), "%s %s %s\r\n", method, path, version);
    snprintf(server + strlen(server), sizeof(server) - strlen(server), "Host: %s\r\n", hostname);
    snprintf(server + strlen(server), sizeof(server) - strlen(server), "Connection: close\r\n");
    snprintf(server + strlen(server), sizeof(server) - strlen(server), "User-Agent: Mozilla/5.0\r\n");
    snprintf(server + strlen(server), sizeof(server) - strlen(server), "\r\n");


    int serverfd=open_clientfd(hostname, port);

    if (serverfd < 0)
    {
        printf("Cannot connect to server.\n");
        return;
    }
	//转发给服务器
    Rio_readinitb(&serrio, serverfd);
    Rio_writen(serverfd, server, strlen(server));

    size_t n;
    //回复给客户端
    while ((n = Rio_readlineb(&serrio, buf, MAXLINE)) != 0)
    {
        printf("proxy received %d bytes,then send\n", (int)n);
        Rio_writen(fd, buf, n);
    }
    //关闭服务器描述符
    Close(serverfd);
}

int parse_uri(char *uri, char *hostname, char *port, char *path) {
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) == 0) {
        uri += 7;
    } else {
        return -1;
    }

    hostbegin = uri;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    if (*hostend == ':') {
        char *portbegin = hostend + 1;
        char *portend = strpbrk(portbegin, "/\r\n\0");
        len = portend - portbegin;
        strncpy(port, portbegin, len);
        port[len] = '\0';
    } else {
        strcpy(port, "80");
    }

    pathbegin = strchr(hostend, '/');
    if (pathbegin) {
        strcpy(path, pathbegin);
    } else {
        path[0] = '\0';
    }

    return 0;
}