# Redilón

Redilón is an ultralightweight, simple, fast and opinionated c library for sockets communication that features:

-   An intuitive api
-   A standard packet serialization protocol
-   Two server mechanisms: **async non-blocking io** and **on-demand**
-   Robust error handling
-   No memory leaks

## Installation

The installation is simple, just run:

-   `make`
-   `sudo make install`

Then you'd just include the library like so

```c
#include "redilon.h"
```

If you want to uninstall the lib run `sudo make uninstall && make clean`

## Basic Usage

See [here](./examples/) for more detailed examples.

Create a server:

```c
int main()
{
    int server_fd = redilon_createTcpServer(PORT, QUEUE_SIZE);
    if (server_fd == -1)
    {
        printf("err while creating server: [%s]\n", strerror(errno));
        return -1;
    }
    printf("Server started listening in port %s\n", PORT);

    int epoll_fd;

    // server conf
    redilon_AsyncServerConf conf;
    conf.server_fd = server_fd;
    conf.epoll_fd = &epoll_fd;
    conf.max_clients = MAX_CLIENTS;
    conf.handlersArgs = NULL;
    conf.requestHandler = handleRequest;
    conf.onConnectionClosed = onConnectionClosed;
    conf.onNewConnection = NULL;

    int status = redilon_acceptConnectionsAsync(&conf);

    if (status == -1)
    {
        printf("the server has quit unexpectedly: %s", strerror(errno));
        return 1;
    }

    // should never reach this point
    return 0;
}

```

Create a client:

```c
int main() {
    int server_fd = redilon_connectToTcpServer(HOST, PORT);
    if (server_fd == -1)
    {
        printf("err while connecting to server: [%s]\n", strerror(errno));
        return NULL;
    }
    redilon_Packet *packet = redilon_createPacket(GET_RESOURCES);
    // we pass the resources as an argument to the handle response
    int res = redilon_sendToServer(server_fd, packet, handleResponse, resources);
}
```
