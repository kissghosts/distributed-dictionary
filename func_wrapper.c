#include "nameservice.h"


void handle_err(char *str)
{
    perror(str);
    exit(EXIT_FAILURE);
}

void print_ipaddr(struct sockaddr_in *servaddr)
{
    char ipstr[INET_ADDRSTRLEN];
    void *addr;
    char *ipver;

    addr = &(servaddr->sin_addr);
    inet_ntop(AF_INET, addr, ipstr, sizeof(ipstr));
    printf("Connecting to %s\n", ipstr);
}

int tcp_connect(char *serv_name, char *port) {
    int sockfd, status;
    struct addrinfo hints, *result, *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow IPv4 */
    hints.ai_socktype = SOCK_STREAM; /* stream socket */
    
    /* initialize socket fd */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Error: fail to initialize tcp connection\n");
        return(-1);
    }

    /* get ip address */
    status = getaddrinfo(serv_name, port, &hints, &result);
    if (status != 0) {
        perror("Error: getaddrinfo\n");
        return(-1);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        print_ipaddr((struct sockaddr_in *)rp->ai_addr);
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
    }

    if (rp == NULL) {
        perror("Error: could not find correct ip to connect\n");
        return(-1);
    }

    return(sockfd);
}