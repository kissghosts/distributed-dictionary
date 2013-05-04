#include "nameservice.h"

void route_server(int sockfd, int routefd, struct sockaddr_in *cliaddr);
int find_route_ip(int routefd, char nameitem, char *ipaddr, struct sockaddr_in *cliaddr);

// main func for routing server
int main(int argc, char *argv[])
{
    int listenfd, connfd, routefd;
    int opt;
    int port = 0;
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;
    struct sigaction act1, oact1, act2, oact2;
    act1.sa_handler = sig_chld;
    act1.sa_flags = SA_RESETHAND;
    act2.sa_handler = sig_int;
    act2.sa_flags = SA_RESETHAND;

    // parameter parser
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
        case 'p': /* port number */
            port = atoi(optarg);
            if (port < 65535 && port > 1024) {
                fprintf(stderr, "[Error] illegal port number\n");
                exit(EXIT_FAILURE);
            }
            break;
        default: /* ? */
            fprintf(stderr, "[Usage] %s [-p port]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    
    // open the routetable file
    if ((routefd = open(ROUTE_TABLE, O_RDWR | O_APPEND | O_CREAT, 
        S_IRUSR | S_IWUSR)) < 0) {
        handle_err("[Error] open or create route table file error");
    }

    // initialize the tcp server
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
        handle_err("[Error] socket error");

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
        if (connfd == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                handle_err("[Error] accept error");    
            }
        }

        if ((childpid = fork()) < 0)
            handle_err("[Error] fork error");
        else if (childpid == 0) { /* child process */
            close(listenfd);
            route_server(connfd, routefd, &cliaddr); /* process the request */
            exit(0);
        }

        close(connfd); /* parent closes connected socket */
    }

    exit(EXIT_SUCCESS);
}

/*
Usage:  lookup specific nameitem, if it is in the routetbale, return the 
        corresponding ip address, else add a new mapping using client's ip for
        this nameitem
Return: 1) -1 on error
        2) 0 if no mapping is found, the client's ip will be add into routetable
        3) 1 if found a route pair, the corresponding ip address is restored in
        char *ipaddr
*/
int find_route_ip(int routefd, char nameitem, char *ipaddr, 
    struct sockaddr_in *cliaddr)
{    
    char *eachline, *buf;
    int n, i, j, len;
    int flag = -1;

    lock_file(routefd);
    if (lseek(routefd, 0, SEEK_SET) == -1) {
        perror("[Error] lseek error");
        return flag;
    }

    // search line by line
    while ((n = readline(routefd, &eachline)) > 0) {
        if (eachline[0] == '#') { /* comment, ignore */
            continue;
        }

        if (eachline[0] == nameitem) { // find the nameitem
            // copy the ip address from this line
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
        // get client's ip address
        ipaddr = inet_ntoa(cliaddr->sin_addr);

        if (ipaddr == NULL) {
            flag = -1;
        } else { // write the new mapping to routetable
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

    unlock_file(routefd);
    return flag;
}

/*
Usage:  main logical func for route server, receive the request, and send the 
        reply
Return: none
*/
void route_server(int sockfd, int routefd, struct sockaddr_in *cliaddr)
{
    ssize_t n;
    int flag;
    struct route_prtl request, reply;

    reply.protocol = '2'; /* route_prtl */

    if ((n = read(sockfd, &request, sizeof(request))) < 0) {
        handle_err("[Error] read error");
    }

    if (request.protocol != '2') {
        fprintf(stderr, "[Error] unknown packet\n");
        return;
    }

    if (request.type == '1') { /* lookup */
        reply.id == request.id;
        
        flag = find_route_ip(routefd, request.id, reply.ipaddr, cliaddr);
        if (flag == -1) { /* fail to execute find_route_ip */
            fprintf(stderr, "[Error] fail to open or write route table\n");
            reply.type = '4'; /* fail */
        } else if (flag == 0) {
            /* the client is responesible for the nameitem now */
            reply.type = '3';
        } else if (flag == 1) {
            reply.type = '2'; /* found corresponding ip */
        }

        // send the reply by using the struct
        if (write(sockfd, &reply, sizeof(reply)) == -1) {
            fprintf(stderr, "[Error] fail to sent the reply\n");
        }
    }
}
