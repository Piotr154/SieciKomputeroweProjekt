#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>


int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in sa;
    int SocketFD;
    int port = atoi(argv[2]);
    printf("addr: %s\n", argv[1]);
    printf("port: %i\n", port);
    SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (SocketFD == -1)
    {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    memset(&sa, 0, sizeof sa);

    sa.sin_addr.s_addr = inet_addr(argv[1]);
    sa.sin_family = AF_INET;

    sa.sin_port = htons(port);

    if (connect(SocketFD, (struct sockaddr *)&sa, sizeof sa) == -1)
    {
        perror("Connect failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Connection accepted \n");
    }

    ssize_t n = write(SocketFD, "AGENT\n", strlen("AGENT\n"));
    if (n <= 0)
    {
        perror("Write failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    char buff[16];
    int id;
    n = read(SocketFD, &buff, sizeof(buff));
    if (n <= 0)
    {
        perror("Read failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }
    buff[n] = '\0';
    id = atoi(buff);
    printf("Assigned Agent ID: %d\n", id);

    fd_set read_fds;
struct timeval tv;

char recvbuf[256];

for (;;)
{
    FD_ZERO(&read_fds);
    FD_SET(SocketFD, &read_fds);

    tv.tv_sec = 10;
    tv.tv_usec = 0;

    int rv = select(SocketFD + 1, &read_fds, NULL, NULL, &tv);

    if (rv == -1)
    {
        perror("select");
        break;
    }
    else if (rv == 0)
    {
        snprintf(buff, sizeof(buff), "HEARTBEAT\n");
        if (write(SocketFD, buff, strlen(buff)) <= 0)
        {
            perror("heartbeat failed");
            break;
        }
    }
    else
    {
        int n = read(SocketFD, recvbuf, sizeof(recvbuf) - 1);
        if (n <= 0)
        {
            printf("Server disconnected\n");
            break;
        }

        recvbuf[n] = '\0';

        if (strncmp(recvbuf, "SHUTDOWN", 8) == 0)
        {
            printf("Received SHUTDOWN, exiting\n");
            //system("shutdown -h now");
            break;
        }
    }
}


    close(SocketFD);
    return EXIT_SUCCESS;
}