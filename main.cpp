#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <zconf.h>

pthread_t accept_thread;
pthread_t threads[10];

pthread_mutex_t mutex;

int server_socket;
int sockets[10];

int connections = 0;

struct _socket_key_map {
    int socket;
    int index;
};

typedef struct _socket_key_map socket_key_map;


int readn(int s, char *buf, int n)
{

    int rc;
    char tmp_buf[1];

    for (int i = 0; i < n; ++i) {

        rc = recv(s, tmp_buf, 1, 0);

        if (rc <= 0)
            return -1;

        buf[i] = tmp_buf[0];
    }

    return 1;
}

//void *readn_routine(void *socket_ptr)
void *readn_routine(void *skp_void_ptr)
{
    int rc;
    //int socket = *((int *)socket_ptr);
    auto *skp = (socket_key_map *)skp_void_ptr;
    int socket = skp->socket;
    int index = skp->index;
    char buff[10];

    while (1) {
        rc = readn(socket, buff, 10);

        if (rc < 0)
        {
            perror("recv call failed");
            break;
        }

        printf("%d\t%d: \'%s\'\n", index, rc, buff);

        rc = send(socket, "OK\n", 3, 0);
        if (rc <= 0)

        {
            perror("send call failed");
            break;
        }
    }

    for (int i = index; i < connections - 1; ++i) {
        sockets[i] = sockets[i+1];
    }
    connections -= 1;

}

void *accept_routine(void *arg)
{
    socket_key_map skp;
    int socket;
    int result;
    //void *ptr;
    //ptr = malloc(sizeof(int));

    while (1)
    {
        if (connections < 10)
        {
            socket = accept(server_socket, NULL, NULL);
            if (socket < 0) {
                perror("accept call failed");
                break;
            }

            //*((int *)ptr) = socket;
            skp.index = connections;
            skp.socket = socket;


            pthread_mutex_lock(&mutex);
            sockets[connections] = socket;
            //pthread_create(&threads[connections], NULL, &readn_routine, ptr);
            pthread_create(&threads[connections], NULL, &readn_routine, &skp);
            connections = connections + 1;
            pthread_mutex_unlock(&mutex);


        }
    }
    //free(ptr);

    for(int i = 0; i < connections; ++i)
    {
        result = shutdown(sockets[i], 2);
        if (result < 0)
            perror("shutdown call failed");

        result = close(sockets[i]);
        if (result < 0)
            perror("close call failed");

        pthread_join(threads[i], NULL);
    }

}


int main(void)
{
    struct sockaddr_in local;
    int rc;
    bool quit = 0;
    char in;
    int socket_index;

    pthread_mutex_init(&mutex, NULL);
    pthread_create(&accept_thread, NULL, &accept_routine, NULL);

    local.sin_family = AF_INET;
    local.sin_port = htons(7500);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server_socket < 0) {
        perror("socket call failed");
        exit(1);
    }

    rc = bind(server_socket, (struct sockaddr *) &local, sizeof(local));

    if (rc < 0) {
        perror("bind call failure");
        exit(1);
    }

    rc = listen(server_socket, 10);

    if (rc) {
        perror("listen call failed");
        exit(1);
    }

    while (1) {
        in = (char) getc(stdin);

        switch (in) {

            case 'c': {
                socket_index = (char) getc(stdin) - '0';

                if (socket_index > connections) {
                    perror("Index out of range");
                    break;
                }

                rc = shutdown(sockets[socket_index], 2);
                if (rc < 0)
                    perror("Failed to shutdown socket");

                rc = close(sockets[socket_index]);
                if (rc < 0)
                    perror("Failed to close socket");

                break;
            }

            case 'l': {
                printf("Connections = %d \n", connections);
                for (int i = 0; i < connections; ++i) {
                    printf("%d\t%d\n", i, sockets[i]);
                }
                break;
            }

            case 'q': {

                quit = 1;
                break;
            }
        }

        if (quit)
            break;
    }



    rc = shutdown(server_socket, 2);
    if (rc < 0)
        perror("shutdown call failed");

    rc = close(server_socket);
    if (rc < 0)
        perror("close call failed");

    pthread_join(accept_thread, NULL);

    pthread_mutex_destroy(&mutex);

    return 0;
}
