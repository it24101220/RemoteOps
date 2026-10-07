#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#define PORT 9410
#define BACKLOG 5
#define SID "0221"
#define AUTH_TOKEN "OPS-1220"

#define BUFFER_SIZE 16384
#define MAX_FILE_SIZE (10 * 1024 * 1024)

#define LOG_FILE "remoteops_IT24101220.log"

void write_log(const char *event)
{
    FILE *log_file;
    time_t now;
    struct tm *tm_info;
    char timestamp[64];

    now = time(NULL);
    tm_info = localtime(&now);

    if (tm_info == NULL)
    {
        return;
    }

    strftime(timestamp,
             sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S",
             tm_info);

    log_file = fopen(LOG_FILE, "a");

    if (log_file == NULL)
    {
        return;
    }

    fprintf(log_file,
            "[%s] %s\n",
            timestamp,
            event);

    fclose(log_file);
}



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

int recv_exact(int socket_fd, char *buffer, size_t length)
{
    size_t total_received = 0;

    while (total_received < length) {
        ssize_t received = recv(socket_fd,
                                buffer + total_received,
                                length - total_received,
                                0);

        if (received <= 0) {
            return -1;
        }

        total_received += (size_t)received;
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


struct monitor_context
{
    int udp_fd;
    struct sockaddr_in destination;
};

static void get_monitor_stats(double *cpu,
                              long *mem,
                              unsigned long *uptime)
{
    FILE *fp;
    char line[256];

    *cpu = 0.0;
    *mem = 0;
    *uptime = 0;

    fp = fopen("/proc/loadavg", "r");
    if (fp != NULL)
    {
        fscanf(fp, "%lf", cpu);
        fclose(fp);
    }

    long total = 0;
    long available = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp != NULL)
    {
        while (fgets(line, sizeof(line), fp) != NULL)
        {
            sscanf(line, "MemTotal: %ld kB", &total);
            sscanf(line, "MemAvailable: %ld kB", &available);
        }

        fclose(fp);

        if (total > 0)
        {
            *mem = (total - available) / 1024;
        }
    }

    fp = fopen("/proc/uptime", "r");
    if (fp != NULL)
    {
        double value = 0;

        fscanf(fp, "%lf", &value);

        *uptime = (unsigned long)value;

        fclose(fp);
    }
}

static void *monitor_thread_function(void *arg)
{
    struct monitor_context *ctx =
        (struct monitor_context *)arg;

    while (1)
    {
        double cpu;
        long mem;
        unsigned long uptime;

        char message[256];

        get_monitor_stats(&cpu,
                          &mem,
                          &uptime);

        snprintf(message,
                 sizeof(message),
                 "SYSINFO %.2f %ld %lu SID:%s\n",
                 cpu,
                 mem,
                 uptime,
                 SID);

        sendto(ctx->udp_fd,
               message,
               strlen(message),
               0,
               (struct sockaddr *)&ctx->destination,
               sizeof(ctx->destination));

        sleep(2);
    }

    return NULL;
}

void *handle_client(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    int authenticated = 0;

    int monitor_active = 0;
    int monitor_udp_fd = -1;
    pthread_t monitor_thread;
    struct monitor_context *monitor_ctx = NULL;

    while (1)
    {
        char buffer[BUFFER_SIZE];

        if (recv_line(client_fd,
              buffer,
              sizeof(buffer)) < 0)
        {
        printf("[Agent] Controller disconnected.\n");
        write_log("Controller connection closed");
        break;
        }

        printf("[Agent] Received: %s", buffer);
        write_log(buffer);

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
 * PUT
 */
if (strncmp(buffer, "PUT ", 4) == 0)
{
    char filename[256];
    long file_size;

    if (sscanf(buffer + 4,
           "%255s %ld",
           filename,
           &file_size) != 2)
    {
    char response[128];

    snprintf(response,
         sizeof(response),
         "ERR 400 UNKNOWN_COMMAND SID:%s\n",
         SID);

    send_all(client_fd,
         response,
         strlen(response));

    continue;
    }

    if (file_size < 0 || file_size > MAX_FILE_SIZE)
    {
    char response[128];

    snprintf(response,
         sizeof(response),
         "ERR 004 FILE_TOO_LARGE SID:%s\n",
         SID);

    send_all(client_fd,
         response,
         strlen(response));

    continue;
    }

    if (strstr(filename, "..") != NULL ||
    strchr(filename, '/') != NULL ||
    strchr(filename, '\\') != NULL)
    {
    char response[128];

    snprintf(response,
         sizeof(response),
         "ERR 400 UNKNOWN_COMMAND SID:%s\n",
         SID);

    send_all(client_fd,
         response,
         strlen(response));

    continue;
    }

    char filepath[512];

    snprintf(filepath,
         sizeof(filepath),
         "./agentfiles/IT24101220/%s",
         filename);

    FILE *file = fopen(filepath, "wb");

    if (file == NULL)
    {
    perror("[Agent] fopen");

    continue;
    }

    char file_buffer[4096];
    long remaining = file_size;
    int file_error = 0;

    while (remaining > 0)
    {
    size_t chunk_size;

    if (remaining > (long)sizeof(file_buffer))
    {
        chunk_size = sizeof(file_buffer);
    }
    else
    {
        chunk_size = (size_t)remaining;
    }

    if (recv_exact(client_fd,
               file_buffer,
               chunk_size) < 0)
    {
        file_error = 1;
        break;
    }

    if (fwrite(file_buffer,
           1,
           chunk_size,
           file) != chunk_size)
    {
        file_error = 1;
        break;
    }

    remaining -= (long)chunk_size;
    }

    fclose(file);

    if (file_error)
    {
    remove(filepath);

    printf("[Agent] File transfer failed: %s\n",
           filename);

    continue;
    }

    printf("[Agent] File received: %s (%ld bytes)\n",
       filename,
       file_size);

    char response[512];

    snprintf(response,
         sizeof(response),
         "OK FILE_RECEIVED %s SID:%s\n",
         filename,
         SID);

    send_all(client_fd,
         response,
         strlen(response));

    continue;
}
        /*
         * GET
         */
        if (strncmp(buffer, "GET ", 4) == 0)
        {
        char filename[256];

        if (sscanf(buffer + 4, "%255s", filename) != 1)
        {
            char response[128];
            snprintf(response, sizeof(response),
                 "ERR 005 FILE_NOT_FOUND SID:%s\n", SID);
            send_all(client_fd, response, strlen(response));
            continue;
        }

        if (strstr(filename, "..") != NULL ||
            strchr(filename, '/') != NULL ||
            strchr(filename, '\\') != NULL)
        {
            char response[128];
            snprintf(response, sizeof(response),
                 "ERR 005 FILE_NOT_FOUND SID:%s\n", SID);
            send_all(client_fd, response, strlen(response));
            continue;
        }

        char filepath[512];
        snprintf(filepath, sizeof(filepath),
             "./agentfiles/IT24101220/%s", filename);

        FILE *file = fopen(filepath, "rb");
        if (file == NULL)
        {
            char response[128];
            snprintf(response, sizeof(response),
                 "ERR 005 FILE_NOT_FOUND SID:%s\n", SID);
            send_all(client_fd, response, strlen(response));
            continue;
        }

        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (file_size < 0)
        {
            fclose(file);
            continue;
        }

        char response[512];

        snprintf(response, sizeof(response),
             "OK FILE_SEND %s %ld SID:%s\n",
             filename, file_size, SID);

        if (send_all(client_fd,
                 response,
                 strlen(response)) < 0)
        {
            fclose(file);
            break;
        }

        char file_buffer[4096];
        size_t bytes_read;

        while ((bytes_read = fread(file_buffer,
                       1,
                       sizeof(file_buffer),
                       file)) > 0)
        {
            if (send_all(client_fd,
                 file_buffer,
                 bytes_read) < 0)
            {
            fclose(file);
            break;
            }
        }

        fclose(file);

        printf("[Agent] File sent: %s (%ld bytes)\n",
               filename,
               file_size);

        continue;
        }

        /*
         * MONITOR START <udp_port>
         */
        if (strncmp(buffer, "MONITOR START ", 14) == 0)
        {
            int udp_port;

            if (!authenticated)
            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "ERR 003 AUTH_REQUIRED SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }

            if (monitor_active)
            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "ERR 006 MONITOR_ALREADY_RUNNING SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }

            if (sscanf(buffer + 14,
                       "%d",
                       &udp_port) != 1 ||
                udp_port < 1 ||
                udp_port > 65535)
            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "ERR 007 INVALID_UDP_PORT SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }

            monitor_udp_fd =
                socket(AF_INET, SOCK_DGRAM, 0);

            if (monitor_udp_fd < 0)
            {
                perror("[Agent] UDP socket");
                continue;
            }

            monitor_ctx =
                malloc(sizeof(struct monitor_context));

            if (monitor_ctx == NULL)
            {
                close(monitor_udp_fd);
                monitor_udp_fd = -1;
                continue;
            }

            memset(monitor_ctx,
                   0,
                   sizeof(struct monitor_context));

            monitor_ctx->udp_fd = monitor_udp_fd;

            monitor_ctx->destination.sin_family =
                AF_INET;

            monitor_ctx->destination.sin_port =
                htons((uint16_t)udp_port);

            {
                struct sockaddr_in peer;
                socklen_t peer_len = sizeof(peer);

                memset(&peer, 0, sizeof(peer));

                if (getpeername(
                        client_fd,
                        (struct sockaddr *)&peer,
                        &peer_len) == 0)
                {
                    monitor_ctx->destination.sin_addr =
                        peer.sin_addr;
                }
                else
                {
                    inet_pton(
                        AF_INET,
                        "127.0.0.1",
                        &monitor_ctx->destination.sin_addr);
                }
            }

            if (pthread_create(
                    &monitor_thread,
                    NULL,
                    monitor_thread_function,
                    monitor_ctx) != 0)
            {
                free(monitor_ctx);
                monitor_ctx = NULL;

                close(monitor_udp_fd);
                monitor_udp_fd = -1;

                continue;
            }

            monitor_active = 1;

            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "OK MONITOR_STARTED SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));
            }

            printf("[Agent] UDP monitoring started on port %d.\n",
                   udp_port);

            continue;
        }

        /*
         * MONITOR STOP
         */
        if (strcmp(buffer, "MONITOR STOP\n") == 0)
        {
            if (!authenticated)
            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "ERR 003 AUTH_REQUIRED SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }

            if (!monitor_active)
            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "ERR 009 MONITOR_NOT_RUNNING SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));

                continue;
            }

            pthread_cancel(monitor_thread);
            pthread_join(monitor_thread, NULL);

            close(monitor_udp_fd);

            monitor_udp_fd = -1;

            free(monitor_ctx);

            monitor_ctx = NULL;

            monitor_active = 0;

            {
                char response[128];

                snprintf(response,
                         sizeof(response),
                         "OK MONITOR_STOPPED SID:%s\n",
                         SID);

                send_all(client_fd,
                         response,
                         strlen(response));
            }

            printf("[Agent] UDP monitoring stopped.\n");

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

    return NULL;
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
        write_log("Controller connected");

        int *client_socket = malloc(sizeof(int));

        if (client_socket == NULL)
        {
            perror("malloc");
            close(client_fd);
            continue;
        }

        *client_socket = client_fd;

        pthread_t thread_id;

        if (pthread_create(&thread_id,
                           NULL,
                           handle_client,
                           client_socket) != 0)
        {
            perror("pthread_create");
            free(client_socket);
            close(client_fd);
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_fd);

    return 0;
}
