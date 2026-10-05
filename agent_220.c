#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BACKLOG 5
#define SID "0221"

int main(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;

    char buffer[1024];

    /* 1. Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("[Agent] Socket created successfully.\n");

    /* 2. Allow address reuse */
    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    /* 3. Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    /* 4. Bind */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("[Agent] Bound to port %d.\n", PORT);

    /* 5. Listen */
    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[Agent] Listening for Controller connections...\n");

    /* 6. Accept Controllers continuously */
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

        /* Clear receive buffer */
        memset(buffer, 0, sizeof(buffer));

        /* Receive command */
        ssize_t bytes_received = recv(client_fd,
                                      buffer,
                                      sizeof(buffer) - 1,
                                      0);

        if (bytes_received < 0)
        {
            perror("recv");
            close(client_fd);
            continue;
        }

        buffer[bytes_received] = '\0';

        printf("[Agent] Received: %s", buffer);

        /* Handle PING command */
        if (strcmp(buffer, "PING\n") == 0)
        {
            const char *response =
                "OK PONG SID:0221\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);

            printf("[Agent] Sent: %s", response);
        }
        else
        {
            const char *response =
                "ERR 400 UNKNOWN_COMMAND SID:0221\n";

            send(client_fd,
                 response,
                 strlen(response),
                 0);

            printf("[Agent] Sent: %s", response);
        }

        close(client_fd);

        printf("[Agent] Controller disconnected.\n");
    }

    close(server_fd);

    return 0;
}

