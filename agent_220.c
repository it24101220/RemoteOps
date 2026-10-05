#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 9410
#define BACKLOG 5

int main(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;

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

    /* 4. Bind to port 9410 */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    printf("[Agent] Bound to port %d.\n", PORT);

    /* 5. Start listening */
    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("[Agent] Listening for Controller connections...\n");

    /* 6. Continuously accept Controllers */
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

        /* Get Controller IP address */
        printf("[Agent] Client IP: %s\n",
               inet_ntoa(client_addr.sin_addr));

        /* Close this client for now */
        close(client_fd);

        printf("[Agent] Controller disconnected.\n");
    }

    /* This code is not reached in the current version */
    close(server_fd);

    return 0;
}

