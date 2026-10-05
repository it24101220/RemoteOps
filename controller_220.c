#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9410

#define BUFFER_SIZE 1024

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

int main(void)
{
    int sock_fd;

    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd < 0)
    {
        perror("socket");
        return 1;
    }

    printf("[Controller] Socket created successfully.\n");

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

    /*
     * AUTH
     */
    const char *auth_command = "AUTH OPS-1220\n";

    send_all(sock_fd,
             auth_command,
             strlen(auth_command));

    printf("[Controller] Sent: %s", auth_command);

    if (recv_line(sock_fd,
                  buffer,
                  sizeof(buffer)) < 0)
    {
        printf("[Controller] Connection closed.\n");
        close(sock_fd);
        return 1;
    }

    printf("[Controller] Received: %s", buffer);

    /*
     * SYSINFO
     */
    const char *sysinfo_command = "SYSINFO\n";

    send_all(sock_fd,
             sysinfo_command,
             strlen(sysinfo_command));

    printf("[Controller] Sent: %s", sysinfo_command);

    if (recv_line(sock_fd,
                  buffer,
                  sizeof(buffer)) < 0)
    {
        printf("[Controller] Connection closed.\n");
        close(sock_fd);
        return 1;
    }

    printf("[Controller] Received: %s", buffer);

    /*
     * QUIT
     */
    const char *quit_command = "QUIT\n";

    send_all(sock_fd,
             quit_command,
             strlen(quit_command));

    printf("[Controller] Sent: %s", quit_command);

    if (recv_line(sock_fd,
                  buffer,
                  sizeof(buffer)) < 0)
    {
        printf("[Controller] Connection closed.\n");
        close(sock_fd);
        return 1;
    }

    printf("[Controller] Received: %s", buffer);

    close(sock_fd);

    printf("[Controller] Disconnected.\n");

    return 0;
}
