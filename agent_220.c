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

#define BUFFER_SIZE 16384


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
int get_process_list(char *output, size_t output_size)
{
    FILE *process_file;
    char line[256];
    size_t used = 0;

    process_file = popen("ps -e -o pid=,comm=", "r");

    if (process_file == NULL)
    {
        return -1;
    }

    while (fgets(line, sizeof(line), process_file) != NULL)
    {
        int pid;
        char process_name[128];

        if (sscanf(line, "%d %127s", &pid, process_name) == 2)
        {
            int written;

            written = snprintf(output + used,
                               output_size - used,
                               "%d:%s,",
                               pid,
                               process_name);

            if (written < 0)
            {
                pclose(process_file);
                return -1;
            }

            if ((size_t)written >= output_size - used)
            {
                break;
            }

            used += (size_t)written;
        }
    }

    pclose(process_file);

    if (used > 0 && output[used - 1] == ',')
    {
        output[used - 1] = '\0';
    }
    else
    {
        output[used] = '\0';
    }

    return 0;
}
int execute_allowed_command(const char *command,
                            char *output,
                            size_t output_size)
{
    FILE *process;

    char command_buffer[128];

    /*
     * Only the five allowed commands are accepted.
     */
    if (strcmp(command, "DATE") == 0)
    {
        snprintf(command_buffer,
                 sizeof(command_buffer),
                 "date");
    }
    else if (strcmp(command, "UPTIME") == 0)
    {
        snprintf(command_buffer,
                 sizeof(command_buffer),
                 "uptime");
    }
    else if (strcmp(command, "DISKFREE") == 0)
    {
        snprintf(command_buffer,
                 sizeof(command_buffer),
                 "df -h /");
    }
    else if (strcmp(command, "HOSTNAME") == 0)
    {
        snprintf(command_buffer,
                 sizeof(command_buffer),
                 "hostname");
    }
    else if (strcmp(command, "WHOAMI") == 0)
    {
        snprintf(command_buffer,
                 sizeof(command_buffer),
                 "whoami");
    }
    else
    {
        return -2;
    }

    process = popen(command_buffer, "r");

    if (process == NULL)
    {
        return -1;
    }

    output[0] = '\0';

    while (fgets(output + strlen(output),
                 output_size - strlen(output),
                 process) != NULL)
    {
        if (strlen(output) >= output_size - 1)
        {
            break;
        }
    }

    pclose(process);

    /*
     * EXEC response must be one protocol line.
     * Replace newline characters with spaces.
     */
    for (size_t i = 0; output[i] != '\0'; i++)
    {
        if (output[i] == '\n' || output[i] == '\r')
        {
            output[i] = ' ';
        }
    }

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

              if (strcmp(buffer, "LISTPROC\n") == 0)
{
    char process_list[12000];
    char response[14000];

    memset(process_list, 0, sizeof(process_list));

    if (get_process_list(process_list,
                         sizeof(process_list)) == 0)
    {
        snprintf(response,
                 sizeof(response),
                 "OK PROCS %s SID:%s\n",
                 process_list,
                 SID);

        send_all(client_fd,
                 response,
                 strlen(response));

        printf("[Agent] Sent LISTPROC response.\n");
    }
    else
    {
        char error_response[128];

        snprintf(error_response,
                 sizeof(error_response),
                 "ERR 003 PROCESS_LIST_FAILED SID:%s\n",
                 SID);

        send_all(client_fd,
                 error_response,
                 strlen(error_response));
    }

    continue;
}
         /*
 * EXEC
 */
if (strncmp(buffer, "EXEC ", 5) == 0)
{
    char command_name[64];
    char command_output[4096];
    char response[8192];

    memset(command_name, 0, sizeof(command_name));
    memset(command_output, 0, sizeof(command_output));

    /*
     * Extract command after "EXEC ".
     */
    sscanf(buffer + 5,
           "%63s",
           command_name);

    int result = execute_allowed_command(command_name,
                                         command_output,
                                         sizeof(command_output));

    /*
     * Command is not in the whitelist.
     */
    if (result == -2)
    {
        snprintf(response,
                 sizeof(response),
                 "ERR 002 COMMAND_NOT_ALLOWED SID:%s\n",
                 SID);

        send_all(client_fd,
                 response,
                 strlen(response));

        printf("[Agent] EXEC rejected: %s\n",
               command_name);

        continue;
    }

    /*
     * Execution failed.
     */
    if (result == -1)
    {
        snprintf(response,
                 sizeof(response),
                 "ERR 003 EXEC_FAILED SID:%s\n",
                 SID);

        send_all(client_fd,
                 response,
                 strlen(response));

        continue;
    }

    /*
     * Successful EXEC.
     */
    snprintf(response,
             sizeof(response),
             "OK EXEC_RESULT %s SID:%s\n",
             command_output,
             SID);

    send_all(client_fd,
             response,
             strlen(response));

    printf("[Agent] EXEC successful: %s\n",
           command_name);

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
