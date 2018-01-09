#include <stdio.h>
#include <string.h>                     // strlen
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>                  // inet_addr
#include <unistd.h>                     // write
#include <signal.h>                     // signal

#include <pthread.h>                    // Threading

#include <assert.h>                     // assert


int socket_fd;                          // Socket file descriptor
pthread_mutex_t counters_lock;          // Mutex for the counters


/* The following counter variables may only be accessed when the
 * counters_lock lock is held!
 */
long m_counter = 0;     // Counter variable for male users
long f_counter = 0;     // Counter variable for female users

#define PORT                        8666  // Port to listen on
#define CONCURRENT_BATHROOM_USERS   3     // Number of people of the same
                                          // gender that can be in the bathroom at the same time
#define BUFFER_SIZE                 2048  // String buffer for receiving

#define BACKLOG         10

/* SIGINT Handler
 */
void handle_int(int dummy)
{
    puts("\rReceived SIGINT, exiting now...");
    close(socket_fd);
    pthread_mutex_destroy(&counters_lock);
    exit(0);
}

/* This function is used to communicate with a specific client
 * that was accepted in our main loop, in a separate thread for
 * each client.
 */
void *handle_socket(void *l_socket_fd)
{
    int fd = *(int *)l_socket_fd, read_size, msg_id;
    char msg[BUFFER_SIZE], gender[64], *tok;

    // Read data from the client
    while ((read_size = recv(fd, msg, sizeof(msg), 0)) > 0)
    {
        // Look for the valid start sequence of the clients message
        tok = strchr(msg, '#');
        if (tok == NULL ||
            msg[0] != '$' ||
            msg[5] != ':' ||
            strncmp(tok, "#\r\n", 3) != 0)
        {
            // If there is no valid start sequence, print an error
            // and disconnect the client.
            puts("Wrong message format, disconnecting client!");
            close(fd);
            break;
        }

        // The first 4 bytes after the start sequence ($) hold the id of
        // the message that was sent.
        char id[5] = {msg[1], msg[2], msg[3], msg[4], 0};
        msg_id = atoi(id);

        // Next check the type of the message and the argument:
        // HELO
        if (strncmp(msg+6, "HELO;gender=", 12) == 0)
        {
            tok = strchr(msg, '#');
            *tok = '\0';
            tok = strchr(msg, '=');
            strncpy(gender, tok+1, sizeof(gender));

            // Lock the mutex for the counters
            pthread_mutex_lock(&counters_lock);
            switch (gender[0]) {
                case 'M':
                    if (m_counter < CONCURRENT_BATHROOM_USERS && !f_counter) {
                        m_counter++;
                        printf("%c User entered, now %ld users\n", 'M', m_counter);
                        sprintf(msg, "$%04d:ENTR;#\r\n", msg_id);
                        write(fd, msg, strlen(msg));
                    } else {
                        printf("%c User can't enter now. (%ld M, %ld F)\n", 'M', m_counter, f_counter);
                        sprintf(msg, "$%04d:BLOK;#\r\n", msg_id);
                        write(fd, msg, strlen(msg));
                    }
                break;
                case 'F':
                    if (f_counter < CONCURRENT_BATHROOM_USERS && !m_counter) {
                        f_counter++;
                        printf("%c User entered, now %ld users\n", 'F', f_counter);
                        sprintf(msg, "$%04d:ENTR;#\r\n", msg_id);
                        write(fd, msg, strlen(msg));
                    } else {
                        printf("%c User can't enter now. (%ld M, %ld F)\n", 'F', m_counter, f_counter);
                        sprintf(msg, "$%04d:BLOK;#\r\n", msg_id);
                        write(fd, msg, strlen(msg));
                    }
                break;
                default:
                    sprintf(msg, "$%04d:ERR ;msg=%s#\r\n", msg_id, "Unhandled gender");
                    write(fd, msg, strlen(msg));
                break;
            }

            pthread_mutex_unlock(&counters_lock);
        }

        // EXIT
        else if (strncmp(msg+6, "EXIT;gender=", 12) == 0)
        {
            tok = strchr(msg, '#');
            *tok = '\0';
            tok = strchr(msg, '=');
            strncpy(gender, tok+1, sizeof(gender));

            // Lock the mutex for the counters
            pthread_mutex_lock(&counters_lock);
            switch (gender[0]) {
                case 'M':
                    if (m_counter) {
                        m_counter--;
                        printf("%c User left, now %ld users\n", 'M', m_counter);
                        sprintf(msg, "$%04d:LEFT;#\r\n", msg_id);
                        write(fd, msg, strlen(msg));
                    } else {
                        printf("%c User can't leave, not entered. (%ld M, %ld F)\n", 'M', m_counter, f_counter);
                        sprintf(msg, "$%04d:ERR ;msg=%s#\r\n", msg_id, "Can't leave, not entered");
                        write(fd, msg, strlen(msg));
                    }
                break;
                case 'F':
                    if (f_counter) {
                        f_counter--;
                        printf("%c User left, now %ld users\n", 'F', f_counter);
                        sprintf(msg, "$%04d:LEFT;#\r\n", msg_id);
                        write(fd, msg, strlen(msg));
                    } else {
                        printf("%c User can't leave, not entered. (%ld M, %ld F)\n", 'F', m_counter, f_counter);
                        sprintf(msg, "$%04d:ERR ;msg=%s#\r\n", msg_id, "Can't leave, not entered");
                        write(fd, msg, strlen(msg));
                    }
                break;
                default:
                    sprintf(msg, "$%04d:ERR ;msg=%s#\r\n", msg_id, "Unhandled gender");
                    write(fd, msg, strlen(msg));
                break;
            }

            pthread_mutex_unlock(&counters_lock);
        }
        memset(msg, sizeof(msg), 0);
    }
    if (read_size == 0)
    {
        puts("Client disconnected!");
    }
    if (read_size < 0)
    {
        perror("Receive failed");
        close(fd);
    }

    // Cleanup the socket
    free(l_socket_fd);
    return 0;
}

 /* Main */
int main(int argc, char const *argv[]) {
    int new_socket_fd, c, *new_sock;
    struct sockaddr_in server, client;
    char *msg;

    // SIGINT handler registration for graceful termination
    signal(SIGINT, handle_int);

    // Init the mutex for the apple_left counter
    if (pthread_mutex_init(&counters_lock, NULL))
    {
        perror("Could not create mutex");
        return 1;
    }

    // Create the server socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        perror("Could not create socket");
        return 1;
    }

    // Address and port
    server.sin_family       = AF_INET;
    server.sin_addr.s_addr  = INADDR_ANY;
    server.sin_port         = htons(PORT);

    // Bind the socket
    if (bind(socket_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("Could not bind to socket");
        return 1;
    }
    puts("Bound to socket!");

    // Start listening on the bound socket
    listen(socket_fd, BACKLOG);

    puts("Waiting for connections...");
    c = sizeof(struct sockaddr_in);
    // Wait for new connections and accept them
    while   ((new_socket_fd = accept(   socket_fd,
                                        (struct sockaddr *)&client,
                                        (socklen_t *)&c)
            ))
    {
        puts("Connection accepted!");

        pthread_t apple_thread;

        // Allocate memory for the thread argument
        new_sock = malloc(sizeof(int));

        // Set the argument for the thread to the socket file descriptor
        // of the newly accepted connection, so the thread can communicate
        // with the client that was just accepted
        *new_sock = new_socket_fd;

        // Create the thread for the accepted client. This thread will handle all
        // further communication with this client.
        if (pthread_create( &apple_thread,
                            NULL,
                            handle_socket,
                            (void *)new_sock)
            < 0)
        {
            perror("Could not create socket thread");
            return 1;
        }
        puts("Handler assigned");
    }

    if (new_socket_fd < 0)
    {
        perror("Could not accept connection");
        return 1;
    }

    // There is no valid condition that should bring us here, so
    // assert that we never reach this point of the program, so if we
    // ever do, we can easier debug it.
    assert(!"End of main should never happen");
}
