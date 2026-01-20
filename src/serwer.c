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
#include <stdbool.h>

enum ConnType
{
    CONN_UNKNOWN,
    CONN_AGENT,
    CONN_CLIENT
};

int main(void)
{
    struct sockaddr_in sa;
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (SocketFD == -1)
    {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    memset(&sa, 0, sizeof sa);

    sa.sin_family = AF_INET;
    sa.sin_port = htons(1100);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(SocketFD, (struct sockaddr *)&sa, sizeof sa) == -1)
    {
        perror("bind failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    if (listen(SocketFD, 10) == -1)
    {
        perror("listen failed");
        close(SocketFD);
        exit(EXIT_FAILURE);
    }

    struct Agent
    {
        int fd;
        int id;
        time_t last_heartbeat;
        bool online;
    };

    struct Agent agents[10];
    int agent_count = 0;

    fd_set master_set, read_fds;
    int max_fd;

    FD_ZERO(&master_set);
    FD_SET(SocketFD, &master_set);
    max_fd = SocketFD;

    enum ConnType conn_type[FD_SETSIZE];
    int conn_agent_id[FD_SETSIZE];

    for (int i = 0; i < FD_SETSIZE; i++)
    {
        conn_type[i] = CONN_UNKNOWN;
        conn_agent_id[i] = -1;
    }
    for (;;)
    {
        read_fds = master_set;

        int rv = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (rv == -1)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }
        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (FD_ISSET(fd, &read_fds))
            {

                if (fd == SocketFD)
                {
                    int newfd = accept(SocketFD, NULL, NULL);
                    FD_SET(newfd, &master_set);
                    if (newfd > max_fd)
                        max_fd = newfd;
                    conn_type[newfd] = CONN_UNKNOWN;
                    conn_agent_id[newfd] = -1;
                }
                else
                {
                    char buff[256];
                    bzero(buff, 256);
                    int n = read(fd, buff, sizeof(buff) - 1);

                    if (n <= 0)
                    {
                        conn_type[fd] = CONN_UNKNOWN;
                        conn_agent_id[fd] = -1;
                        close(fd);
                        FD_CLR(fd, &master_set);

                        for (int i = 0; i < agent_count; i++)
                        {
                            if (agents[i].fd == fd)
                            {
                                agents[i].online = false;
                                printf("Agent %d disconnected\n", agents[i].id);
                                break;
                            }
                        }
                    }
                    else
                    {
                        buff[n] = '\0';

                        if (conn_type[fd] == CONN_UNKNOWN)
                        {
                            if (strncmp(buff, "AGENT", 5) == 0)
                            {
                                if (agent_count >= 10)
                                {
                                    close(fd);
                                    FD_CLR(fd, &master_set);
                                    continue;
                                }

                                int id = agent_count + 1;

                                agents[agent_count].fd = fd;
                                agents[agent_count].id = id;
                                agents[agent_count].last_heartbeat = time(NULL);
                                agents[agent_count].online = true;
                                agent_count++;

                                conn_type[fd] = CONN_AGENT;
                                conn_agent_id[fd] = id;

                                char reply[32];
                                snprintf(reply, sizeof(reply), "%d\n", id);
                                write(fd, reply, strlen(reply));

                                printf("Agent %d registered\n", id);
                            }
                            else if (strncmp(buff, "CLIENT", 6) == 0)
                            {
                                conn_type[fd] = CONN_CLIENT;
                                printf("Client registered\n");
                            }
                            else
                            {
                                conn_type[fd] = CONN_UNKNOWN;
                                conn_agent_id[fd] = -1;
                                close(fd);
                                FD_CLR(fd, &master_set);
                            }

                            continue;
                        }

                        else if (conn_type[fd] == CONN_AGENT && strncmp(buff, "HEARTBEAT", 9) == 0)
                        {
                            int id = conn_agent_id[fd];

                            for (int i = 0; i < agent_count; i++)
                            {
                                if (agents[i].id == id)
                                {
                                    agents[i].last_heartbeat = time(NULL);
                                    agents[i].online = true;
                                    printf("Heartbeat from agent %d\n", id);
                                    break;
                                }
                            }
                        }
                        else if (conn_type[fd] == CONN_CLIENT && strncmp(buff, "LIST", 4) == 0)
                        {
                            char reply[512];
                            int offset = 0;

                            for (int i = 0; i < agent_count; i++)
                            {
                                offset += snprintf(
                                    reply + offset,
                                    sizeof(reply) - offset,
                                    "AGENT %d %s\n",
                                    agents[i].id,
                                    agents[i].online ? "ON" : "OFF");
                            }

                            write(fd, reply, strlen(reply));
                        }
                        else if (conn_type[fd] == CONN_CLIENT && strncmp(buff, "KILL", 4) == 0)
                        {
                            int id;
                            sscanf(buff, "KILL %d", &id);

                            for (int i = 0; i < agent_count; i++)
                            {
                                if (agents[i].id == id && agents[i].online)
                                {
                                    printf("Client requested shutdown of agent %d\n", id);

                                    write(agents[i].fd, "SHUTDOWN\n", 9);
                                    close(agents[i].fd);
                                    FD_CLR(agents[i].fd, &master_set);

                                    agents[i].online = false;
                                    break;
                                }
                            }
                        }

                        else
                        {
                            printf("Unknown connection\n");
                        }
                    }
                }
            }
        }

        time_t now = time(NULL);
        for (int i = 0; i < agent_count; i++)
        {
            if (agents[i].online && now - agents[i].last_heartbeat > 15)
            {
                int fd = agents[i].fd;
                agents[i].online = false;
                printf("Agent %d timed out\n", agents[i].id);
                conn_type[agents[i].fd] = CONN_UNKNOWN;
                conn_agent_id[agents[i].fd] = -1;
                close(agents[i].fd);
                FD_CLR(agents[i].fd, &master_set);
            }
        }
    }

    close(SocketFD);
    return EXIT_SUCCESS;
}