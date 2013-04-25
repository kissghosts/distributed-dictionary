#include "nameservice.h"

/*
Usage: write log
Return: none
*/
void log(int fd, char* message) {
   time_t now;
   char buf[256];
   
   time(&now);
   sprintf(buf, "%s %s\n", ctime(&now), message);
   lock_file(fd); /* get the file lock */
   write(fd, buf, strlen(buf));
   unlock_file(fd); /* release lock */
}

/*
Usage:  write error message to stdout and exit
Return: none
*/
void handle_err(char *str)
{
    perror(str);
    exit(EXIT_FAILURE);
}

/*
Usage:  print the ip connecting ip address
Return: none
*/
void print_ipaddr(struct sockaddr_in *servaddr)
{
    char ipstr[INET_ADDRSTRLEN];
    void *addr;
    char *ipver;

    addr = &(servaddr->sin_addr);
    inet_ntop(AF_INET, addr, ipstr, sizeof(ipstr));
    printf("[Info] Connecting to server %s\n", ipstr);
}

/*
Usage:  connect wrapper, use connect_timeo to establish a tcp connect
Return: connfd if OK, -1 on error
*/
int tcp_connect(char *serv_name, char *port) {
    int sockfd, status;
    struct addrinfo hints, *result, *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow IPv4 */
    hints.ai_socktype = SOCK_STREAM; /* stream socket */
    
    /* initialize socket fd */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("[Error] tcp_connect -- fail to initialize socket fd");
        return(-1);
    }

    /* get ip address */
    status = getaddrinfo(serv_name, port, &hints, &result);
    if (status != 0) {
        perror("[Error] tcp_connect -- getaddrinfo error");
        return(-1);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        print_ipaddr((struct sockaddr_in *)rp->ai_addr);
        if (connect_timeo(sockfd, rp->ai_addr, rp->ai_addrlen, 
            CONNECT_TIMEO) == 0) { /* connect with timeout */
            break;
        }
    }

    if (rp == NULL) {
        perror("[Error] tcp_connect -- could not find correct ip");
        return(-1);
    }

    return(sockfd);
}

/*
Usage:  set sig_alarm before conncet, the signal will be send 
        if connection is not established in a specific period
Return: 0 if OK, -1 on error
*/
int connect_timeo(int sockfd, struct sockaddr *addr, socklen_t addrlen, 
    int nsec)
{
    int n;

    signal(SIGALRM, sig_alarm);
    if (alarm(nsec) != 0) {
        printf("connect_timeo: alarm was already set\n");
    }

    if ((n = connect(sockfd, addr, addrlen)) < 0) {
        close(sockfd);
        if (errno == EINTR) {
            errno = ETIMEDOUT;
        }
    }
    alarm(0); /* turn off the alarm */
    
    return(n);
}

/*
Usage:  read wrapper with timeout
Return: 0 if OK, -1 on error
*/
int read_timeo(int fd, char *buf, ssize_t count, int nsec)
{
    int n;

    signal(SIGALRM, sig_alarm);
    alarm(nsec);

    if ((n = read(fd, buf, count)) < 0) {
        if (errno == EINTR) {
            errno = ETIMEDOUT;
        }
    }
    alarm(0); /* turn off the alarm */

    return(n);
}

/*
Usage:  generate a string with hostname and suffix
Return: 0 if OK, -1 on error
*/
int generate_filename(char **str, char *suffix)
{   
    int n, len, i;
    int suflen;
    char hostname[MAXHOSTNAME];
    
    // get hostname
    if ((n = gethostname(hostname, MAXHOSTNAME)) == -1) {
        perror("[Error] get_hostname -- gethostname error");
        return n;
    }

    // alloc mem for the new string
    len = strlen(hostname);
    suflen = strlen(suffix);
    if ((*str = (char *) malloc(len + suflen + 1)) == NULL) {
        return -1;
    }

    // concatenate hostname and suffix in the new string
    for (i = 0; i < len; i++) {
        (*str)[i] = hostname[i];
    }
    for ( ; i < len + suflen; i++) {
        (*str)[i] = suffix[i - len];
    }
    (*str)[i] = '\0';

    return 0;
}
