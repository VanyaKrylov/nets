#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <zconf.h>
#include <map>
#include <algorithm>
#include <vector>
#include <stddef.h>
#include <sys/time.h>

#define _POSIX_SOURCE
#include <unistd.h>
#include <arpa/inet.h>
#undef _POSIX_SOURCE

#define LOGIN_L 1

const int buf_size = 512;
const int buf_out_size = 8192;
const int size_gap = 4;
const int index_gap = 4;


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


void pack_index(char* buf, ulong index)
{
    char c;
    for ( int i = buf_size - 1; i >= buf_size - 4; --i ) {
        if ( index > 0 ) {
            c = char(index % 10) + '0';
            buf[i] = c;
        } else {
            buf[i] = '0';
        }
        index /= 10;
    }
}


int main( void )
{
    int s;
    int rc;
    int command_descriptor_index = size_gap;
    int msg_size = 32;
    bool noInput = 1;
    char logInRes[1];
    int index = 0;
    std::string str;
    std::string substring;
    std::string login_str;
    timeval timeval1;
    timeval1.tv_sec = 3;
    timeval1.tv_usec = 0;

    int n;
    struct sockaddr_in from;
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

    char login_buf[buf_size];
    char buf[ buf_size ];
    char buf_out[buf_out_size];

    if ( setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeval1, sizeof(timeval1)) < 0)
    {
        perror("Error setting timeout option \n");
    }

    //Log in loop

    while (1) {
        index = index + 1;
        printf("Log in: {user_name}:{password}: \n");
        std::getline(std::cin, login_str);
        if (login_str.size() > buf_size - size_gap - index_gap) {
            perror("Incorrect authentication length, should be less than 64");
        } else {
            login_str.resize(buf_size, 0);
            strcpy(login_buf, login_str.c_str());
            pack_index(login_buf, index);
            rc = sendto(s, login_buf, buf_size, 0, (struct sockaddr *) &from, slen);
            if (rc <= 0) {
                perror("send call failed");
                exit(1);
            }
            //ack loop
            if ( recvfrom(s, logInRes, LOGIN_L, 0, (struct sockaddr *) &from, &slen) < 0 ) {
                for (int i = 0; i <= 10; ++i) {
                    if (recvfrom(s, logInRes, LOGIN_L, 0, (struct sockaddr *) &from, &slen) < 0) {
                        rc = sendto(s, login_buf, buf_size, 0, (struct sockaddr *) &from, slen);
                        if (rc <= 0) {
                            perror("send call failed");
                            exit(1);
                        }
                        if (i > 9) {
                            std::cout << "Aborting connection, no response" << std::endl;
                            exit(0);
                        }
                    }
                }
            }

            if (logInRes[0] == '1') {
                printf("Succesfully logged in \n");
                break;
            } else if (logInRes[0] == '2')
                printf("Already logged in in another session \n");
            else if (logInRes[0] == '*')
                printf("Incorrect format \n");
            else {
                printf("Wrong login:password pair \n");
                std::cout << logInRes << std::endl;
            }
        }
    }


    //main workflow loop
    while(1) {

        std::getline (std::cin, str);

        if (str == "ls") {
            //packing message size 0001 to buffer
            for (int i = 0; i < size_gap - 1; ++i) {
                buf[i] = '0';
            }
            buf[command_descriptor_index - 1] = '1';
            buf[command_descriptor_index] = '1';
            noInput = 0;
        }

        else if (str.compare(0,2,"cd") == 0) {
            for (int i = 0; i < size_gap - 1; ++i) {
                buf[i] = '0';
            }
            buf[command_descriptor_index - 1] = '1';
            buf[command_descriptor_index] = '2';
            if (str[2] != ' ') {
                std::cout << "Incorrect input format" << std::endl;
                continue;
            } else {
                substring = str.substr(3);
                if (substring.size() > buf_size - size_gap - index_gap) {
                    perror("Out of input size, max length is 30 symbols");
                    break;
                } else {
                    for (int i = 0; i < substring.size(); ++i) {
                        buf[i + size_gap + 1] = substring[i];
                    }
                    pack_size(buf, substring.size() + 1);
                }
                substring.clear();
            }
            noInput = 0;
        }

        else if (str=="who") {
            for (int i = 0; i < size_gap - 1; ++i) {
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
                if (substring.size() > buf_size - size_gap - index_gap) {
                    perror("Out of input size");
                    //break;
                } else {
                    for (int i = 0; i < substring.size(); ++i) {
                        buf[i + size_gap + 1] = substring[i];
                    }
                    pack_size(buf, substring.size() + 1);
                }
                substring.clear();
            }
            noInput = 0;
        }

        else if (str == "logout") {
            for (int i = 0; i < size_gap - 1; ++i) {
                buf[i] = '0';
            }
            buf[command_descriptor_index - 1] = '1';
            buf[command_descriptor_index] = '5';
            //rc = send(s, buf, buf_size, 0);
            pack_index(buf, index);
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
            index ++;
            pack_index(buf, index);
            rc = sendto(s, buf, buf_size, 0, (struct sockaddr *) &from, slen);
            if (rc <= 0) {
                perror("send call failed");
                break;
            }

            if ( recvfrom(s, buf_out, buf_out_size, 0, (struct sockaddr *) &from, &slen) < 0 ) {
                for (int i = 0; i <= 10; ++i) {
                    if (recvfrom(s, buf_out, buf_out_size, 0, (struct sockaddr *) &from, &slen) < 0) {
                        rc = sendto(s, buf, buf_size, 0, (struct sockaddr *) &from, slen);
                        if (rc <= 0) {
                            perror("send call failed");
                            break;
                        }
                        if (i > 9) {
                            std::cout << "Server unreachable" << std::endl;
                            exit(0);
                        }
                    }
                }
            }

            if (buf_out[0] == '*')
            {
                std::cout << "Session closed by administrator" << std::endl;
                exit(0);
            }
            std::cout << "Received: \n" << std::string(buf_out) << std::endl; //may not work */
            memset(&buf_out, 0, sizeof(buf_out));
        }

    }

    close(s);
    printf("Session closed, logged out");
    exit( 0 );
}
