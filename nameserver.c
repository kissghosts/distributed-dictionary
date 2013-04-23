#include "nameservice.h"

void handle_err(char *str);
void name_server(int sockfd, int dbfd, int itemfd, int rservfd, int port);
int parse_name_pkt(struct name_prtl *pkt, char *data);
void gen_name_pkt(struct name_prtl *pkt, char *data);
int readline(int fd, char **buf);
void lock_file(int fd);
void unlock_file(int fd);
int get_server_info(int fd, char *hostname, char *hostport);
void add_name(int sockfd, int dbfd, struct name_prtl *name_request);
int is_in_database(int dbfd, char *name);
int route(int rservfd, char nameitem, char *hostipaddr);
int add_nameitem(int itemfd, char nameitem);

int main(int argc, char *argv[])
{
    int listenfd, connfd, dbfd, itemfd, rservfd;
    int dflag, iflag, opt, n, i;
    int port = 0;
    pid_t childpid;
    char *database, *nameitem;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    dflag = iflag = 0; 
    while ((opt = getopt(argc, argv, "d:i:p:")) != -1) {
        switch (opt) {
        case 'd':
            n = strlen(optarg);

            if ((database = (char *) malloc(n + 1)) == NULL) {
                handle_err("malloc error");
            }            

            for (i = 0; i < n; i++) {
                database[i] = optarg[i];
            }
            database[i]  = '\0';
            dflag = 1;
            break;
        case 'i':
            n = strlen(optarg);

            if ((nameitem = (char *) malloc(n + 1)) == NULL) {
                handle_err("malloc error");
            }            

            for (i = 0; i < n; i++) {
                nameitem[i] = optarg[i];
            }
            nameitem[i]  = '\0';
            iflag = 1;
            break;
        case 'p':
            port = atoi(optarg);
            if (port < 65535 && port > 1024) {
                fprintf(stderr, "Error: illegal port number\n");
                exit(EXIT_FAILURE);
            }
            break;
        default: /* ? */
            fprintf(stderr, "Usage: %s [-d filepath] [-r filepath]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (dflag == 0 || iflag == 0) {
        fprintf(stderr, "Error: missing arg(s)\n");
        exit(EXIT_FAILURE);
    }

    if ((dbfd = open(database, O_RDWR | O_APPEND | O_CREAT, 
        S_IRUSR | S_IWUSR)) < 0) {
        handle_err("Open or create database file error");
    }

    if ((itemfd = open(nameitem, O_RDWR | O_APPEND | O_CREAT, 
        S_IRUSR | S_IWUSR)) < 0) {
        handle_err("Open or create nameitem table file error");
    }

    if ((rservfd = open(ROUTE_SERVER, O_RDONLY)) < 0) {
        handle_err("Open routeserver config file error");
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
            /* process the request */
            name_server(connfd, dbfd, itemfd, rservfd, port);
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

int parse_name_pkt(struct name_prtl *pkt, char *data)
{
    int i;
    char len[MAXLENBYTE + 1];

    if (data[1] < 48 || data[1] > 57) {
        return -1;
    } else {
        pkt->type = (int)data[1] - (int)'0';
    }

    for (i = 0; i < MAXLENBYTE; i++) {
        if (data[i + 2] < 48 || data[i + 2] > 57) {
            return -1;
        }
        len[i] = data[i + 2];
    }
    pkt->len = atoi(len);

    for (i = 0; i < MAXNAMESIZE; i++) {
        pkt->name[i] = data[i + 6];
    }

    if ((pkt->data = (char *) malloc(pkt->len + 1)) == NULL) {
        return -1;
    }
    for (i = 0; i < pkt->len; i++) {
        pkt->data[i] = data[i + 38];
    }
    pkt->data[i] = '\0';

    return 0;
}

void gen_name_pkt(struct name_prtl *pkt, char *data)
{
    int i, n;
    char len[MAXLENBYTE + 1];

    data[0] = (char)(((int)'0') + pkt->protocol);
    data[1] = (char)(((int)'0') + pkt->type);

    sprintf(len, "%d", pkt->len);
    n = strlen(len);
    for (i = 0; i < MAXLENBYTE - n; i++) {
        data[i + 2] = '0';
    }
    for (i = 0; i < strlen(len); i++) {
        data[i + 2 + MAXLENBYTE - n] = len[i];
    }

    for (i = 0; i < MAXNAMESIZE; i++) {
        data[i + 6] = pkt->name[i];
    }
    
    i = 0;
    if (pkt->len > 0) {
        for (i = 0; i < pkt->len; i++) {
            data[i + 38] = pkt->data[i];
        }
    }
    data[i + 38] = '\0';
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

int is_local(int itemfd, char *name)
{
    char *buf;
    int flag = -1;
    int n;

    lock_file(itemfd);
    while ((n = readline(itemfd, &buf)) > 0) {
        if (buf[0] == name[0]) {
            flag = 0;
            break;
        }
    }

    return flag;
}

int is_in_database(int dbfd, char *name)
{
    char *buf;
    int result = -1;
    int n, len;

    len = strlen(name);

    lock_file(dbfd);

    if (lseek(dbfd, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return result;
    }

    while ((n = readline(dbfd, &buf)) > 0) {
        result = strncmp(name, buf, len);
        printf("%s\n", buf);
        printf("%d, %s\n", result, name);
        if (result == 0) {
            break;
        }
    }

    // -1 -> error, 0 -> found, 1 -> not found
    if (flag != 0) {
        flag = 1;
    }
    
    unlock_file(dbfd);
    return result;
}

int add_nameitem(int itemfd, char nameitem)
{
    int flag = -1
    char buf[3];
    sprintf(buf, "%c\n", nameitem);

    lock_file(dbfd);
    if (write(itemfd, buf, 2) == 2) {
        flag = 0;
    }
    unlock_file(dbfd);

    return flag;
}

int get_server_info(int fd, char *hostname, char *hostport)
{
    char *eachline;
    int n, i, j, len;
    int flag = -1;

    lock_file(fd);
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return flag;
    }

    while ((n = readline(fd, &eachline)) > 0) {
        if (eachline[0] == '#') {
            continue;
        }

        len = strlen(eachline);
        for (i = 0; i < len; i++) {
            if (eachline[i] == ',') {
                flag = 0;
                break;
            }
            hostname[i] = eachline[i];
        }

        if (flag != 0) {
            continue;
        }

        for (j = i; j < MAXHOSTNAME; j++) {
            hostname[j] = '\0';
        }

        // get port string
        j = 0;
        for (i += 2 ; i < len && j < MAXPORTSIZE - 1; i++) {
            hostport[j++] = eachline[i];
        }
        while (j < MAXPORTSIZE) {
            hostport[j++] = '\0';
        }
        break;
    }

    unlock_file(fd);
    return flag;
}

int route(int rservfd, char nameitem, char *hostipaddr)
{
    int sockfd, status;
    int n;
    int flag = -1;
    char serv_name[MAXHOSTNAME];
    char port[MAXPORTSIZE];
    struct addrinfo hints, *result, *rp;
    struct route_prtl request, reply;
    
    if ((m = get_server_info(rservfd, serv_name, port)) == -1) {
        fprintf(stderr, "Error: failed to parse routeserver name and port\n");
        return flag;
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow IPv4 */
    hints.ai_socktype = SOCK_STREAM; /* stream socket */

    /* initialize socket fd */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "Error: failed to initial socket\n");
        return flag;
    }

    /* get ip address */
    status = getaddrinfo(serv_name, port, &hints, &result);
    if (status != 0) {
        fprintf(stderr, "Error: failed to get addr info\n");
        return flag;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        print_ipaddr((struct sockaddr_in *)rp->ai_addr);
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
    }

    if (rp == NULL) {
        fprintf(stderr, "Error: could not find correct ip to connect\n");
        return flag;
    }

    // initialize the request pkt
    request.protocol = '2';
    request.type = '1';
    request.id = nameitem;
    
    /* send route request */
    if (write(sockfd, &request, sizeof(request)) == -1) {
        fprintf(stderr, "Error: fail to send route prtl request\n");
        return flag;
    }

    if ((n = read(sockfd, &reply, sizeof(reply))) < 0) {
        fprintf(stderr, "Error: fail to get the route reply\n");
        return flag;
    }

    if (reply.type == '4') {
        fprintf(stderr, "Error: routeserver failed\n");
        return flag;
    }

    close(sockfd);
    return flag;
}


int forward_request(int sockfd, char *data, char *ipaddr, int port, ssize_t n)
{
    int sfd, m;
    int flag = -1;
    char recvline[MAXBYTE];
    struct sockaddr_in servaddr;

    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        fprintf(stderr, "Error: fail to initial socket\n");
        return flag;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ipaddr);
    servaddr.sin_port = htons(port);

    if (connect(sfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "Error: connect failed\n");
        return flag;
    }

    m = write(sfd, data, n);
    if (m == EOF && m == -1) {
        fprintf(stderr, "Error: forward failed\n");
        return flag;
    }

    if ((m = read(sfd, recvline, MAXBYTE)) > 0) {
        write(sockfd, recvline, m);
    }

    close(sfd);
    flag = 1;

    return flag;   
}

void add_name(int sockfd, int dbfd, struct name_prtl *name_request)
{
    int len, n, flag;
    char *buf;

    int is_local(int itemfd, char *name)

    flag = is_in_database(dbfd, name_request->name);
    if (flag == 0) {
        write(sockfd, "Already in database\n", 20);
        return;
    } else if (flag == -1) {
        write(sockfd, "Fail to read the datebase, please try it later\n", 50);
        return;
    }

    len = strlen(name_request->name) + 2 + strlen(name_request->data);
    if ((buf = (char*) malloc(len + 1)) == NULL) {
        handle_err("malloc error");
    }
    sprintf(buf, "%s: %s", name_request->name, name_request->data);

    lock_file(dbfd);
    if ((n = write(dbfd, buf, len)) == -1) {
        write(sockfd, "Failed\n", 8);
    } else {
        write(sockfd, "OK\n", 4);
    }
    unlock_file(dbfd);
}

void name_server(int sockfd, int dbfd, int itemfd, int rservfd, int port)
{
    ssize_t n;
    int result, i, flag, k, m;
    char buf[MAXBYTE];
    char *data;
    char hostipaddr[16];
    struct name_prtl name_request, name_reply;

    for ( ; ; ) {
        if ((n = read(sockfd, buf, MAXBYTE)) == 0) {
            return;
        }

        if (buf[0] == '1') {
            if ((data = (char *) malloc(n + 1)) == NULL) {
                handle_err("malloc error");
            }
            for (i = 0; i < n; i ++) {
                data[i] = buf[i];
            }
            
            name_request.protocol = 1;
            if ((result == parse_name_pkt(&name_request, data)) == -1) {
                printf("Error: fail to parse the pkt, invalid format\n");
                break;   
            }

            if (name_request.type > 4 || name_request.type <= 0) {
                printf("Error: invalid type: %d\n", name_request.type);
                break;     
            } else {
                if (name_request.type == 2) { // add new name
                    flag = is_local(itemfd, name_request.name);
                    if (flag == -1) {
                        // ask route server
                        k = route(rservfd, name_request.name[0], hostipaddr);
                        if (k == -1) {
                            printf("Error: route check failed\n");
                            write(sockfd, "Failed\n", 8);
                        } else if (k == 0) {
                            // local
                            m = add_nameitem(itemfd, name_request.name[0]);
                            if (m == -1) {
                                printf("Error: fail to update itemtable\n");
                                write(sockfd, "Failed\n", 8);
                            } else {
                                add_name(sockfd, dbfd, &name_request);
                            }
                        } else if (k == 1) {
                            m = forward_request(sockfd, data, hostipaddr, 
                                port, n);
                            if (m == -1) {
                                printf("Error: fail to forward request\n");
                                write(sockfd, "Failed\n", 8);
                            }
                        }
                    } else {
                        add_name(sockfd, dbfd, &name_request);
                    }

                    break;
                }
            }

        } else {
            write(STDOUT_FILENO, "unknown pkt\n", 13);
        }
    }
}