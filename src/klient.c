#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    struct sockaddr_in sa;
    int SocketFD;
    int port = atoi(argv[2]);
    char buff[512];
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
        perror("connect failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Connection accepted \n");
    }

    ssize_t n = write(SocketFD, "CLIENT\n", strlen("CLIENT\n"));

    if (n <= 0)
    {
        perror("Write failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    for (;;){
        printf("\n1. Lista agentów\n");
        printf("2. Wyłącz agenta\n");
        printf("3. Wyjście\n");
        printf("> ");
        bzero(buff, sizeof(buff));

        int choice;
        scanf("%d", &choice);

        if (choice == 1)
        {
            write(SocketFD, "LIST\n", 5);

            int n = read(SocketFD, buff, sizeof(buff) - 1);
            buff[n] = '\0';
            printf("\n%s", buff);
        }
        else if (choice == 2)
        {
            int id;
            printf("Podaj ID agenta: ");
            scanf("%d", &id);

            snprintf(buff, sizeof(buff), "KILL %d\n", id);
            write(SocketFD, buff, strlen(buff));
        }
        else if (choice == 3)
        {
            break;
        }
        else
        {
            printf("Nieprawidłowy wybór\n");
        }
    }


    close(SocketFD);
    return EXIT_SUCCESS;
}