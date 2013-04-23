#include "nameservice.h"

void name_service(int sockfd, struct name_prtl *pkt);
void get_attr_input(struct name_prtl *pkt);

int main(int argc, char *argv[])
{
    int sockfd, status;
    int flags, opt, i, n;
    int hostfd, m;
    char serv_name[MAXHOSTNAME];
    char port[MAXPORTSIZE];
    char name[MAXNAMESIZE + 1];
    struct addrinfo hints, *result, *rp;
    struct name_prtl request;

    // get options
    flags = 0;
    name[0] = '\0';
    while ((opt = getopt(argc, argv, "adlun:")) != -1) {
        switch (opt) {
        case 'l':
            flags = 1;
            break;
        case 'a':
            flags = 2;
            break;
        case 'd':
            flags = 3;
            break;
        case 'u':
            flags = 4;
            break;
        case 'n':
            n = strlen(optarg);
            if (n > MAXNAMESIZE) {
                fprintf(stderr, 
                "Error: the name is too long, its maxsize is %d\n", MAXNAMESIZE);
                exit(EXIT_FAILURE);
            }

            for (i = 0; i < n; i++) {
                name[i] = optarg[i];
            }
            for ( ; i < MAXNAMESIZE + 1; i++) {
                name[i] = '\0';
            }

            break;
        default: /* ? */
            fprintf(stderr, "Usage: %s [-adlu] [-n name]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (flags == 0 || name[0] == '\0') {
        fprintf(stderr, "Error: missing args\n");
        fprintf(stderr, "Usage: %s [-adlu] [-n name]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (!((name[0] >= 48 && name[0] <= 57) || (name[0] >= 65 && name[0] <= 90) 
        || (name[0] >= 97 && name[0] <= 122))) {
        fprintf(stderr, "Error: illegal name\n       ");
        fprintf(stderr, "the name should begin with a letter or a number\n");
        exit(EXIT_FAILURE);
    }

    if ((hostfd = open(CONF_FILE, O_RDONLY)) < 0) {
        handle_err("Open config file error");
    }
    if ((m = get_server_info(hostfd, serv_name, port)) == -1) {
        fprintf(stderr, "Error: failed to parse server name and port\n");
        exit(EXIT_FAILURE);
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow IPv4 */
    hints.ai_socktype = SOCK_STREAM; /* stream socket */

    /* initialize socket fd */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        handle_err("socket error");

    /* get ip address */
    status = getaddrinfo(serv_name, port, &hints, &result);
    if (status != 0)
        handle_err("getaddrinfo error");

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        print_ipaddr((struct sockaddr_in *)rp->ai_addr);
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
    }

    if (rp == NULL)
        handle_err("Could not find correct ip to connect");

    // initialize the request pkt
    request.protocol = 1;
    request.type = flags;
    strncpy(request.name, name, MAXNAMESIZE + 1);
    if (flags == 1 || flags == 3) {
        request.len = 0;
    } else {
        get_attr_input(&request); /* get the attribute string */
    }
    
    /* client */
    name_service(sockfd, &request); 

    close(sockfd);
    exit(EXIT_SUCCESS);
}

void get_attr_input(struct name_prtl *pkt)
{
    int n;
    char buf[MAXBYTE];
    char info[] = "Please input the attribute:\n"; /* used for write func */

    write(STDIN_FILENO, info, sizeof(info));
    if ((n = read(STDIN_FILENO, buf, MAXBYTE)) > 0) {
        if ((pkt->data = (char *) malloc(n)) == NULL) {
            handle_err("malloc error before sending data");
        }

        pkt->len = n;
        strncpy(pkt->data, buf, n);
    }
}

void name_service(int sockfd, struct name_prtl *pkt)
{
    char *data;
    char buf[MAXBYTE];
    int len, n;

    len = 38 + pkt->len + 1;
    if ((data = (char *) malloc(len)) == NULL) {
        handle_err("malloc error before sending data");
    }

    gen_name_pkt(pkt, data);

    if (write(sockfd, data, len) == -1) {
        handle_err("write error to server");
    }

    if ((n = read(sockfd, buf, MAXBYTE)) > 0) {
        write(STDOUT_FILENO, buf, n);
    }
}