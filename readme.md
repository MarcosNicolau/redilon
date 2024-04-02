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

Then include the library

```c
#include redilon/soquetes.h
#include redilon/paquetes
```

If you want to uninstall the lib run `sudo make uninstall` and `make clean`

## Usage

See [here](./example/) for a full example.

Create a server:

```c
int main()
{
    int server_fd = soquetes_createTcpServer(PORT, QUEUE_SIZE);
    if (server_fd == -1)
    {
        printf("err while creating server: [%s]", strerror(errno));
        return -1;
    }

    /**
     * Here you can choose between the different server mechanism
     *
     * they will return only when the server crashes.
     */
    printf("Server started listening in port %s\n", PORT);
    int status = soquetes_acceptConnectionsAsync(server_fd, MAX_CLIENTS, handleRequest, NULL);
    // int status = soquetes_acceptConnectionsOnDemand(server_fd, handleRequest, NULL);
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
    int server_fd = soquetes_connectToTcpServer(HOST, PORT);
    if (server_fd == -1)
    {
        printf("err while connecting to server: [%s]\n", strerror(errno));
        return NULL;
    }
    paquetes_Packet *packet = paquetes_create(GET_RESOURCES);
    // we pass the resources as an argument to the handle response
    soquetes_sendToServer(server_fd, packet, handleRes, resources);
}
```
