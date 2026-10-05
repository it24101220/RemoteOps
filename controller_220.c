#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9410

int main(void)
{
    int sock_fd;

    struct sockaddr_in server_addr;

    char buffer[1024];

    /* 1. Create TCP socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("[Controller] Socket created successfully.\n");

    /* 2. Configure Agent address */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET,
                  SERVER_IP,
                  &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sock_fd);
        return 1;
    }

    /* 3. Connect to Agent */
    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sock_fd);
        return 1;
    }

    printf("[Controller] Connected to Agent %s:%d\n",
           SERVER_IP,
           SERVER_PORT);

    /* 4. Send PING command */
    const char *command = "PING\n";

    send(sock_fd,
         command,
         strlen(command),
         0);

    printf("[Controller] Sent: %s", command);

    /* 5. Receive Agent response */
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_received = recv(sock_fd,
                                  buffer,
                                  sizeof(buffer) - 1,
                                  0);

    if (bytes_received < 0)
    {
        perror("recv");
        close(sock_fd);
        return 1;
    }

    buffer[bytes_received] = '\0';

    printf("[Controller] Received: %s", buffer);

    /* 6. Close connection */
    close(sock_fd);

    printf("[Controller] Disconnected.\n");

    return 0;
}
