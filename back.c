#define _XOPEN_SOURCE 500  //ca sa mearga ftw ul cum trebuie cu toate structurile lui etc - din man ftw
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define login 10
#define mystat 11
#define myfind 12
#define quit 13
#define incorrect 14
#define too_many 15
#define logout 16

bool is_authentcated = false;
bool was_authentcated;
char *myfind_target;

#define handle_err(msg)                       \
    {                                         \
        fprintf(stderr, "errno=%d\n", errno); \
        perror(msg);                          \
        exit(1);                              \
    }
bool check_username(const char *usr);
void auth(char *console_in);
short process_input(char *console_in);
void my_find(char *fname);
void my_stat(char *path);
void *malloc_except(size_t n, const char *debug_msg) {
    void *temp = malloc(n);
    if (temp == NULL)
        handle_err(debug_msg);
    return temp;
}
void write_except(int fd, const void *buf, const char *msg) {
    int n = strlen(buf) + 1;
    size_t result = write(fd, &n, 4);
    if (result != 4) {
        fprintf(stderr, "%s\ncannot write msg len - \nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    result = write(fd, buf, n);
    if (result == -1) {
        fprintf(stderr, "%s write returned -1 - \nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    if (result != n) {
        fprintf(stderr, "%s cannot write whole msg - \nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
}
char *read_except(int fd, const char *msg) {
    int msg_len;
    size_t result = read(fd, &msg_len, 4);
    if (result != 4) {
        fprintf(stderr, "%s cannot read msg len -\nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    char *buf = malloc(msg_len);
    if (buf == NULL) {
        fprintf(stderr, "%s cannot malloc to read message - \nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    result = read(fd, buf, msg_len);
    if (result == -1) {
        fprintf(stderr, "%s read returned -1 - \nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    if (result != msg_len) {
        fprintf(stderr, "%s cannot read whole msg - \nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    return buf;
}
void inform_father_file_attributes(const int fd, const struct stat *sb, const char *fpath) {
    char *msg = malloc_except(10000, "malloc inform father");
    //nu chiar toate de aici dar most of them
    sprintf(msg, "File Size: \t\t%d bytes\n", msg, sb->st_size);
    //https://stackoverflow.com/questions/10610225/linux-c-stat-octal-permissions-masks-concatenate-to-send-to-function
    // !!!! pe wsl toate fisierele au Access: (0777/-rwxrwxrwx), chmod ul nu are efect pe ele
    // ctime nu prea stie sa interpreteze astea de la windows
    sprintf(msg, "%sFile Permissions: \t%o\n", msg, (sb->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)));
    sprintf(msg, "%sLast accesed: \t\t%d\n", msg, ctime(&sb->st_atime));
    sprintf(msg, "%sLast modified: \t\t%d\n", msg, ctime(&sb->st_mtime));
    sprintf(msg, "%sLast change: \t\t%d\n", msg, ctime(&sb->st_ctime));

    // cu asta puteam face rost de creation date, dar am aflat de el prea tarziu :(
    // nu face parte din POSIX tho (cica), deci nu stiu cum ar merge pe WSL

    // struct statx tmp;
    // int res = statx(AT_FDCWD, )

    if (fpath != NULL)
        sprintf(msg, "%sFile path: \t\t%s", msg, fpath);
    write_except(fd, msg, "[back] informing father");
}
void close_except(int fd, const char *msg) {
    int res = close(fd);
    if (res == -1)
        handle_err(msg);
}
int soket_token = -1;
int main(int argc, char *argv[], char *envp[]) {
    int send_fd, recv_fd, soket_fd;
    int old_recv, old_send;
    if (argc != 2) {
        fprintf(stderr, "[back] not enough args recieved, cannot establish conn to frontend\n");
        exit(1);
    }
    int matches = 0;
    matches += sscanf(argv[1], "%d %d %d", &send_fd, &recv_fd, &soket_fd);

    if (matches != 3)
        handle_err("[back] maybe did not recieve ints as file descriptors;\nanyway, cannot estabilsh conn to frontend");

    char *console_in;
    int buff_sz;
    while (1) {
        //stdin readings
        console_in = read_except(recv_fd, "[back] reading from front");
        int res = process_input(console_in);
        switch (res) {
            //---------------------------------------------------- errors
        case quit:
            write_except(send_fd, "quit", "[back] quit");
            exit(1);
        case incorrect:
            write_except(send_fd, "incorrect command", "[back] incorrect cmd");
            break;
        case too_many:
            write_except(send_fd, "incorrect command (too many arguments given)", "[back] too many");
            break;
            //---------------------------------------------------- ok commands
        case login:
            was_authentcated = is_authentcated;
            auth(console_in);
            if (is_authentcated) {
                //send comm "token"
                char final[100];
                sprintf(&final, "Welcome, %s", console_in);
                write_except(send_fd, &final, "[back] success auth");
                soket_token = soket_fd;
                old_recv = recv_fd;
                old_send = send_fd;
                recv_fd = send_fd = soket_token;
                break;
                // am incercat eu o manevra, apoi am vazut ca exemplul din manual
                // cu socket() si connect() are 100 de linii pt client si server,
                // si tema trebuie prezentata maine deci... vad next time
                /*
                // if (socketpair(AF_UNIX, SOCK_STREAM, 0, soket_p) < 0)
                //     handle_err("[back] socketpair");

                // int wr_result = -1;
                // wr_result = write(send_fd, &soket_p[1], 4);
                // if (wr_result != 4)
                //     handle_err("[back] cannot send 1st socket descriptor");

                // wr_result = write(send_fd, &soket_p[0], 4);
                // if (wr_result != 4)
                //     handle_err("[back] cannot send 2nd socket descriptor");
                // printf("[back debug]: sent tokens: %d %d\n", soket_p[1], soket_p[0]);
                // close_except(soket_p[1], "[back] closing one end 1 of soketp");
                // soket_token = soket_p[0];
                */
            } else if (was_authentcated) {
                write_except(send_fd, "Logged out due to invalid credentials.\nPlease log in again.", "[back] failed auth");
                was_authentcated = is_authentcated = false;
                recv_fd = old_recv;
                send_fd = old_send;
                break;
            }
            write_except(send_fd, "Invalid credentials", "[back] failed auth");

            break;
        case mystat:
            if (!is_authentcated) {
                write_except(send_fd, "please authentcate first", "[back] mystat no auth");
                break;
            }
            my_stat(console_in);
            break;
        case myfind:
            if (!is_authentcated) {
                write_except(send_fd, "please authentcate first", "[back] myfind no auth");
                break;
            }
            my_find(console_in);
            break;
        case logout:
            if (is_authentcated) {
                was_authentcated = is_authentcated = false;
                recv_fd = old_recv;
                send_fd = old_send;
                write_except(soket_token, "Logged out.", "[back] logout");
                break;
            }
            write_except(send_fd, "please authentcate first", "[back] no auth logout");
            break;
        }
        free(console_in);
    }
    return 0;
}

short process_input(char *console_in) {
    char arg1[100], arg2[100], arg3[100], arg_foolproof[100];
    arg1[0] = arg2[0] = arg3[0] = arg_foolproof[0] = NULL;
    int args_given = sscanf(console_in, "%s %s %s %s\n", &arg1, &arg2, &arg3, &arg_foolproof);

    //errs si login
    if (args_given > 3)
        return too_many;
    if (args_given == 1) {
        if (strcmp("quit", &arg1) == 0)
            return quit;  //puteam exit dar eh, functia doar "proceseaza" inputul
        if (strncmp("login:", &arg1, 6) == 0) {
            strcpy(console_in, arg1 + 6);
            return login;
        }
        if (strcmp("logout", &arg1) == 0)
            return logout;
        return incorrect;
    }

    //restu
    if (args_given == 2) {
        if (strcmp("mystat", &arg1) == 0) {
            strcpy(console_in, &arg2);
            return mystat;
        }
        if (strcmp("myfind", &arg1) == 0) {
            strcpy(console_in, &arg2);
            return myfind;
        }
        if (strcmp("login:", &arg1) == 0) {
            strcpy(console_in, &arg2);
            return login;
        }
    }
    if (args_given == 3) {
        if (strcmp("login", &arg1) + strcmp(":", &arg2) == 0) {
            strcpy(console_in, &arg3);
            return login;
        }
        return too_many;
    }
    return incorrect;
}

void auth(char *usr) {
    FILE *db = fopen("./db.txt", "r");
    if (db == NULL)
        handle_err("db file opening");
    char db_usr[100];
    is_authentcated = false;
    while (fscanf(db, "%s", &db_usr) != EOF)
        if (strcmp(usr, &db_usr) == 0)
            is_authentcated = true;
    fclose(db);
}

void my_stat(char *path) {
    struct stat file_details;
    if (stat(path, &file_details) == -1)            //mergea facut si cu access dar eh
        if (errno == ENOENT || errno == ENOTDIR) {  //din man stat(2) la return + errors
            write_except(soket_token, "path is incorrect or file does not exist", "[back] mystat no file");
            return 0;
        } else
            handle_err("[back] stat err");
    inform_father_file_attributes(soket_token, &file_details, NULL);
    //https://stackoverflow.com/questions/10610225/linux-c-stat-octal-permissions-masks-concatenate-to-send-to-function
    // !!!! pe wsl toate fisierele au Access: (0777/-rwxrwxrwx), chmod ul nu are efect pe ele

    // https://www.reddit.com/r/bashonubuntuonwindows/comments/du6ae5/chmod_doesnt_work_in_wsl_what_is_not_understood/
    // quote: CHMOD changes file permission of a linux file system. Your files are on a Windows FS.
    // CHMOD does not change file permissions of files stored on NTFS or fat32 file systems.
}

int file_name_exact_match(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    //    ./d1/f1
    if (strcmp(fpath + ftwbuf->base, myfind_target) == 0) {
        inform_father_file_attributes(soket_token, sb, fpath);
        return 1;
    }
    return 0;
}
int file_name_weak_match(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    // printf("performing weak match\n");
    if (strstr(fpath + ftwbuf->base, myfind_target) != NULL) {
        inform_father_file_attributes(soket_token, sb, fpath);
        return 1;
    }
    return 0;
}
void my_find(char *fname) {
    if (strchr(fname, '/') != NULL) {
        write_except(soket_token, "Please enter only the file name, not a path to a file", "[back] myfind err path in name");
        return;
    }
    /*
            0Ring the flags? ce inseamna asta?
             FTW_DEPTH
              If  set, do a post-order traversal, that is, call fn() for the directory itself after handling the con‐
              tents of the directory and its subdirectories.  (By default, each directory is handled before its  con‐
              tents.)
    */
    int flags = FTW_DEPTH;  //cred ca i mai ok asa
    myfind_target = fname;
    int find_result = nftw("./", &file_name_exact_match, 100, flags);
    if (find_result == 0)
        find_result = nftw("./", &file_name_weak_match, 100, flags);

    if (find_result == -1)
        handle_err("[myfind child] nftw err");
    if (find_result == 0)
        write_except(soket_token, "No such file or directory", "[back] myfind no file");
    return 0;
}
