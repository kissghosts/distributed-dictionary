#include "nameservice.h"


void name_server(int sockfd, int dbfd, int itemfd, int rservfd, 
    int logfd, int port);
void handle_request(struct name_prtl *name_request, int itemfd, int dbfd, 
    int rservfd, int sockfd, int logfd, ssize_t n, int port, char *data);
int route(int rservfd, int logfd, char nameitem, char *hostipaddr);
int forward_request(int sockfd, int logfd, char *data, char *ipaddr, int port, 
    ssize_t n);
void add_name(int sockfd, int dbfd, int logfd, struct name_prtl *name_request);
void lookup_name(int sockfd, int dbfd, int logfd, 
    struct name_prtl *name_request);
void delete_name(int sockfd, int dbfd, int logfd, 
    struct name_prtl *name_request);
void update_name(int sockfd, int dbfd, int logfd, 
    struct name_prtl *name_request);

int main(int argc, char *argv[])
{
    int listenfd, connfd, dbfd, itemfd, rservfd, logfd;
    int dflag, iflag, opt, n, i;
    int port = 0;
    pid_t childpid;
    char *database, *nameitem, *logfile;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    struct sigaction act1, oact1, act2, oact2;
    act1.sa_handler = sig_chld;
    act1.sa_flags = SA_RESETHAND;
    act2.sa_handler = sig_int;
    act2.sa_flags = SA_RESETHAND;

    dflag = iflag = 0; /* parameter parer flag, unset at the beginning */
    // parameter parser using getopt
    while ((opt = getopt(argc, argv, "d:i:p:")) != -1) {
        switch (opt) {
        case 'd': /* database file path */
            n = strlen(optarg);

            // copy the input string content to a new string
            if ((database = (char *) malloc(n + 1)) == NULL) {
                handle_err("[Error] main -- malloc error");
            }            
            for (i = 0; i < n; i++) {
                database[i] = optarg[i];
            }
            database[i]  = '\0';
            dflag = 1; /* has been set */
            break;
        case 'i': /* nameindex file path */
            n = strlen(optarg);

            // copy the input string content to a new string
            if ((nameitem = (char *) malloc(n + 1)) == NULL) {
                handle_err("[Error] main -- malloc error");
            }            

            for (i = 0; i < n; i++) {
                nameitem[i] = optarg[i];
            }
            nameitem[i]  = '\0';
            iflag = 1;
            break;
        case 'p': /* server port number */
            port = atoi(optarg);
            if (port < 65535 && port > 1024) {
                fprintf(stderr, "[Error] main -- illegal port number\n");
                exit(EXIT_FAILURE);
            }
            break;
        default: /* ? */
            fprintf(stderr, "[Usage] %s [-d filepath] [-r filepath]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (dflag == 0) { // not input database path
        if (generate_filename(&database, ".db") < 0) {
            fprintf(stderr, "[Error] generate_filename error\n");
            exit(EXIT_FAILURE);
        }
    }

    if (iflag == 0) { // not input name index path
        if (generate_filename(&nameitem, ".index") < 0) {
            fprintf(stderr, "[Error] generate_filename error\n");
            exit(EXIT_FAILURE);
        }
    }

    if (generate_filename(&logfile, ".log") < 0) {
        fprintf(stderr, "[Error] generate_filename error\n");
        exit(EXIT_FAILURE);
    }

    // open needed file
    if ((dbfd = open(database, O_RDWR | O_APPEND | O_CREAT, 
        S_IRUSR | S_IWUSR)) < 0) {
        handle_err("[Error] main -- open or create database file error");
    }

    if ((itemfd = open(nameitem, O_RDWR | O_APPEND | O_CREAT, 
        S_IRUSR | S_IWUSR)) < 0) {
        handle_err("[Error] main -- open or create nameindex table file error");
    }

    if ((rservfd = open(ROUTE_SERVER, O_RDONLY)) < 0) {
        handle_err("[Error] main -- open routeserver config file error");
    }

    if ((logfd = open(logfile, O_RDWR | O_APPEND | O_CREAT, 
        S_IRUSR | S_IWUSR)) < 0) {
        handle_err("[Error] main -- open or create log file error");
    }    

    // initialize the socket and listen to request
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
        handle_err("[Error] main -- socket error");

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

    // signal handler
    if (sigaction(SIGCHLD, &act1, &oact1) < 0) {
        handle_err("sig_chld error");
    }
    if (sigaction(SIGINT, &act2, &oact2) < 0) {
        handle_err("sig_int error");
    }

    /* call accept, wait for a client */ 
    for ( ; ; ) {
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);
        if (connfd < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                handle_err("accept error");
            }
        }

        if ((childpid = fork()) < 0)
            handle_err("fork error");
        else if (childpid == 0) { /* child process */
            close(listenfd);
            /* process the request */
            name_server(connfd, dbfd, itemfd, rservfd, logfd, port);
            exit(0);
        }

        close(connfd); /* parent closes connected socket */
    }

    exit(EXIT_SUCCESS);
}

/*
Usage:  ask the routing server to get the routing info
        this func uses route_prtl to communicate with specific routing server
Return: 1) -1 -> error
        2) 0 -> the local server itself is/will be responsible for this kind of 
        name in the future
        3) 1 -> another server is responsible for the name request, its ip 
        address is restored in char *hostipaddr
*/
int route(int rservfd, int logfd, char nameitem, char *hostipaddr)
{
    int sockfd;
    int n;
    int flag = -1;
    char serv_name[MAXHOSTNAME];
    char port[MAXPORTSIZE];
    struct route_prtl request, reply;
    
    write_log(logfd, "[Info] route -- start to check routing info");

    // read the routing server config, get the ip of it
    if (get_server_info(rservfd, serv_name, port) == -1) {
        fprintf(stderr, "[Error] route -- failed to parse routeserver info\n");
        write_log(logfd, "[Error] route -- failed to parse routeserver info");
        return flag;
    }

    // connect to name server with timeout
    sockfd = tcp_connect(serv_name, port);
    if (sockfd == -1) {
        fprintf(stderr, "[Error] route -- failed to connect\n");
        write_log(logfd, "[Error] route -- failed to connect");
        return flag;   
    }

    // initialize the request pkt
    request.protocol = '2';
    request.type = '1'; /* lookup */
    request.id = nameitem;
    
    /* send route request */
    if (write(sockfd, &request, sizeof(request)) == -1) {
        fprintf(stderr, "[Error] route -- fail to send route prtl request\n");
        write_log(logfd, "[Error] route -- fail to send route prtl request");
        return flag;
    }

    // read the route_prtl struct directly with timeout
    if ((n = read_timeo(sockfd, &reply, sizeof(reply), TRANS_TIMEO)) < 0) {
        fprintf(stderr, "[Error] route -- fail to get the route reply\n");
        write_log(logfd, "[Error] route -- fail to get the route reply");
        return flag;
    }

    if (reply.type == '4') {
        fprintf(stderr, "[Error] route -- routeserver failed\n");
        write_log(logfd, "[Error] route -- routeserver failed");
        return flag;
    } else if (reply.type == '2') {
        strcpy(hostipaddr, reply.ipaddr);
        flag = 1;
    } else if (reply.type == '3') {
        flag = 0;
    }

    close(sockfd);
    return flag;
}

/*
Usage:  forward the name request to the responsible server, and send the reply 
        from that server directly back to the requester
Return: -1 on error, 1 if OK
*/
int forward_request(int sockfd, int logfd, char *data, char *ipaddr, int port, 
    ssize_t n)
{
    int sfd, m;
    int flag = -1;
    char recvline[MAXBYTE];
    struct sockaddr_in servaddr;

    write_log(logfd, "[Info] forward_request -- forward the request");

    // initialize a new socket
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        fprintf(stderr, "[Error] forward_request -- fail to initial socket\n");
        write_log(logfd, "[Error] forward_request -- fail to initial socket");
        return flag;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ipaddr);
    servaddr.sin_port = htons(port);

    // tcp connect with timeout
    if (connect_timeo(sfd, (struct sockaddr *) &servaddr, 
        sizeof(servaddr), CONNECT_TIMEO) < 0) {
        fprintf(stderr, "[Error] forward_request -- connect failed\n");
        write_log(logfd, "[Error] forward_request -- connect failed");
        return flag;
    }

    // forward the request to another server
    m = write(sfd, data, n);
    if (m == EOF && m == -1) {
        fprintf(stderr, "[Error] forward_request -- forward failed\n");
        write_log(logfd, "[Error] forward_request -- forward failed");
        return flag;
    }

    if ((m = read_timeo(sfd, recvline, MAXBYTE, TRANS_TIMEO)) > 0) {
        write(sockfd, recvline, m);
    }

    close(sfd);
    flag = 1;

    return flag;   
}

/*
Usage:  add name operation, add the name to database if possible, 
        and send the reply back
Return: none
*/
void add_name(int sockfd, int dbfd, int logfd, struct name_prtl *name_request)
{
    int len, n, flag, result;
    char *buf;

    write_log(logfd, "[Info] add_name -- adding name to database");

    lock_file(dbfd);

    /* check wheter a same name is in database, and send reply */
    flag = is_in_database(dbfd, name_request->name);
    printf("[Info] add_name -- finish search database\n");
    write_log(logfd, "[Info] add_name -- finish search database");
    if (flag == 0) { /* found */
        pkt_write(sockfd, 8, name_request->name, 
            "the name is already in database");
        return;
    } else if (flag == -1) { /* fail to search */
        pkt_write(sockfd, 8, name_request->name, 
            "fail to read the datebase, please try it later");
        return;
    }

    /* not find existing name, execute the add operation */
    len = strlen(name_request->name) + 2 + strlen(name_request->data);
    if ((buf = (char*) malloc(len + 1)) == NULL) {
        perror("[Error] add_name: malloc error");
        write_log(logfd, "[Error] add_name: malloc error");
        return;
    }

    // generate the new line string
    sprintf(buf, "%s: %s", name_request->name, name_request->data);

    if ((n = write(dbfd, buf, len)) == -1) {
        result = pkt_write(sockfd, 8, name_request->name, 
            "fail to add the name pair into database");
    } else {
        result = pkt_write(sockfd, 7, name_request->name, 
            "ok, the name has been added into database");
    }
    unlock_file(dbfd);

    write_log(logfd, "[Info] add_name: finish adding");

    if (result < 0) {
        fprintf(stderr, "[Error] add_name -- send reply fail\n");
        write_log(logfd, "[Error] add_name -- send reply fail");
    } else {
        fprintf(stdout, "[Info] add_name -- send reply ok\n");
        write_log(logfd, "[Info] add_name -- send reply ok");
    }
    return;
}

/*
Usage:  lookup name in the database, send reply back to the requester
Return: none
*/
void lookup_name(int sockfd, int dbfd, int logfd, struct name_prtl *name_request)
{
    int flag, result;
    char *attr;

    lock_file(dbfd);

    write_log(logfd, "[Info] lookup_name -- start to lookup a name in database");

    flag = database_lookup(dbfd, name_request->name, &attr);
    printf("[Info] lookup_name -- search database\n");
    if (flag == 0) { // found
        // send the attribute as the reply data back
        result = pkt_write(sockfd, 5, name_request->name, attr);
    } else if (flag == 1) {
        result = pkt_write(sockfd, 6, name_request->name, 
            "the name is not found");
    } else if (flag == -1) {
        result = pkt_write(sockfd, 8, name_request->name, 
            "fail to read the datebase, please try it later");
    }

    unlock_file(dbfd);
    write_log(logfd, "[Info] lookup_name -- finish lookup");
    
    if (result < 0) {
        fprintf(stderr, "[Error] lookup_name -- send reply fail\n");
        write_log(logfd, "[Error] lookup_name -- send reply fail");
    } else {
        fprintf(stdout, "[Info] lookup_name -- send reply ok: %s\n", 
            name_request->name);
        write_log(logfd, "[Info] lookup_name -- send reply ok");
    }
    return;
}

/*
Usage:  delete a name pair from database, send the reply
Return: none
*/
void delete_name(int sockfd, int dbfd, int logfd, struct name_prtl *name_request)
{
    int flag, result;

    lock_file(dbfd);
    write_log(logfd, "[Info] delete_name -- start searching the database");

    flag = is_in_database(dbfd, name_request->name);
    printf("[Info] delete_name -- finish searching database\n");
    if (flag == 0) { /* found */
        if (delete_line(dbfd, name_request->name) != 0) {
            result = pkt_write(sockfd, 8, name_request->name, 
            "fail to operate read or write the database");    
        } else { /* delete name pair */
            result = pkt_write(sockfd, 7, name_request->name, 
            "delete the name successfully");
        }
    } else if (flag == 1) { /* not find such name */
        result = pkt_write(sockfd, 8, name_request->name, 
            "the name is in database");
    } else if (flag == -1) { /* error */
        result = pkt_write(sockfd, 8, name_request->name, 
            "fail to read the datebase, please try it later");
    }

    unlock_file(dbfd);
    if (result < 0) {
        fprintf(stderr, "[Error] delete_name -- send reply fail\n");
        write_log(logfd, "[Error] delete_name -- send reply fail");
    } else {
        fprintf(stdout, "[Info] delete_name -- send reply ok: %s\n", 
            name_request->name);
        write_log(logfd, "[Info] delete_name -- send reply ok");
    }
    return;
}

void update_name(int sockfd, int dbfd, int logfd, struct name_prtl *name_request)
{
    int len, n, flag, result;
    char *buf;

    lock_file(dbfd);
    write_log(logfd, "[Info] update_name -- start searching the database");

    flag = is_in_database(dbfd, name_request->name);
    printf("[Info] update_name -- finish searching database\n");
    if (flag == 0) { /* found */
        if (delete_line(dbfd, name_request->name) != 0) {
            result = pkt_write(sockfd, 8, name_request->name, 
            "fail to operate read or write the database");    
        } else { /* deleting the old line succeed */

            // write a new line to the datebase
            len = strlen(name_request->name) + 2 + strlen(name_request->data);
            if ((buf = (char*) malloc(len + 1)) == NULL) {
                perror("[Error] add_name -- malloc error");
                write_log(logfd, "[Error] add_name -- malloc error");
                return;
            }
            sprintf(buf, "%s: %s", name_request->name, name_request->data);

            if ((n = write(dbfd, buf, len)) == -1) {
                result = pkt_write(sockfd, 8, name_request->name, 
                    "fail to update the name pair in database");
            } else {
                result = pkt_write(sockfd, 7, name_request->name, 
                    "ok, the name pair has been updated");
            }
        }
    } else if (flag == 1) {
        result = pkt_write(sockfd, 8, name_request->name, 
            "the name is not found");
    } else if (flag == -1) {
        result = pkt_write(sockfd, 8, name_request->name, 
            "fail to read the datebase, please try it later");
    }

    unlock_file(dbfd);
    if (result < 0) {
        fprintf(stderr, "[Error] update_name -- send reply fail\n");
    } else {
        fprintf(stdout, "[Info] update_name -- send reply ok: %s\n", 
            name_request->name);
    }
    return;
}

/*
Usage:  follow the protocol to handle a request, and call right func
Return: none
*/
void handle_request(struct name_prtl *name_request, int itemfd, int dbfd, 
    int rservfd, int sockfd, int logfd, ssize_t n, int port, char *data)
{
    int flag, result, m;
    char hostipaddr[16];

    // chech which server should be responsible for the request
    flag = is_local(itemfd, name_request->name);
    if (flag == -1) { /* not have this kind of names */
        // ask route server
        printf("[Info] ask routing server for the name\n");
        result = route(rservfd, logfd, name_request->name[0], hostipaddr);
        if (result == -1) { /* fail */
            fprintf(stderr, "[Error] route check failed\n");
            write_log(logfd, "[Error] handle_request -- route check failed");
            pkt_write(sockfd, 8, name_request->name, 
                "fail to check route table");
        } else if (result == 0) { /* new kind of names, add it locally */
            // add new mapping first
            printf("[Info] find the corresponding server\n");
            m = add_nameitem(itemfd, name_request->name[0]);
            if (m == -1) {
                fprintf(stderr, "[Error] fail to update indextable\n");
                write_log(logfd, 
                    "[Error] handle_request -- update indextable error");
                pkt_write(sockfd, 8, name_request->name, 
                    "fail to updata indextable");
            } else {
                if (name_request->type == 2) { /* add new name */
                    printf("[Info] adding new name: %s\n", name_request->name);
                    add_name(sockfd, dbfd, logfd, name_request);
                } else if (name_request->type == 1) { /* lookup */
                    printf("[Info] lookup name: %s\n", name_request->name);
                    lookup_name(sockfd, dbfd, logfd, name_request);
                } else if (name_request->type == 3) { /* delete */
                    printf("[Info] delete name: %s\n", name_request->name);
                    delete_name(sockfd, dbfd, logfd, name_request);
                } else if (name_request->type == 4) { /* update */
                    printf("[Info] update name: %s\n", name_request->name);
                    update_name(sockfd, dbfd, logfd, name_request);
                }
            }
        } else if (result == 1) { /* there is another server which is 
                                    responsible for this kind of names */
            printf("[Info] forward request\n"); 
            m = forward_request(sockfd, logfd, data, hostipaddr, 
                port, n);
            if (m == -1) { /* fail */
                fprintf(stderr, "[Error] fail to forward request\n");
                pkt_write(sockfd, 8, name_request->name, 
                    "[Error] fail to forward pkt");
            }
        }
    } else { /* find this kind of name locally */
        if (name_request->type == 2) { /* add new name */
            printf("[Info] adding new name: %s\n", name_request->name);
            add_name(sockfd, dbfd, logfd, name_request);
        } else if (name_request->type == 1) { /* lookup */
            printf("[Info] lookup name: %s\n", name_request->name);
            lookup_name(sockfd, dbfd, logfd, name_request);
        } else if (name_request->type == 3) { /* delete */
            printf("[Info] delete name: %s\n", name_request->name);
            delete_name(sockfd, dbfd, logfd, name_request);
        } else if (name_request->type == 4) { /* update */
            printf("[Info] update name: %s\n", name_request->name);
            update_name(sockfd, dbfd, logfd, name_request);
        }
    }
}

/*
Usage:  name server main logical func for each server process, 
        receive the pkt and decide what to do
Return: none
*/
void name_server(int sockfd, int dbfd, int itemfd, int rservfd, int logfd, 
    int port)
{
    ssize_t n;
    int result, i, flag, k, m;
    char buf[MAXBYTE];
    char *data;
    char hostipaddr[16];
    struct name_prtl name_request, name_reply;

    if ((n = read(sockfd, buf, MAXBYTE)) <= 0) {
        return;
    }

    if (buf[0] == '1') {
        printf("[Info] receive name request\n");
        if ((data = (char *) malloc(n + 1)) == NULL) {
            perror("malloc error");
            return;
        }
        for (i = 0; i < n; i ++) {
            data[i] = buf[i];
        }
        
        name_request.protocol = 1;
        if ((result = parse_name_pkt(&name_request, data)) == -1) {
            fprintf(stderr, "[Error] fail to parse the pkt, invalid format\n");
            pkt_write(sockfd, 8, name_request.name, "[Error] unknown pkt");
            return;
        }

        if (name_request.type > 4 || name_request.type <= 0) {
            // ignore
            fprintf(stderr, "[Error] invalid type: %d\n", name_request.type);
            return;     
        } else {
            handle_request(&name_request, itemfd, dbfd, rservfd, sockfd, logfd, 
                n, port, data);
        }

    } else {
        fprintf(stderr, "[Error] unknown pkt received\n");
    }
}