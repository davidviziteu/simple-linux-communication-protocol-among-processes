#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define pp_write 1
#define pp_read 0
const char *fifo_path = "/tmp/fifo_tema1111";
#define handle_err(msg)                       \
    {                                         \
        fprintf(stderr, "errno=%d\n", errno); \
        perror(msg);                          \
        exit(1);                              \
    }

void write_except(int fd, const void *buf, size_t n, const char *msg) {
    size_t result = write(fd, &n, 4);
    if (result != 4) {
        fprintf(stderr, "%s; cannot write msg len - \nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    result = write(fd, buf, n);
    if (result == -1) {
        fprintf(stderr, "%s; write returned -1 - \nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    if (result != n) {
        fprintf(stderr, "%s; cannot write whole msg - \nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
}
char *read_except(int fd, const char *msg) {
    int msg_len;
    size_t result = read(fd, &msg_len, 4);
    if (result != 4) {
        fprintf(stderr, "cannot read msg len - %s\nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    char *buf = malloc(msg_len);
    if (buf == NULL) {
        fprintf(stderr, "cannot malloc to read message - %s\nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    result = read(fd, buf, msg_len);
    if (result == -1) {
        fprintf(stderr, "read returned -1 - %s\nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    if (result != msg_len) {
        fprintf(stderr, "cannot read whole msg - %s\nerrno=%d\n", msg, errno);
        perror("");
        exit(1);
    }
    return buf;
}
void *malloc_except(size_t n, const char *debug_msg) {
    void *temp = malloc(n);
    if (temp == NULL)
        handle_err(debug_msg);
    return temp;
}
void remove_existing_fifo(const char *path) {
    if (access(path, F_OK) == 0)
        if (unlink(path) < 0)
            handle_err("cannot delete existing fifo :(");
}
void close_except(int fd, const char *msg) {
    int res = close(fd);
    if (res == -1)
        handle_err(msg);
}

int main(int argc, char *argv[], char *envp[]) {
    int soket_token, soket_token_close;  // soket
    int send_fd, recv_fd, __pipe[2], __soketp[2];
    //pe pipe doar trimiti date.
    int res = pipe(__pipe);
    if (res == -1)
        handle_err("pipe");
    remove_existing_fifo(fifo_path);  //dc nu merge acum?????????
    res = mkfifo(fifo_path, 0666);
    if (res == -1)
        handle_err("mkfifo");
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, __soketp) < 0)
        handle_err("[front] socketpair");

    int pid_backend;
    pid_backend = fork();
    if (pid_backend < 0)
        handle_err("[front] cannot do the first fork");
    if (pid_backend == 0) {
        //--------------------                            fiu            backend launch
        //FIUL SCRIE IN PIPE SI CITESTE PRIN FIFO
        close_except(__soketp[1], "[back - front.c] closing soket[1]");
        soket_token = __soketp[0];
        close_except(__pipe[pp_read], "[back - front.c] pipe read close");
        recv_fd = open(fifo_path, O_RDONLY);
        if (recv_fd == -1)
            handle_err("[back - front.c] fifo open rdonly");
        char *fd_backend = malloc_except(50, "[back - front.c] malloc for back fd char arr");
        send_fd = __pipe[pp_write];
        sprintf(fd_backend, "%d %d %d", send_fd, recv_fd, soket_token);
        char *back_args[] = {"./back.exe", fd_backend, NULL};
        execvp("./back.exe", back_args);
        //poate trimiti ceva la tata ca sa stie daca a mers exec ul sau nu
        handle_err("[back - front.c] exec failed. cannot start backend");
    }
    //
    //
    //
    //
    //
    //======================================= begins loop =======================================
    close_except(__soketp[0], "[front] closing soket[0]");
    soket_token = __soketp[1];
    close_except(__pipe[pp_write], "[front] pipe wr end closing");
    recv_fd = __pipe[pp_read];
    send_fd = open(fifo_path, O_WRONLY);
    if (send_fd == -1)
        handle_err("[front] fifo open wronly");

    char *console_in = malloc_except(1000, "[front] malloc console in");
    char *back_response;
    printf("\n\navailable commands:\n login : username\n mystat file_path\n myfind file_path\n quit\n\n");

beggining:
    while (1) {
        fgets(console_in, 1000, stdin);
        write_except(send_fd, console_in, strlen(console_in) + 1, "[front] write msg to back");
        back_response = read_except(recv_fd, "[front] read msg from back");
        if (strcmp("quit", back_response) == 0) {
            remove_existing_fifo(fifo_path);
            exit(1);
        }
        printf("%s\n\n", back_response);
        if (strncmp("Welcome,", back_response, strlen("Welcome,")) == 0)
            break;

        free(back_response);
    }
    free(back_response);
    //
    //
    //------------------------- auth ok -------------------------
    //
    //
    // res = 0;
    // res += read(recv_fd, &soket_token, 4);
    // res += read(recv_fd, &soket_token_close, 4);
    // if (res != 8)
    //     handle_err("[front] cannot read auth tokens from back");
    // close_except(soket_token_close, "[front] closing one soket end");

    while (1) {
        fgets(console_in, 1000, stdin);
        write_except(soket_token, console_in, strlen(console_in) + 1, "[front auth ok] write msg to back");
        back_response = read_except(soket_token, "[front auth ok] read msg to back");
        if (strcmp("quit", back_response) == 0) {
            remove_existing_fifo(fifo_path);
            exit(1);
        }
        printf("%s\n\n", back_response);
        if (strncmp("Logged out", back_response, strlen("Logged out")) == 0)
            goto beggining;
        free(back_response);
    }
    return 0;
}