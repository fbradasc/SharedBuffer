#ifndef U_SERVER_H
#define U_SERVER_H

#include <string>
#include <vector>
#include <sys/un.h>
#include <pthread.h>

using namespace std;

#define MAXPACKETSIZE 4096

class IPC
{
public:
    IPC();
    ~IPC();

    inline bool connected() { return _socket >= 0; }

    bool server_setup(const std::string & path);
    bool client_setup(const std::string & path);

    void server(void (*server_cb)(IPC *, void *) = NULL, void *server_data = NULL);

    std::vector<std::string> parse(const char& separator);

    bool put(const std::string & data);
    bool get(      std::string & out, size_t size = MAXPACKETSIZE);

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
    static std::string _message;

    static void * server_cb_(void * argv);
};

#endif
