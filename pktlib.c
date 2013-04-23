#include "nameservice.h"


int parse_name_pkt(struct name_prtl *pkt, char *data)
{
    int i;
    char len[MAXLENBYTE + 1];

    if (data[1] < 48 || data[1] > 57) {
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