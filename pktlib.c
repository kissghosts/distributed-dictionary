#include "nameservice.h"

void gen_name_pkt(struct name_prtl *pkt, char *data)
{
    int i;
    char len[MAXLENBYTE + 1];

    data[0] = (char)(((int)'0') + pkt->protocol);
    data[1] = (char)(((int)'0') + pkt->type);

    sprintf(len, "%d", pkt->len);
    for (i = 0; i < MAXLENBYTE; i++) {
        data[i + 2] = len[i];
    }

    for (i = 0; i < MAXNAMESIZE; i++) {
        data[i + 6] = pkt->name[i];
    }
    
    if (pkt->len > 0) {
        for (i = 0; i < pkt->len; i++) {
            data[i + 38] = pkt->data[i];
        }
    }
    data[i + 38] = '\0';
}