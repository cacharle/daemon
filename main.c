#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <errno.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <signal.h>

static const char *pid_file_path = "daemon.pid";
static int sock_fd = -1;
static FILE *log_file = NULL;

void log_info(const char *format, ...)
{
    time_t now = time(NULL);
    char *now_string = ctime(&now);
    now_string[strlen(now_string) - 2] = '\0'; // Ends with a \n by default
    fputs(now_string, stdout);
    fputs(" - ", stdout);
    va_list ap;
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    fputc('\n', stdout);
}

void cleanup(void)
{
    log_info("Cleanup");
    if (sock_fd != -1)
        close(sock_fd);
    if (log_file != NULL)
        fclose(log_file);
    remove(pid_file_path);
}

void die(const char *message)
{
    log_info("Error: %s: %s", message, strerror(errno));
    cleanup();
    exit(EXIT_FAILURE);
}

void signal_handler(int signum)
{
    log_info("Received signal: %d", signum);
    cleanup();
    log_info("Quitting after signal");
    exit(EXIT_SUCCESS);
}

int main(void)
{
    pid_t child_pid;
    child_pid = fork();
    if (child_pid == -1)
    {
        log_info("Error: Cannot fork: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    // We have successfuly created a child process, the parent can exit
    if (child_pid > 0)
        exit(EXIT_SUCCESS);
    // In the child/daemon process now
    // Create a new process session (SID), the child process becomes the leader of the session
    // This is done to "unlink" the process from the original terminal and any
    // other kind of things the parent had (signal handling, ..)
    if (setsid() == -1)
    {
        log_info("Error: setsid failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Forking a second time to make the daemon NOT the session leader
    // Which restricts it's permissions more (it cannot take control of a
    // terminal for example)
    child_pid = fork();
    if (child_pid == -1)
    {
        log_info("Error: Cannot fork: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    // We have successfuly created a child process, the parent can exit
    if (child_pid > 0)
        exit(EXIT_SUCCESS);

    pid_t pid = getpid();
    // Note the O_EXCL flag saying the file HAS to be created
    int pid_fd = open(pid_file_path, O_CREAT | O_EXCL | O_WRONLY | O_TRUNC, 0644);
    if (pid_fd == -1)
    {
        log_info("Error: Failed to open pid file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    FILE *pid_file = fdopen(pid_fd, "w");
    assert(pid_file != NULL);
    fprintf(pid_file, "%d", pid);
    fclose(pid_file); // Closes the stream AND the file descriptor (man fdopen)

    const char *log_file_path = "daemon.log";
    log_file = fopen(log_file_path, "a");
    if (log_file == NULL)
        die("Failed to open log file");
    // Set line buffering for stdout, stderr and the logfile
    // See: https://stackoverflow.com/questions/34806490
    // See: man 3 setvbuf
    setlinebuf(log_file);
    setlinebuf(stdout);
    setlinebuf(stderr);
    // Redirect stdout and stderr file descriptors to the log file
    int log_fd = fileno(log_file);
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);

    // Registering signal handler for common signals
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    log_info("Started");
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1)
        die("Failed to create socket");
    // Set the SO_REUSEADDR option to avoid the "Address already in use" error
    // See: https://stackoverflow.com/questions/5106674
    int option = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);
    struct sockaddr_in addr = {
        addr.sin_family = AF_INET,
        addr.sin_port = htons(8042),
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };
    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof addr) == -1)
        die("Failed to bind socket");
    if (listen(sock_fd, 32) == -1)
        die("Failed to listen on socket");
    log_info("Waiting for a connection");
    bool running = true;
    while (running)
    {
        int client_fd = accept(sock_fd, NULL, NULL);
        if (client_fd == -1)
            die("Failed to accept new client");
        while (true)
        {
            char buffer[1024];
            memset(buffer, 0, sizeof buffer);
            ssize_t read_size = read(client_fd, buffer, sizeof buffer - 1);
            if (read_size == -1)
                die("Couldn't read from socket");
            if (read_size == 0)
            {
                log_info("Client disconnected");
                close(client_fd);
                break;
            }
            buffer[read_size] = '\0';
            if (buffer[read_size - 1] == '\n')
                buffer[read_size - 1] = '\0';
            log_info("Read %s", buffer);
            if (strcmp(buffer, "quit") == 0)
            {
                running = false;
                close(client_fd);
                break;
            }
        }
    }

    cleanup();
    log_info("Quitting after 'quit' command");
    return 0;
}
