#ifndef U_SERVER_H
#define U_SERVER_H

#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>

using namespace std;

#define MAXPACKETSIZE 4096

class UServer
{
public:
    int _sockfd;
    int _sock;
    struct sockaddr_un _server_addr;
    struct sockaddr_un _client_addr;
    pthread_t _server_thread;
    static string _message;

    UServer();

    void setup(const string & path);

    void receive();
    vector<string> getMessage(const char& separator);
    void send_iv(int val);

    bool send_fd(int fd);
    int  recv_fd();

    void detach();
    void clean();

private:
    static void * task_(void * argv);
};

#endif
