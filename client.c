#include "nameservice.h"

void name_service(int sockfd, struct name_prtl *pkt);
void get_attr_input(struct name_prtl *pkt);

int main(int argc, char *argv[])
{
    int sockfd, hostfd;
    int flags, opt, i, n, m;
    char serv_name[MAXHOSTNAME];
    char port[MAXPORTSIZE];
    char name[MAXNAMESIZE + 1];
    struct name_prtl request;

    // get options
    flags = 0;
    name[0] = '\0';
    while ((opt = getopt(argc, argv, "adlun:")) != -1) {
        switch (opt) {
        case 'l': /* lookup */
            flags = 1;
            break;
        case 'a': /* add */
            flags = 2;
            break;
        case 'd': /* delete */
            flags = 3;
            break;
        case 'u': /* update */
            flags = 4;
            break;
        case 'n': /* name */
            n = strlen(optarg);
            if (n > MAXNAMESIZE) {
                fprintf(stderr, 
                "[Error] the name is too long, its maxsize is %d\n", MAXNAMESIZE);
                exit(EXIT_FAILURE);
            }

            // copy the input string to name[]
            for (i = 0; i < n; i++) {
                name[i] = optarg[i];
            }
            for ( ; i < MAXNAMESIZE + 1; i++) {
                name[i] = '\0';
            }

            break;
        default: /* ? */
            fprintf(stderr, "[Usage] %s [-adlu] [-n name]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // check input parameters
    if (flags == 0 || name[0] == '\0') {
        fprintf(stderr, "[Error] missing args\n");
        fprintf(stderr, "[Usage] %s [-adlu] [-n name]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (!((name[0] >= 48 && name[0] <= 57) || (name[0] >= 65 && name[0] <= 90) 
        || (name[0] >= 97 && name[0] <= 122))) {
        fprintf(stderr, "[Error] illegal name\n        ");
        fprintf(stderr, "the name should begin with a letter or a number\n");
        exit(EXIT_FAILURE);
    }

    // open the server config file
    if ((hostfd = open(CONF_FILE, O_RDONLY)) < 0) {
        handle_err("Open config file error");
    }
    if ((m = get_server_info(hostfd, serv_name, port)) == -1) {
        fprintf(stderr, "Error: failed to parse server name and port\n");
        exit(EXIT_FAILURE);
    }

    // connect to name server with timeout
    sockfd = tcp_connect(serv_name, port); /* tcp_connect in func_wrapper */
    if (sockfd == -1) {
        fprintf(stderr, "[Error] failed to connect to the server\n");
        exit(EXIT_FAILURE);
    }

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

/*
Usage:  ask user input the attribute from stdin, used by add and update
Return: none, directly modify the struct parameter
*/
void get_attr_input(struct name_prtl *pkt)
{
    int n;
    char buf[MAXBYTE];
    char info[] = "Please input the attribute:\n"; /* used for write func */

    write(STDIN_FILENO, info, sizeof(info));
    if ((n = read(STDIN_FILENO, buf, MAXBYTE)) > 0) {
        if ((pkt->data = (char *) malloc(n)) == NULL) {
            handle_err("[Error] get_attr_input -- malloc error");
        }

        pkt->len = n;
        strncpy(pkt->data, buf, n); /* copy the first n bytes */
    }
}

/*
Usage:  main func after connecting to server, send request, interprate reply 
Return: none
*/
void name_service(int sockfd, struct name_prtl *pkt)
{
    char *data, *str;
    char buf[MAXBYTE];
    int len, n, i;
    struct name_prtl reply;

    /* alloc memory for request pkt, the name_prtl struct can not be send 
       directly: 1) binary type may have different bit order
                 2) unfixed string in the struct
       here we need to transform the struct into a text string
    */
    len = 38 + pkt->len + 1;
    if ((data = (char *) malloc(len)) == NULL) {
        handle_err("[Error] name_service -- malloc error");
    }

    // transform the pkt struct to a text string
    gen_name_pkt(pkt, data);

    // send the text string
    if (write(sockfd, data, len) == -1) {
        handle_err("[Error] name_service -- write error to server");
    }

    // socket read with timeout
    if ((n = read_timeo(sockfd, buf, MAXBYTE, TRANS_TIMEO)) < 0) {
        fprintf(stderr, "[Error] name_service -- read_timeo error");
    } else if (n == 0) {
        fprintf(stderr, "[Error] name_service -- read timeout");
    }

    if (buf[0] != '1') {
        fprintf(stderr, "[Error] name_service -- receive unknown pkt\n");
        return;
    }

    // alloc mem for parsing the pkt
    if ((str = (char *) malloc(n + 1)) == NULL) {
        handle_err("[Error] name_service -- malloc error");
    }
    for (i = 0; i < n; i ++) { /* copy the text data */
        str[i] = buf[i];
    }
    str[i] = '\0';
    
    // initialize the reply pkt structure
    reply.protocol = 1;
    if ((n = parse_name_pkt(&reply, str)) == -1) {
        fprintf(stderr, "[Error] name_service -- fail to parse pkt\n");
        return;
    }

    // check the names of request and reply
    if (strcmp(reply.name, pkt->name) != 0) {
        printf("[Error] name_service -- unmatching reply\n");
        return;
    }

    // print message data get from server
    if (reply.type == 5) {
        printf("[Info] attribute: ");
    } else if (reply.type == 6 || reply.type == 7) {
        printf("[Info] ");
    } else {
        printf("[Error] ");
    }
    printf("%s\n", reply.data);
}
