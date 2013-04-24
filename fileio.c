#include "nameservice.h"


int readline(int fd, char **buf)
{
    int n, i;
    int length = 0;
    char line[MAXBYTE];
    char cbuf[2];

    for ( ; ; ) {
        if ((n = read(fd, cbuf, 1)) < 0) {
            perror("readline: read error");
            return -1;
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
        perror("readline: malloc error");
        return -1;
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

    if (lseek(dbfd, 0, SEEK_SET) == -1) {
        perror("is_in_database: lseek error\n");
        return -1;
    }

    while ((n = readline(dbfd, &buf)) > 0) {
        result = strncmp(name, buf, len);
        if (result == 0) {
            if (buf[len] == 58) {
                break;
            } else {
                result = 1;
            }
        }
    }

    // -1 -> error, 0 -> found, 1 -> not found
    if (result != 0) {
        result = 1;
    }
    
    return result;
}

int database_lookup(int dbfd, char *name, char **attr)
{
    char *buf;
    int result = -1;
    int n, len, i;

    len = strlen(name);

    if (lseek(dbfd, 0, SEEK_SET) == -1) {
        perror("database_lookup: lseek error\n");
        return -1;
    }

    while ((n = readline(dbfd, &buf)) > 0) {
        result = strncmp(name, buf, len);
        if (result == 0) {
            if (buf[len] == 58) { /* found */
                if ((*attr = (char*) malloc(n - len - 2)) == NULL) {
                    perror("database_lookup: malloc error");
                    return result;
                }

                for (i = len + 2; i < n; i++) {
                    (*attr)[i - len - 2] = buf[i];
                }
                (*attr)[i - len - 2] = '\0';
                break;
            } else {
                result == 1;
            }
        }
    }

    // -1 -> error, 0 -> found, other -> not found
    if (result != 0) {
        result = 1;
    }
    
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

int turnoff_fd_mode(int fd, int fmode)
{
    int val;

    if ((val = fcntl(fd, F_GETFL, 0)) < 0) {
        return -1;
    }

    val &= ~fmode;
    
    if (fcntl(fd, F_SETFL, val) < 0) {
        return -1;
    }
    return 0;
}

int turnon_fd_mode(int fd, int fmode) 
{
    int val;

    if ((val = fcntl(fd, F_GETFL, 0)) < 0) {
        return -1;
    }

    val |= fmode;
    
    if (fcntl(fd, F_SETFL, val) < 0) {
        return -1;
    }
    return 0;
}

int delete_line(int fd, char *name) 
{
    int len, result, line_len;
    int n, i;
    int tmpfd;
    char hostname[MAXHOSTNAME];
    char copydata[MAXBYTE];
    char *str, *buf, *space_str;

    if ((n = gethostname(hostname, MAXHOSTNAME)) == -1) {
        return n;
    }

    len = strlen(hostname);
    if ((str = (char *)malloc(len + 5)) == NULL) {
        return -1;
    }

    sprintf(str, "%s.tmp", hostname);

    // create tmp file use an unique file name "str"
    if ((tmpfd = open(str, O_RDWR | O_CREAT, 
        S_IRUSR | S_IWUSR)) < 0) {
        return -1;
    }

    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("[Error] delete_line -- lseek error\n");
        close(tmpfd);
        unlink(str);
        return -1;
    }

    // copy lines without the name to tmpfile
    len = strlen(name);
    while ((n = readline(fd, &buf)) > 0) {
        result = strncmp(name, buf, len);
        if (result != 0 || (result == 0 && buf[len] != 58)) {
            write(tmpfd, buf, strlen(buf));
            write(tmpfd, "\n", 1);
        }
        line_len = strlen(buf);
    }

    printf("%d\n", line_len);

    // rewrite the fd using lines from tmpfile
    if (turnoff_fd_mode(fd, O_APPEND) == -1) {
        fprintf(stderr, "[Error] turnoff_fd_mode -- fcntl error\n");
        close(tmpfd);
        unlink(str);        
        return -1;
    }

    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("[Error] delete_line -- lseek error\n");
        close(tmpfd);
        unlink(str);
        return -1;
    }

    if (lseek(tmpfd, 0, SEEK_SET) == -1) {
        perror("[Error] delete_line -- lseek error\n");
        close(tmpfd);
        unlink(str);
        return -1;
    }

    while((n = read(tmpfd, copydata, MAXBYTE)) > 0) {
        write(fd, copydata, n);
    }

    if ((space_str = (char *) malloc(line_len)) == NULL) {
        fprintf(stderr, "[Error] delete_line -- malloc error\n");
        close(tmpfd);
        unlink(str);        
        return -1;
    }
    for (i = 0; i < line_len - 2; i++) {
        space_str[i] = 32;
    }
    space_str[i] = '\n';
    write(fd, space_str, line_len - 1);

    if (turnon_fd_mode(fd, O_APPEND) == -1) {
        fprintf(stderr, "[Error] turnon_fd_mode -- fcntl error\n");
        close(tmpfd);
        unlink(str);        
        return -1;
    }

    close(tmpfd);
    unlink(str);
    return 0;
}