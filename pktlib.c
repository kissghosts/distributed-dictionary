#include "nameservice.h"

/*
Usage:  interpret a string data, and convert it to a struct
Return: 0 if OK, -1 on error; the struct is directly restored via the pointer
*/
int parse_name_pkt(struct name_prtl *pkt, char *data)
{
    int i;
    char len[MAXLENBYTE + 1];

    if (data[1] < 48 || data[1] > 57) { /* illegal char */
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

/*
Usage:  interpret a pkt struct, and convert it to a text string
Return: 0 if OK, -1 on error; the string is directly restored via the pointer
*/
void gen_name_pkt(struct name_prtl *pkt, char *data)
{
    int i, n;
    char len[MAXLENBYTE + 1];

    // first two chars
    data[0] = (char)(((int)'0') + pkt->protocol);
    data[1] = (char)(((int)'0') + pkt->type);

    // length part of the pkt
    sprintf(len, "%d", pkt->len);
    n = strlen(len);
    for (i = 0; i < MAXLENBYTE - n; i++) {
        data[i + 2] = '0';
    }
    for (i = 0; i < strlen(len); i++) {
        data[i + 2 + MAXLENBYTE - n] = len[i];
    }

    // name part
    for (i = 0; i < MAXNAMESIZE; i++) {
        data[i + 6] = pkt->name[i];
    }
    
    // data part
    i = 0;
    if (pkt->len > 0) {
        for (i = 0; i < pkt->len; i++) {
            data[i + 38] = pkt->data[i];
        }
    }
    data[i + 38] = '\0';
}

/*
Usage:  send reply by using name protocl pkt, the pkt is in text string
Return: 0 if OK, -1 on error
*/
int pkt_write(int sockfd, int type, char *name, char *data)
{
    int n, len, total_len;
    char *buf;
    struct name_prtl pkt;

    // construct the pkt
    pkt.protocol = 1;
    pkt.type = type;
    pkt.len = strlen(data);
    strcpy(pkt.name, name);

    if ((pkt.data = (char *) malloc(len + 1)) == NULL) {
        perror("[Error] pkt_write -- malloc error\n");
        return -1;
    }

    strncpy(pkt.data, data, len);

    // copy the data of the pkt to a string
    total_len = 38 + pkt.len + 1;
    if ((buf = (char *) malloc(total_len)) == NULL) {
        perror("[Error] pkt_write -- malloc error\n");
        return -1;
    }

    gen_name_pkt(&pkt, buf);

    if ((n = write(sockfd, buf, len)) == -1) {
        perror("[Error] pkt_write -- write error\n");
    }

    return n;
}