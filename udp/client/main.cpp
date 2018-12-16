#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>
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

#define LOGIN_L 1

const int login_pair_size = 64;
const int buf_size = 512;
const int buf_out_size = 8192;
//const int buf_out_size = 128;
const int message_length_buffer_field = 4;




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




void pack_size(char* buf, ulong size)
{
    char c;
    for ( int i = 3; i >= 0; --i ) {
        if ( size > 0 ) {
            c = char(size % 10) + '0';
            buf[i] = c;
        } else {
            buf[i] = '0';
        }
        size /= 10;
    }
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
        if (rc <= n) {
            for (int j = 0; j < rc; ++j) {
                buf[j + read] = tmp_buf[j];
            }
            read += rc;
        }
    }
    return 1;
}



int main( void )
{
    int s;
    int rc;
    int command_descriptor_index = message_length_buffer_field;
    //int command;
    int msg_size = 32;
    bool noInput = 1;
    char logInRes[1];
    std::string str;
    std::string substring;
    std::string login_str;
    login_str.resize(64);

    int n;
    struct sockaddr_in server, from;
    unsigned int slen = sizeof(from);
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        perror("socket");
        exit(1);
    }

    from.sin_family = AF_INET;
    from.sin_port = htons( 7500 );
    from.sin_addr.s_addr = inet_addr( "127.0.0.1" );

    char login_buf[login_pair_size];
    char buf[ buf_size ];
    char buf_out[buf_out_size];


    //Log in loop


    while (1) {
        printf("Log in: {user_name}:{password}: \n");
        std::getline(std::cin, login_str);
        if (login_str.size() > login_pair_size - message_length_buffer_field) {
            perror("Incorrect authentication length, should be less than 64");
        } else {
            //pack_size(login_buf, str.size());
            //for (int i = 0; i < str.size(); ++i) {
            //    login_buf[i+message_length_buffer_field] = str[i];
            //}
            rc = sendto(s, login_str.c_str(), login_pair_size, 0, (struct sockaddr *) &from, slen);
            if (rc <= 0) {
                perror("send call failed");
                exit(1);
            }
            //rc = readn(s, logInRes, 1);
            rc = recvfrom(s, logInRes, LOGIN_L, 0, (struct sockaddr *) &from, &slen);
            if (rc <= 0) {
                perror("recv call failed");
                exit(1);
            }
            rc = sendto(s, "1", 1, 0, (struct sockaddr *) &from, slen);
            if (rc <= 0) {
                perror("ack call failed");
                exit(1);
            }
            if (logInRes[0] == '1') {
                printf("Succesfully logged in \n");
                break;
            } else if (logInRes[0] == '2')
                printf("Already logged in in another session \n");
            else
                printf("Wrong login:password pair \n");
        }
    }


    //main workflow loop
    while(1) {

        std::getline (std::cin, str);
        //std::cin >> str;

        if (str == "ls") {
            //packing message size 0001 to buffer
            for (int i = 0; i < message_length_buffer_field - 1; ++i) {
                buf[i] = '0';
            }
            buf[command_descriptor_index - 1] = '1';
            buf[command_descriptor_index] = '1';
            noInput = 0;
        }

        else if (str.compare(0,2,"cd") == 0) {
            for (int i = 0; i < message_length_buffer_field - 1; ++i) {
                buf[i] = '0';
            }
            buf[command_descriptor_index - 1] = '1';
            buf[command_descriptor_index] = '2';
            if (str[2] != ' ') {
                //perror("Incorrect input format");
                //break;
                std::cout << "Incorrect input format" << std::endl;
                continue;
            } else {
                substring = str.substr(3);
                if (substring.size() > buf_size - message_length_buffer_field) {
                    perror("Out of input size, max length is 30 symbols");
                    break;
                } else {
                    for (int i = 0; i < substring.size(); ++i) {
                        buf[i + message_length_buffer_field + 1] = substring[i];
                    }
                    pack_size(buf, substring.size() + 1);
                }
                substring.clear();
            }
            noInput = 0;
        }

        else if (str=="who") {
            for (int i = 0; i < message_length_buffer_field - 1; ++i) {
                buf[i] = '0';
            }
            buf[command_descriptor_index - 1] = '1';
            buf[command_descriptor_index] = '3';
            noInput = 0;
        }

        else if (str.compare(0,4,"kill") == 0) {
            buf[command_descriptor_index] = '4';
            if (str[4] != ' ') {
                perror("Incorrect input format");
                break;
            } else {
                substring = str.substr(5);
                if (substring.size() > buf_size - message_length_buffer_field) {
                    perror("Out of input size, max length is 30 symbols");
                    //break;
                } else {
                    for (int i = 0; i < substring.size(); ++i) {
                        buf[i + message_length_buffer_field + 1] = substring[i];
                    }
                    pack_size(buf, substring.size() + 1);
                }
                substring.clear();
            }
            noInput = 0;
        }

        else if (str == "logout") {
            for (int i = 0; i < message_length_buffer_field - 1; ++i) {
                buf[i] = '0';
            }
            buf[command_descriptor_index - 1] = '1';
            buf[command_descriptor_index] = '5';
            //rc = send(s, buf, buf_size, 0);
            rc = sendto(s, buf, buf_size, 0, (struct sockaddr *) &from, slen);
            if (rc <= 0)
            {
                perror("send call failed");
                break;
            }
            break;
        }

        else
            noInput = 1;

        if (!noInput) {
            //rc = send(s, buf, buf_size, 0);
            rc = sendto(s, buf, buf_size, 0, (struct sockaddr *) &from, slen);
            if (rc <= 0) {
                perror("send call failed");
                break;
            }


            //rc = readn(s, buf_out, buf_out_size);
            rc = recvfrom(s, buf_out, buf_out_size, 0, (struct sockaddr *) &from, &slen);
            if (rc <= 0) {
                perror("recv call failed");
                break;
            }

            std::cout << "Received: \n" << std::string(buf_out) << std::endl; //may not work */
            memset(&buf_out, 0, sizeof(buf_out));


            //std::cout << str.substr(1, str[0]);
            //str.clear();
        }

    }

    printf("Session closed, logged out");
    exit( 0 );
}
