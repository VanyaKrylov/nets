#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>

const int buf_size = 32;
const int buf_out_size = 256;



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



int main( void )
{
    struct sockaddr_in peer;
    int s;
    int rc;
    //int command;
    int msg_size = 32;
    bool noInput = 1;
    char logInRes[1];
    std::string str;
    std::string substring;

    char buf[ buf_size ];
    char buf_out[buf_out_size];

    peer.sin_family = AF_INET;
    peer.sin_port = htons( 7500 );
    peer.sin_addr.s_addr = inet_addr( "127.0.0.1" );

    s = socket( AF_INET, SOCK_STREAM, 0 );
    if ( s < 0 )
    {
        perror( "socket call failed" );
        exit( 1 );
    }
/*@.bp*/
    rc = connect( s, ( struct sockaddr * )&peer, sizeof( peer ) );
    if ( rc )
    {
        perror( "connect call failed" );
        exit( 1 );
    }

    while (1) {
        printf("Log in: {user_name}:{password}");
        getline(std::cin, str);
        rc = send(s, &str, str.size(), 0);
        if (rc <= 0)
        {
            perror("send call failed");
            exit( 1 );
        }
        rc = readn(s, logInRes, 1);
        if (rc <= 0)
        {
            perror("recv call failed");
            exit( 1 );
        }
        if (logInRes[0] == '1') {
            printf("Succesfully logged in");
            break;
        } else
            printf("Wrong user:password pair");
    }

    while(1) {

        getline(std::cin, str);

        if (str == "ls") {
            buf[0] = '1';
            buf[1] = '1';
            noInput = 0;
        }

        else if (str.compare(0,2,"cd")) {
            buf[1] = '2';
            if (str[2] != ' ') {
                perror("Incorrect input format");
                break;
            } else {
                substring = str.substr(3);
                if (substring.size() > buf_size - 2) {
                    perror("Out of input size, max length is 30 symbols");
                    break;
                } else {
                    for (int i = 0; i < substring.size(); ++i) {
                        buf[i+2] = substring[i];
                    }
                    buf[0] = substring.size() + 1;
                }
                substring.clear();
            }
            noInput = 0;
        }

        else if (str=="who") {
            buf[0] = '1';
            buf[1] = '3';
            noInput = 0;
        }

        else if (str.compare(0,4,"kill")) {
            buf[1] = '4';
            if (str[4] != ' ') {
                perror("Incorrect input format");
                break;
            } else {
                substring = str.substr(5);
                if (substring.size() > buf_size - 2) {
                    perror("Out of input size, max length is 30 symbols");
                    break;
                } else {
                    for (int i = 0; i < substring.size(); ++i) {
                        buf[i+2] = substring[i];
                    }
                    buf[0] = substring.size() + 1;
                }
                substring.clear();
            }
            noInput = 0;
        }

        else if (str == "logout") {
            rc = send(s, "5", 1, 0);
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
            rc = send(s, &buf, msg_size, 0);
            msg_size = 1;
            if (rc <= 0) {
                perror("send call failed");
                break;
            }


            rc = readn(s, buf_out, buf_out_size);
            if (rc <= 0) {
                perror("recv call failed");
                break;
            }
            std::cout << str.substr(1, str[0]);
        }
    }

    printf("Session closed, logged out");
    exit( 0 );
}
