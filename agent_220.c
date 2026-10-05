#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BACKLOG 5
#define SID "0221"
#define AUTH_TOKEN "OPS-1220"

#define BUFFER_SIZE 1024

/*
 * Send all bytes in a buffer.
 */
int send_all(int socket_fd, const char *data, size_t length)
{
    size_t total_sent = 0;

    while (total_sent < length)
    {
        ssize_t sent = send(socket_fd,
                            data + total_sent,
                            length - total_sent,
                            0);

        if (sent <= 0)
        {
            return -1;
        }

        total_sent += sent;
    }

    return 0;
}

/*
 * Receive one line ending with '\n'.
 */
int recv_line(int socket_fd, char *buffer, size_t size)
{
    size_t position = 0;

    while (position < size - 1)
    {
        char character;

        ssize_t received = recv(socket_fd,
                                &character,
                                1,
                                0);

        if (received <= 0)
        {
            return -1;
        }

        buffer[position++] = character;

        if (character == '\n')
        {
            break;
        }
    }

    buffer[position] = '\0';

    return 0;
}

/*
 * Get system information.
 */
int get_sysinfo(double *cpu_load,
                long *memory_used_mb,
                long *uptime_sec)
{
    FILE *file;

    /* CPU load */
    file = fopen("/proc/loadavg", "r");

    if (file == NULL)
    {
        return -1;
    }

    if (fscanf(file, "%lf", cpu_load) != 1)
    {
        fclose(file);
        return -1;
    }

    fclose(file);

    /* Memory */
    long mem_total_kb = 0;
    long mem_available_kb = 0;

    file = fopen("/proc/meminfo", "r");

    if (file == NULL)
    {
        return -1;
    }

    char line[256];

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (sscanf(line, "MemTotal: %ld kB",
                   &mem_total_kb) == 1)
        {
            continue;
        }

        if (sscanf(line, "MemAvailable: %ld kB",
                   &mem_available_kb) == 1)
        {
            continue;
        }
    }

    fclose(file);

    *memory_used_mb =
        (mem_total_kb - mem_available_kb) / 1024;

    /* Uptime */
    double uptime_value;

    file = fopen("/proc/uptime", "r");

    if (file == NULL)
    {
        return -1;
    }

    if (fscanf(file, "%lf", &uptime_value) != 1)
    {
        fclose(file);
        return -1;
    }

    fclose(file);

    *uptime_sec = (long)uptime_value;

    return 0;
}

int main(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("[Agent] Socket created successfully.\n");

    int opt = 1;

    if (setsockopt(server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("[Agent] Bound to port %d.\n", PORT);

    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[Agent] Listening for Controller connections...\n");

    while (1)
    {
        client_len = sizeof(client_addr);

        client_fd = accept(server_fd,
                           (struct sockaddr *)&client_addr,
                           &client_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("[Agent] Controller connected.\n");

        int authenticated = 0;

        while (1)
        {
            char buffer[BUFFER_SIZE];

            if (recv_line(client_fd,
                          buffer,
                          sizeof(buffer)) < 0)
            {
                printf("[Agent] Controller disconnected.\n");
                break;
            }

            printf("[Agent] Received: %s", buffer);

            /*
             * AUTH
             */
            if (strncmp(buffer, "AUTH ", 5) == 0)
            {
                char token[256];

                memset(token, 0, sizeof(token));

                sscanf(buffer + 5,
                       "%255s",
                       token);

                if (strcmp(token, AUTH_TOKEN) == 0)
                {
                    authenticated = 1;

                    char response[128];

                    snprintf(response,
                             sizeof(response),
                             "OK AUTHENTICATED SID:%s\n",
                             SID);

                    send_all(client_fd,
                             response,
                             strlen(response));

                    printf("[Agent] Authentication successful.\n");
                    printf("[Agent] Sent: %s", response);
                }
                else
                {
                    char response[128];

                    snprintf(response,
                             sizeof(response),
                             "ERR 001 AUTH_FAILED SID:%s\n",
                             SID);

                    send_all(client_fd,
                             response,
                             strlen(response));

                    printf("[Agent] Authentication failed.\n");
                }

                continue;
            }

            /*
             * QUIT can be handled after authentication.
             */
            if (strcmp(buffer, "QUIT\n") == 0)
            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "OK BYE SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));

                printf("[Agent] Sent: %s", response);

                break;
            }

            /*
             * Reject all commands before authentication.
             */
            if (!authenticated)
            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "ERR 001 AUTH_REQUIRED SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));

                printf("[Agent] Authentication required.\n");

                continue;
            }

            /*
             * SYSINFO
             */
            if (strcmp(buffer, "SYSINFO\n") == 0)
            {
                double cpu_load;
                long memory_used_mb;
                long uptime_sec;

                if (get_sysinfo(&cpu_load,
                                &memory_used_mb,
                                &uptime_sec) == 0)
                {
                    char response[256];

                    snprintf(response,
                             sizeof(response),
                             "OK SYSINFO %.2f %ld %ld SID:%s\n",
                             cpu_load,
                             memory_used_mb,
                             uptime_sec,
                             SID);

                    send_all(client_fd,
                             response,
                             strlen(response));

                    printf("[Agent] Sent: %s", response);
                }
                else
                {
                    char response[128];

                    snprintf(response,
                             sizeof(response),
                             "ERR 003 SYSINFO_FAILED SID:%s\n",
                             SID);

                    send_all(client_fd,
                             response,
                             strlen(response));
                }

                continue;
            }

            /*
             * Unknown command.
             */
            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "ERR 400 UNKNOWN_COMMAND SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));
            }
        }

        close(client_fd);

        printf("[Agent] Controller connection closed.\n");
    }

    close(server_fd);

    return 0;
}
