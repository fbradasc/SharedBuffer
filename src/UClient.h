#ifndef U_CLIENT_H
#define U_CLIENT_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netdb.h>
#include <vector>
#include <sstream>
#include <sys/socket.h>

using namespace std;

#define MAXPACKETSIZE 4096

typedef struct
{
    struct cmsghdr h;
    int fd;
} _cmsghdr_t;


class UClient
{
public:
    UClient();
    ~UClient();
    inline bool connected() { return _sock >= 0; }
    bool setup(const string & path);

    bool send_tv(const string & data);
    string recv_tv(int size = MAXPACKETSIZE);

    bool send_iv(int val);
    int  recv_iv();

    bool send_fd(int fd);
    int  recv_fd();

    void exit();

private:
    int                _sock   ;
    struct sockaddr_un _server ;

    bool setup_(const string & path);
};

#endif
