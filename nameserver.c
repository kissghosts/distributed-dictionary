#include "nameservice.h"

void handle_err(char *str);
void name_server(int sockfd, int dbfd, int itemfd, int rservfd, int port);
int route(int rservfd, char nameitem, char *hostipaddr);

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

int route(int rservfd, char nameitem, char *hostipaddr)
{
    int sockfd, status;
    int n;
    int flag = -1;
    char serv_name[MAXHOSTNAME];
    char port[MAXPORTSIZE];
    struct addrinfo hints, *result, *rp;
    struct route_prtl request, reply;
    
    if (get_server_info(rservfd, serv_name, port) == -1) {
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
    } else if (reply.type == '2') {
        strcpy(hostipaddr, reply.ipaddr);
        
        printf("%s, %s\n", reply.ipaddr, hostipaddr);

        flag = 1;
    } else if (reply.type == '3') {
        flag = 0;
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