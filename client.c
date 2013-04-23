#include "nameservice.h"

void handle_err(char *str);
void name_service(int sockfd, struct name_prtl *pkt);
void print_ipaddr(struct sockaddr_in *servaddr);
int get_server_info(int fd, char *hostname, char *hostport);
void get_attr_input(struct name_prtl *pkt);
void gen_name_pkt(struct name_prtl *pkt, char *data);

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

void handle_err(char *str)
{
    perror(str);
    exit(EXIT_FAILURE);
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

void print_ipaddr(struct sockaddr_in *servaddr)
{
    char ipstr[INET_ADDRSTRLEN];
    void *addr;
    char *ipver;

    addr = &(servaddr->sin_addr);
    inet_ntop(AF_INET, addr, ipstr, sizeof(ipstr));
    printf("Connecting to %s\n", ipstr);
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