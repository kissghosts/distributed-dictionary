#include "nameservice.h"

void handle_err(char *str);
int readline(int fd, char **buf);
void lock_file(int fd);
void unlock_file(int fd);
void route_server(int sockfd, int routefd, struct sockaddr_in *cliaddr);
int find_route_ip(int routefd, char nameitem, char *ipaddr);

int main(int argc, char *argv[])
{
    int listenfd, connfd, routefd;
    int opt;
    int port = 0;
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
        case 'p':
            port = atoi(optarg);
            if (port < 65535 && port > 1024) {
                fprintf(stderr, "Error: illegal port number\n");
                exit(EXIT_FAILURE);
            }
            break;
        default: /* ? */
            fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    
    if ((routefd = open(ROUTE_TABLE, O_RDWR | O_APPEND | O_CREAT, 
        S_IRUSR | S_IWUSR)) < 0) {
        handle_err("Open or create route table file error");
    }

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
        handle_err("socket error");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (port == 0) {
        port = SERV_PORT;
    }
    servaddr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 )
        handle_err("binding error");

    if (listen(listenfd, LISTEN_BACKLOG) == -1)
        handle_err("listen error");

    /* call accept, wait for a client */ 
    for ( ; ; ) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen); 
        if (connfd == -1)
            handle_err("accept error");

        if ((childpid = fork()) < 0)
            handle_err("fork error");
        else if (childpid == 0) { /* child process */
            close(listenfd);
            route_server(connfd, routefd, &cliaddr); /* process the request */
            exit(0);
        }

        close(connfd); /* parent closes connected socket */
    }

    exit(EXIT_SUCCESS);
}

void handle_err(char *str)
{
    perror(str);
    exit(EXIT_FAILURE);
}

int readline(int fd, char **buf)
{
    int n, i;
    int length = 0;
    char line[MAXBYTE];
    char cbuf[2];

    for ( ; ; ) {
        if ((n = read(fd, cbuf, 1)) < 0) {
            handle_err("read error");
        } else if (n == 0) {
            break; // end of the file
        } else if ((cbuf[0] == '\n')) {
            line[length] = cbuf[0];
            length++;
            break;
        } else if ((cbuf[0] != '\n')) {
            if (length == MAXBYTE) { // a very long line
                return(-1);
            }

            line[length] = cbuf[0]; // stuff in buffer.
            length++;
        }
    }

    if ((*buf = (char*) malloc(length + 1)) == NULL) {
        handle_err("malloc error");
    }

    for (i = 0; i < length && line[i] != '\n'; i++) {
        (*buf)[i] = line[i];
    }
    (*buf)[i] = '\0';

    return length;
}

void lock_file(int fd)
{
    struct flock lock;

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;             /* write lock entire file */

    fcntl(fd, F_SETLKW, &lock);
}

void unlock_file(int fd)
{
    struct flock lock;

    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;             /* unlock entire file */

    fcntl(fd, F_SETLK, &lock);
}

int find_route_ip(int routefd, char nameitem, char *ipaddr, 
    struct sockaddr_in *cliaddr) {
    
    char *eachline, *buf;
    int n, i, j, len;
    int flag = -1;

    lock_file(routefd);
    if (lseek(routefd, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return flag;
    }

    while ((n = readline(routefd, &eachline)) > 0) {
        if (eachline[0] == '#') {
            continue;
        }

        if (eachline[0] == nameitem) {
            flag = 1;
            len = strlen(eachline);
            j = 0;
            for (i = 3; i < len && eachline[i] != '\n'; i++) {
                ipaddr[j++] = eachline[i];
            }
            for ( ; j < 16; j++) {
                ipaddr[j] = '\0';
            }
            break;
        }
    }

    if (flag != 1) { /* not found, add a new mapping */
        
        ipaddr = inet_ntoa(cliaddr->sin_addr);
        // ptr = inet_ntop(AF_INET, &(cliaddr->sin_addr), ipaddr, sizeof(ipaddr));

        if (ipaddr == NULL) {
            flag = -1;
        } else {
            len = 3 + strlen(ipaddr) + 1;
            if ((buf = (char*) malloc(len + 1)) == NULL) {
                return -1;
            }

            sprintf(buf, "%c: %s\n", nameitem, ipaddr);

            if ((n = write(routefd, buf, len)) == -1) {
                flag = -1;
                return flag;
            }

            flag = 0;
        }
    }

    lock_file(routefd);
    return flag;
}

void route_server(int sockfd, int routefd, struct sockaddr_in *cliaddr)
{
    ssize_t n;
    int flag;
    struct route_prtl request, reply;

    reply.protocol = '2';

    if ((n = read(sockfd, &request, sizeof(request))) < 0) {
        handle_err("read error");
    }

    if (request.protocol != '2') {
        fprintf(stderr, "Error: unknown packet\n");
        return;
    }

    if (request.type == '1') { /* lookup */
        reply.id == request.id;
        
        flag = find_route_ip(routefd, request.id, reply.ipaddr, cliaddr);
        if (flag == -1) {
            fprintf(stderr, "Error: fail to open or write route table\n");
            reply.type = '4';
        } else if (flag == 0) {
            reply.type = '3';
        } else if (flag == 1) {
            reply.type = '2';
        }

        // send the reply
        if (write(sockfd, &reply, sizeof(reply)) == -1) {
            fprintf(stderr, "Error: fail to sent the reply\n");
        }
    }
}