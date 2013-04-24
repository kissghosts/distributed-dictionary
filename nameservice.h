#ifndef __NAME_SERVICE_H
#define __NAME_SERVICE_H

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#define CONF_FILE "nameserver.config"
#define ROUTE_SERVER "routeserver.config"
#define ROUTE_TABLE "routetable.txt"
#define MAXBYTE 1024
#define MAXLENBYTE 4       /* bytes for length in a packet */
#define MAXHOSTNAME 128    /* host name in serverlist.txt*/
#define MAXPORTSIZE 6      /* string length for restore port */
#define MAXNAMESIZE 32
#define LISTEN_BACKLOG 5
#define SERV_PORT 56284
#define CONNECT_TIMEO 5
#define TRANS_TIMEO 10


#ifndef HAVE_NAMEPTRL_STRUCT
#define HAVE_NAMEPTRL_STRUCT
struct name_prtl {
    int protocol;
    int type;
    int len;
    char name[MAXNAMESIZE + 1];
    char *data;
};
#endif

#ifndef HAVE_ROUTEPTRL_STRUCT
#define HAVE_ROUTEPTRL_STRUCT
struct route_prtl {
    char protocol;
    char type;
    char id;
    char ipaddr[16];
};
#endif

// typedef void (*sighandler_t)(int);

// func_wrapper.c
void handle_err(char *str);
void print_ipaddr(struct sockaddr_in *servaddr);
int tcp_connect(char *serv_name, char *port);
int connect_timeo(int sockfd, struct sockaddr *addr, socklen_t addrlen, int nsec);
ssize_t read_timeo(int fd, char *buf, ssize_t count, int nsec);

// fileio.c
int readline(int fd, char **buf);
void lock_file(int fd);
void unlock_file(int fd);
int get_server_info(int fd, char *hostname, char *hostport);
int is_local(int itemfd, char *name);
int is_in_database(int dbfd, char *name);
int add_nameitem(int itemfd, char nameitem);

// pktlib.c
int parse_name_pkt(struct name_prtl *pkt, char *data);
void gen_name_pkt(struct name_prtl *pkt, char *data);
int pkt_write(int sockfd, int type, char *name, char *data);

// signalhandler.c
void sig_chld(int signo);
void sig_int(int signo);
void sig_alarm(int signo);

// log.c


#endif /* __NAME_SERVICE_H */