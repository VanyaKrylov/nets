#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <zconf.h>
#include <map>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>



const int max_connections = 10;
const int message_length = 10;

const int login_pair_size = 64;
const int buf_size = 32;
const int buf_out_size = 8192;

std::map <std::string, std::string> usr_map;
std::vector<std::string> root_users;

pthread_t accept_thread;
pthread_t threads[10];

pthread_mutex_t mutex;

//int server_socket;
int sockets[10];


int connections = 0;

struct _socket_key_map {
    int socket;
    int index;
};


typedef struct _socket_key_map socket_key_map;




std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != NULL)
                result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}




int get_size(char* buf) {
    int size;
    size = (buf[0] - '0') * 1000 + (buf[1] - '0') * 100 +
            (buf[2] - '0') * 10 + (buf[4] - '0');
    return size;
}



int readn(int s, char *buf, int n)
{

    int rc;
    char tmp_buf[n];
    int read = 0;

    while (read < n) {

        rc = recv(s, tmp_buf, n - read, 0);

        if (rc <= 0)
            return -1;

        if (rc <= 10) {
            for (int j = 0; j < rc; ++j) {
                buf[j + read] = tmp_buf[j];
            }
            read += rc;
        }

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
    int index;
    int input_size;
    //int index = skp->index;
    char buff[message_length];
    std::string output;
    bool logout;

    char login_buf[login_pair_size];
    char buf[ buf_size ];
    char buf_out[buf_out_size];
    char sub_buf[ buf_size ];

    while (1) {
        //login
        rc = readn(socket, buf, buf_size);
        if (rc < 0)
        {
            perror("recv call failed");
            break;
        }
        input_size = get_size(buf);
        switch (buf[4]) {
            case '1': {
                output = exec("ls");
                std::cout << output << std::endl;
                break;
            }
            case '2': {
                strncpy(sub_buf, buf+5, input_size);
                output = exec(sub_buf);
                break;
            }
            case '3': {
                break;
            }
            case '4': {
                break;
            }
            case '5': {
                logout = 1;
                break;
            }
        }

        if (logout)
            break;
        //printf("%d\t%d: \'%s\'\n", index, rc, buff);
        rc = send(socket, output.c_str(), output.size(), 0);
        if (rc <= 0)
        {
            perror("send call failed");
            break;
        }
    }
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < connections; ++i) {
        if (sockets[i] == socket) {
            index = i;
            for (int j = index; j < connections - 1; ++j) {
                sockets[j] = sockets[j + 1];
            }
            break;
        }
    }
    connections -= 1;
    pthread_mutex_unlock(&mutex);

}

void *accept_routine(void *server_socket_ptr)
{
    socket_key_map skp;
    int socket;
    int result;
    int local_connections;
    auto *skp_ptr = (socket_key_map *)server_socket_ptr;
    int server_socket = skp_ptr->socket;
    //void *ptr;
    //ptr = malloc(sizeof(int));

    while (1)
    {
        if (connections < max_connections)
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
        else
            sleep(1);
    }
    //free(ptr);

    pthread_mutex_lock(&mutex);
    local_connections = connections;
    pthread_mutex_unlock(&mutex);

    for(int i = 0; i < local_connections; ++i)
    {
        //printf("%d", local_connections);
        //result = shutdown(sockets[i], 2);
        result = shutdown(sockets[0], 2);
        if (result < 0)
            perror("shutdown call failed");

        //result = close(sockets[i]);
        result = close(sockets[0]);
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
    socket_key_map skp;
    int server_socket;

    usr_map.insert(std::pair <std::string, std::string> ("vaddya", "32283228"));
    usr_map.insert(std::pair <std::string, std::string> ("lamtev2000", "iluvclassmates"));
    usr_map.insert(std::pair <std::string, std::string> ("mikle_undef", "arguetill_theend"));
    usr_map.insert(std::pair <std::string, std::string> ("valik", "1228"));
    usr_map.insert(std::pair <std::string, std::string> ("ivan", "0000"));
    root_users.push_back("ivan");

    local.sin_family = AF_INET;
    local.sin_port = htons(7500);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    skp.socket = socket(AF_INET, SOCK_STREAM, 0);
    server_socket = skp.socket;

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

    pthread_mutex_init(&mutex, NULL);
    pthread_create(&accept_thread, NULL, &accept_routine, &skp);

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
