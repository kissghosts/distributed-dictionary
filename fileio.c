#include "nameservice.h"

/*
Usage:  read one line from a file, the read offset will be set automatically 
        (don't use lseek)
Return: bytes in this line, 0 if at end of the file; the line is stored in buf
*/
int readline(int fd, char **buf)
{
    int n, i;
    int length = 0;
    char line[MAXBYTE];
    char cbuf[2];

    for ( ; ; ) {
        if ((n = read(fd, cbuf, 1)) < 0) { /* each time one byte */
            perror("[Error] readline -- read error");
            return -1;
        } else if (n == 0) {
            break; // end of the file
        } else if ((cbuf[0] == '\n')) { /* the last char of the line */
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

    // alloc mem for buf to store the line string
    if ((*buf = (char*) malloc(length + 1)) == NULL) {
        perror("[Error] readline -- malloc error");
        return -1;
    }

    // copy the line
    for (i = 0; i < length && line[i] != '\n'; i++) {
        (*buf)[i] = line[i];
    }
    (*buf)[i] = '\0';

    return length;
}

/*
Usage:  lock a file
Return: none
*/
void lock_file(int fd)
{
    struct flock lock;

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;             /* write lock entire file */

    fcntl(fd, F_SETLKW, &lock);
}

/*
Usage:  release the file lock
Return: none
*/
void unlock_file(int fd)
{
    struct flock lock;

    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;             /* unlock entire file */

    fcntl(fd, F_SETLK, &lock);
}

/*
Usage:  chech whether name[0] is in file itemfd 
Return: 0 if name[0] is in the file; -1 if not
*/
int is_local(int itemfd, char *name)
{
    char *buf;
    int flag = -1;
    int n;

    lock_file(itemfd);
    // set offset to zero 
    if (lseek(itemfd, 0, SEEK_SET) == -1) {
        perror("[Error] lseek error");
        return flag;
    }

    while ((n = readline(itemfd, &buf)) > 0) {
        if (buf[0] == name[0]) { /* found */
            flag = 0;
            break;
        }
    }

    return flag;
}

/*
Usage:  chech whether a specific name is in database file 
Return: -1 -> error, 0 -> found, 1 -> not found
*/
int is_in_database(int dbfd, char *name)
{
    char *buf;
    int result = -1;
    int n, len;

    len = strlen(name);

    if (lseek(dbfd, 0, SEEK_SET) == -1) {
        perror("[Error] is_in_database -- lseek error");
        return -1;
    }

    // check line by line
    while ((n = readline(dbfd, &buf)) > 0) {
        result = strncmp(name, buf, len);
        if (result == 0) {
            if (buf[len] == 58) { // string like "name:"
                break; // found
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

/*
Usage:  chech whether a specific name is in database file, 
        if yes, save the attibute to a string **attr
Return: -1 -> error, 0 -> found, 1 -> not found
*/
int database_lookup(int dbfd, char *name, char **attr)
{
    char *buf;
    int result = -1;
    int n, len, i;

    len = strlen(name);

    if (lseek(dbfd, 0, SEEK_SET) == -1) {
        perror("[Error] database_lookup -- lseek error");
        return -1;
    }

    // check line by line
    while ((n = readline(dbfd, &buf)) > 0) {
        result = strncmp(name, buf, len);
        if (result == 0) {
            if (buf[len] == 58) { /* found */
                if ((*attr = (char*) malloc(n - len - 2)) == NULL) {
                    perror("[Error] database_lookup -- malloc error");
                    return result;
                }

                // copy the attribute part
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

/*
Usage:  add a new nameitem into file itemfd;
Return: 0 if ok; -1 if fail
*/
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

/*
Usage:  get server hostname and port number from config file fd
        save them into the string *hostname and *hostport
Return: 0 if OK, -1 on error
*/
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

    // read line by line
    while ((n = readline(fd, &eachline)) > 0) {
        if (eachline[0] == '#') { // ignore this line
            continue;
        }

        len = strlen(eachline);
        for (i = 0; i < len; i++) { // copy hostname
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

/*
Usage:  turn off some mode on file fd
Return: 0 if OK, -1 on error
*/
int turnoff_fd_mode(int fd, int fmode)
{
    int val;

    // get the original mode
    if ((val = fcntl(fd, F_GETFL, 0)) < 0) {
        return -1;
    }

    val &= ~fmode; // turn off fmode
    
    if (fcntl(fd, F_SETFL, val) < 0) {
        return -1;
    }
    return 0;
}

/*
Usage:  turn on some mode on file fd
Return: 0 if OK, -1 on error
*/
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

/*
Usage:  delete one line from file fd
        first copy the lines which are not to delete to a temp file,
        then copy back to overwrite original one
Return: 0 if OK, -1 on error        
*/
int delete_line(int fd, char *name) 
{
    int len, result, line_len;
    int n, i;
    int tmpfd;
    char hostname[MAXHOSTNAME];
    char copydata[MAXBYTE];
    char *str, *buf, *space_str;

    // generate a unique file name
    if ((n = gethostname(hostname, MAXHOSTNAME)) == -1) {
        return n;
    }

    len = strlen(hostname);
    if ((str = (char *) malloc(len + 5)) == NULL) {
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

    // write back
    while((n = read(tmpfd, copydata, MAXBYTE)) > 0) {
        write(fd, copydata, n);
    }

    // overwrite the remaing part in the end
    if ((space_str = (char *) malloc(line_len)) == NULL) {
        fprintf(stderr, "[Error] delete_line -- malloc error\n");
        close(tmpfd);
        unlink(str);        
        return -1;
    }
    for (i = 0; i < line_len - 2; i++) { /* a space line string */
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