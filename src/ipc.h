#ifndef U_SERVER_H
#define U_SERVER_H

#include <iostream>
#include <sstream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

using namespace std;

#define MAXPACKETSIZE 4096

class IPC
{
public:
    IPC();
    ~IPC();

    inline bool connected() { return _socket >= 0; }

    bool server_setup(const string & path);
    bool client_setup(const string & path);

    void server(void (*server_cb)(IPC *, void *) = NULL, void *server_data = NULL);

    vector<string> parse(const char& separator);

    bool put(const string & data);
    bool get(      string & out, size_t size = MAXPACKETSIZE);

    bool put(const int & val);
    bool get(      int & val);

    bool putfd(const int & fd);
    bool getfd(      int & fd);

    void detach();

private:
    int                _sockfd;
    int                _socket;
    struct sockaddr_un _server;
    struct sockaddr_un _client;
    pthread_t          _thread;
    static string      _message;

    static void * server_cb_(void * argv);
};

#endif
