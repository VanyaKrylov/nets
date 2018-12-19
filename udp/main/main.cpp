#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <pthread.h>
#include <zconf.h>
#include <map>
#include <algorithm>
#include <vector>
#include <iostream>
#include <string>
#include <cstring>
#include <stddef.h>
#include <algorithm>

#define _POSIX_SOURCE
#include <unistd.h>
#include <arpa/inet.h>
#undef _POSIX_SOURCE
#define BUFLEN 512	//Max length of buffer

const int max_connections = 10;
const int buf_out_size = 8182;

pthread_t threads[10];
pthread_t accept_thread;
int connections = 0;
pthread_mutex_t mutex;

std::map <std::string, std::string> usr_map;
std::vector<std::string> root_users;
std::map <std::string, std::string> usr_session;
std::map <std::string, sockaddr_in> usr_addr_map;
std::vector<sockaddr_in> logged_in_addrs;


struct Socket_key_map {
    int socket;
    int index;
};


struct Routine_data {
    int socket;
    int index;
    sockaddr_in addr;
    std::string buf;
};

struct Addr_resp_pair {
    sockaddr_in addr;
    std::string buf;
};

struct Addr_index_pair {
    sockaddr_in addr;
    int index;
};

std::vector<Addr_resp_pair> response_buf;
std::vector<Addr_index_pair> indexes;


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
           (buf[2] - '0') * 10 + (buf[3] - '0');
    return size-1;
}


int get_index(char* buf) {
    int size;
    size = (buf[BUFLEN - 4] - '0') * 1000 + (buf[BUFLEN - 3] - '0') * 100 +
           (buf[BUFLEN - 2] - '0') * 10 + (buf[BUFLEN - 1] - '0');
    return size-1;
}


void *login_routine(void *data)
{
    auto *rd_ptr = (Routine_data *)data;
    int socket = rd_ptr->socket;
    sockaddr_in addr = rd_ptr->addr;
    unsigned int slen = sizeof(addr);
    char login_buf[BUFLEN];
    std::strcpy(login_buf, rd_ptr->buf.c_str());
    char cwd[256];
    std::string str_u_name;

    int rc;
    char login[1];
    char* running;
    char* u_name;
    char* password;

    std::map<std::string, std::string>::iterator it;
    std::pair<std::map<std::string, sockaddr_in>::iterator, bool> res;

    if (rd_ptr->buf.find(":") == std::string::npos)
    {
        login[0] = '*';
        sendto(socket, login, 1, 0, (struct sockaddr *) &addr, slen);
    } else {
        running = strdupa(login_buf);
        u_name = strsep(&running, ":");
        password = strsep(&running, ":");
        if (usr_session.find(u_name) != usr_session.end()) {
            login[0] = '2';
            rc = sendto(socket, login, 1, 0, (struct sockaddr *) &addr, slen);
            if (rc <= 0) {
                perror("send call failed");
            }
        } else {
            if (usr_map.find(u_name)->second == password) {
                login[0] = '1';
                rc = sendto(socket, login, 1, 0, (struct sockaddr *) &addr, slen);
                if (rc <= 0) {
                    perror("send call failed");
                }
            } else {
                login[0] = '0';
                rc = sendto(socket, login, 1, 0, (struct sockaddr *) &addr, slen);
                if (rc <= 0) {
                    perror("send call failed");
                }
            }
        }

        pthread_mutex_lock(&mutex);
        for (auto i = response_buf.begin(); i != response_buf.end();) {
            if ((inet_ntoa(i->addr.sin_addr) == inet_ntoa(addr.sin_addr)) &&
                (ntohs(i->addr.sin_port) == ntohs(addr.sin_port))) {
                response_buf.erase(i);
                //break;
            } else { ++i; }
        }
        if ( login[0] == '1' ) {
            res = usr_addr_map.insert(std::pair<std::string, sockaddr_in>(u_name, addr));
            if (!res.second) {
                usr_addr_map.erase(u_name);
                res = usr_addr_map.insert(std::pair<std::string, sockaddr_in>(u_name, addr));
            }

            response_buf.emplace_back(Addr_resp_pair{addr, std::string(1, login[0])});
            logged_in_addrs.push_back(addr);
            if (getcwd(cwd, sizeof(cwd)) == NULL)
                perror("getcwd() error");
            else {
                usr_session.insert(std::pair<std::string, std::string>(u_name, std::string(cwd)));
            }
        }
    }

    connections--;
    pthread_mutex_unlock(&mutex);
}


void *terminal_routine(void *data) {
    int rc;
    auto *rd_ptr = (Routine_data *) data;
    int socket = rd_ptr->socket;
    sockaddr_in addr = rd_ptr->addr;
    unsigned int slen = sizeof(addr);
    //struct sockaddr_in addr = rd_ptr->addr;
    char buf[BUFLEN];
    std::strcpy(buf, rd_ptr->buf.c_str());
    int socket_index;
    int chdir_ret;
    char buf_out[buf_out_size];
    char sub_buf[BUFLEN];
    bool logout;
    int input_size;
    sockaddr_in victim_addr;
    std::string output;
    std::string u_name;
    std::map<std::string, std::string>::iterator it;

    input_size = get_size(buf);
    for (auto &i : usr_addr_map) {
        if (inet_ntoa(i.second.sin_addr) == inet_ntoa(addr.sin_addr) &&
            (ntohs(i.second.sin_port) == ntohs(addr.sin_port))) {
            u_name = i.first;
            break;
        }
    }


    switch (buf[4]) {
        case '1': {
            output = exec("ls");
            break;
        }
        case '2': {
            strncpy(sub_buf, buf + 5, input_size);
            chdir_ret = chdir(sub_buf);
            if (chdir_ret < 0) {
                perror("Chdir error");
                output = "Unsuccesfull";
            } else {
                usr_session.find(u_name)->second = std::string(sub_buf);
                output = std::string(sub_buf);
            }
            break;
        }
        case '3': {
            for (it = usr_session.begin(); it != usr_session.end(); ++it)
                output = output + it->first + ' ' + it->second + '\n';
            break;
        }
        case '4': {
            if (std::find(root_users.begin(), root_users.end(), u_name) != root_users.end()) {
                strncpy(sub_buf, buf + 5, input_size);
                if (std::string(sub_buf) == u_name) {
                    output = "You can not kill your own session. Use 'logout' command instead";
                } else {
                    pthread_mutex_lock(&mutex);
                    for (auto i = logged_in_addrs.begin(); i != logged_in_addrs.end();) {
                        if (inet_ntoa(i->sin_addr) == inet_ntoa(usr_addr_map.find(std::string(sub_buf))->second.sin_addr) &&
                            (ntohs(i->sin_port) == ntohs(usr_addr_map.find(std::string(sub_buf))->second.sin_port))) {
                            logged_in_addrs.erase(i);
                        } else {++i;}
                    }
                    victim_addr = usr_addr_map.find(std::string(sub_buf))->second;
                    usr_session.erase(std::string(sub_buf));
                    usr_addr_map.erase(std::string(sub_buf));
                    for (auto i = response_buf.begin(); i != response_buf.end();) {
                        if ((inet_ntoa(i->addr.sin_addr) == inet_ntoa(victim_addr.sin_addr)) &&
                            (ntohs(i->addr.sin_port) == ntohs(victim_addr.sin_port))) {
                            response_buf.erase(i);
                        } else {++i;}
                    }
                    for (auto i = indexes.begin(); i != indexes.end();) {
                        if ((inet_ntoa(i->addr.sin_addr) == inet_ntoa(victim_addr.sin_addr)) &&
                            (ntohs(i->addr.sin_port) == ntohs(victim_addr.sin_port))) {
                            indexes.erase(i);
                            //break;
                        } else {++i;}
                    }
                    pthread_mutex_unlock(&mutex);
                    output = "Succesful murder";
                }
            } else {
                output = "Permission denied";
            }
            break;
        }
        case '5': {
            logout = 1;
            break;
        }
    }

    if (logout) {
        pthread_mutex_lock(&mutex);
        for (auto i = logged_in_addrs.begin(); i != logged_in_addrs.end();) {
            if ((inet_ntoa(i->sin_addr) == inet_ntoa(addr.sin_addr)) &&
                (ntohs(i->sin_port) == ntohs(addr.sin_port))) {
                logged_in_addrs.erase(i);
                //break;
            } else {++i;}
        }
        usr_session.erase(u_name);
        pthread_mutex_unlock(&mutex);
    } else {
        std::strcpy(buf_out, output.c_str());
        rc = sendto(socket, buf_out, buf_out_size, 0, (struct sockaddr *) &addr, slen);
        if (rc <= 0) {
            perror("send call failed");
        }
    }
    memset(&buf_out, 0, sizeof(buf_out));
    output.clear();

    pthread_mutex_lock(&mutex);
    connections--;
    pthread_mutex_unlock(&mutex);
}


void *repeat_response_routine(void *data)
{
    auto *rd_ptr = (Routine_data *)data;
    int socket = rd_ptr->socket;
    int index = rd_ptr->index;
    sockaddr_in addr = rd_ptr->addr;
    unsigned int slen = sizeof(addr);
    char buf[BUFLEN];

    for ( auto &response : response_buf ) {
        if ( ( inet_ntoa(response.addr.sin_addr) == inet_ntoa(addr.sin_addr) ) &&
             ( ntohs(response.addr.sin_port) == ntohs(addr.sin_port) ))
        {
            sendto(socket, response.buf.c_str(), buf_out_size, 0, (sockaddr *) &addr, slen);
            std::cout << "Again" << std::endl;
        }
    }

    pthread_mutex_lock(&mutex);
    connections--;
    pthread_mutex_unlock(&mutex);
}


void *recvfrom_routine(void *server_socket_ptr)
{
    sockaddr_in si_other;
    char buf[BUFLEN];
    auto *skp_ptr = (Socket_key_map *)server_socket_ptr;
    int s = skp_ptr->socket, recv_len;
    unsigned int slen = sizeof(si_other);
    bool is_new = true;
    bool already_requested = false;
    int local_connections;
    int index;
    bool index_error = 0;
    Routine_data rd;
    Addr_index_pair addr_index_pair;
    std::string temp = "Session closed by administrator";

    while(1) {
        if (connections < max_connections)
        {
            recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen);
            if ( recv_len < 0 )
            {
                perror("recv_from error");
                break;
            }
            index = get_index(buf);
            rd = {s, index, si_other, buf};

            if ( index < 0)
            {
                std::cout << "Incorrect package index, droping the package" << index <<std::endl;
                continue;
            }

            for ( sockaddr_in &e : logged_in_addrs ) {
                if (inet_ntoa(e.sin_addr) == inet_ntoa(si_other.sin_addr) &&
                    (ntohs(e.sin_port) == ntohs(si_other.sin_port)))
                {
                    is_new = 0;
                    break;
                }
            }

            for ( Addr_index_pair &i : indexes ) {
                if ( ( inet_ntoa(i.addr.sin_addr) == inet_ntoa(si_other.sin_addr) ) &&
                     ( ntohs(i.addr.sin_port) == ntohs(si_other.sin_port) ))
                {
                    if ( i.index == index )
                    {
                        already_requested = 1;
                        break;
                    } else if ( index - i.index == 1 )
                    {
                        i.index = index;
                    } else if ( index - i.index > 1 )
                    {
                        index_error = 1;
                    }
                }
            }
            if ( index_error )
            {
                std::cout << "Index error, package dropped with no response" << std::endl;
                index_error = 0;
                continue;
            }

            if ( is_new )
            {
                addr_index_pair = {si_other, index};
                pthread_mutex_lock(&mutex);
                indexes.push_back(addr_index_pair);
                pthread_create(&threads[connections], NULL, &login_routine, &rd);
                connections++;
                pthread_mutex_unlock(&mutex);
            } else {

                if ( already_requested )
                {
                    pthread_mutex_lock(&mutex);
                    pthread_create(&threads[connections], NULL, &repeat_response_routine, &rd);
                    pthread_mutex_unlock(&mutex);
                } else {
                    pthread_mutex_lock(&mutex);
                    pthread_create(&threads[connections], NULL, &terminal_routine, &rd);
                    connections++;
                    pthread_mutex_unlock(&mutex);
                }
            }
            already_requested = 0;
            is_new = 1;
        } else
            sleep(1);
    }

    pthread_mutex_lock(&mutex);
    local_connections = connections;
    pthread_mutex_unlock(&mutex);

    for (int i = 0; i < local_connections; ++i) {
        pthread_join(threads[i], NULL);
    }
}


int main(void)
{
    struct sockaddr_in si_me;
    int rc;
    bool quit = 0;
    char in;
    int socket_index;
    Socket_key_map skp;
    int s;

    usr_map.insert(std::pair <std::string, std::string> ("vaddya", "32283228"));
    usr_map.insert(std::pair <std::string, std::string> ("lamtev2000", "iluvclassmates"));
    usr_map.insert(std::pair <std::string, std::string> ("mikle_undef", "arguetill_theend"));
    usr_map.insert(std::pair <std::string, std::string> ("valik", "1228"));
    usr_map.insert(std::pair <std::string, std::string> ("ivan", "0000"));
    root_users.emplace_back("ivan");

    //create a UDP socket
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("socket call failed");
        exit(1);
    }

    skp = {s, 0};

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(7500);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind socket to port
    rc = bind(s , (struct sockaddr*)&si_me, sizeof(si_me) );
    if( rc < 0 )
    {
        perror("bind");
        exit(1);
    }
    pthread_mutex_init(&mutex, NULL);
    pthread_create(&accept_thread, NULL, &recvfrom_routine, &skp);

    //keep listening for data
    while (1) {
        in = (char) getc(stdin);
        switch (in) {
            case 'c': {
                socket_index = (char) getc(stdin) - '0';
                if ( socket_index > logged_in_addrs.size() )
                {
                    perror("Index out of range");
                    break;
                }
                pthread_mutex_lock(&mutex);
                logged_in_addrs.erase(logged_in_addrs.begin() + socket_index);
                pthread_mutex_unlock(&mutex);
                break;
            }
            case 'l': {
                pthread_mutex_lock(&mutex);
                std::cout << "Connections: " << logged_in_addrs.size() << std::endl;
                int i = 0;
                for ( auto &c : logged_in_addrs ) {
                    std::cout << i << "\t" << inet_ntoa(c.sin_addr) << ":" << htons(c.sin_port) << std::endl;
                    i++;
                }
                pthread_mutex_unlock(&mutex);
                break;
            }
            case 'q': {

                quit = 1;
                break;
            }
        }
        if ( quit )
            break;
    }

    rc = shutdown(s, 2);
    if (rc < 0)
        perror("shutdown call failed");
    rc = close(s);
    if (rc < 0)
        perror("close call failed");

    pthread_join(accept_thread, NULL);
    pthread_mutex_destroy(&mutex);
    return 0;
}
