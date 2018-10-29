#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>

const int login_pair_size = 64;
const int buf_size = 32;
const int buf_out_size = 8192;
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
    for ( int i = 3; i > 0; --i ) {

        if ( size > 0 ) {
            c = size % 10;
            buf[i] = c;
        } else {
            buf[i] = 0;
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
    int command_descriptor_index = message_length_buffer_field;
    //int command;
    int msg_size = 32;
    bool noInput = 1;
    char logInRes[1];
    std::string str;
    std::string substring;

    char login_buf[login_pair_size];
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

    //Log in loop
    /*
    while (1) {
        printf("Log in: {user_name}:{password}");
        getline(std::cin, str);
        if (str.size() > login_pair_size - message_length_buffer_field) {
            perror("Incorrect authentication length, should be less than 64");
        } else {
            pack_size(login_buf, str.size());
            for (int i = 0; i < str.size(); ++i) {
                login_buf[i+message_length_buffer_field] = str[i];
            }
            rc = send(s, &login_buf, login_pair_size, 0);
            if (rc <= 0) {
                perror("send call failed");
                exit(1);
            }
            rc = readn(s, logInRes, 1);
            if (rc <= 0) {
                perror("recv call failed");
                exit(1);
            }
            if (logInRes[0] == '1') {
                printf("Succesfully logged in");
                break;
            } else
                printf("Wrong user:password pair");
        }
    }
*/
    //main workflow loop
    while(1) {

        //std::getline (std::cin, str);
        std::cin >> str;

        std::cout << "Typed " << str << std::endl;
        if (str == "ls") {
            //packing message size 0001 to buffer
            for (int i = 0; i < message_length_buffer_field - 1; ++i) {
                buf[i] = '0';
            }
            buf[command_descriptor_index - 1] = '1';
            buf[command_descriptor_index] = '1';
            noInput = 0;
            std::cout << "ls read";
        }

        else if (str.compare(0,2,"cd") == 0) {
            buf[command_descriptor_index] = '2';
            if (str[2] != ' ') {
                perror("Incorrect input format");
                break;
            } else {
                substring = str.substr(2);
                if (substring.size() > buf_size - message_length_buffer_field) {
                    perror("Out of input size, max length is 30 symbols");
                    break;
                } else {
                    for (int i = 0; i < substring.size(); ++i) {
                        buf[i + message_length_buffer_field] = substring[i];
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
                    break;
                } else {
                    for (int i = 0; i < substring.size(); ++i) {
                        buf[i+message_length_buffer_field] = substring[i];
                    }
                    pack_size(buf, substring.size() + 1);
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
            rc = send(s, buf, buf_size, 0);
            if (rc <= 0) {
                perror("send call failed");
                break;
            }

            rc = readn(s, buf_out, buf_out_size);
            if (rc <= 0) {
                perror("recv call failed");
                break;
            }

            std::cout << std::string(buf_out) << std::endl; //may not work
            //std::cout << str.substr(1, str[0]);
            //str.clear();
        }

    }

    printf("Session closed, logged out");
    exit( 0 );
}
