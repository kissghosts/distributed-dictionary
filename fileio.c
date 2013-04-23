#include "nameservice.h"


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

int is_local(int itemfd, char *name)
{
    char *buf;
    int flag = -1;
    int n;

    lock_file(itemfd);
    if (lseek(itemfd, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return flag;
    }

    while ((n = readline(itemfd, &buf)) > 0) {
        if (buf[0] == name[0]) {
            flag = 0;
            break;
        }
    }

    return flag;
}

int is_in_database(int dbfd, char *name)
{
    char *buf;
    int result = -1;
    int n, len;

    len = strlen(name);

    lock_file(dbfd);

    if (lseek(dbfd, 0, SEEK_SET) == -1) {
        perror("lseek error");
        return result;
    }

    while ((n = readline(dbfd, &buf)) > 0) {
        result = strncmp(name, buf, len);
        if (result == 0) {
            break;
        }
    }

    // -1 -> error, 0 -> found, 1 -> not found
    if (result != 0) {
        result = 1;
    }
    
    unlock_file(dbfd);
    return result;
}

int add_nameitem(int itemfd, char nameitem)
{
    int flag = -1;
    char buf[3];
    sprintf(buf, "%c\n", nameitem);

    lock_file(itemfd);
    if (write(itemfd, buf, 2) == 2) {
        flag = 0;
    }
    unlock_file(itemfd);

    return flag;
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