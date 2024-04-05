#include <stdlib.h>
#include <stdio.h>
#include "./proto.h"
#include "../../src/paquetes.h"
#include "../../src/soquetes.h"

// settings host as null to use the loopback interface address
#define HOST NULL
#define PORT "8000"

/**
 * utils
 */
void clearInputBuffer()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

void printMessage(Message *message)
{
    printf("> %s: %s\n", message->name, message->msg);
}

void printAllMessages(Message **messages, int size)
{
    for (int i = 0; i < size; i++)
        printMessage(messages[i]);
};

void addMessageEntry(char *name, char *msg, Message **msg_history, int *size)
{
    struct Message *message = malloc(sizeof(struct Message));
    message->name = name;
    message->msg = msg;
    msg_history[*size] = message;
    *size += 1;
}

/**
 * handlers
 */
struct HandleMessageArgs
{
    int server_fd;
    struct Message **msg_history;
    int *show_messages;
    int *msg_history_size;
};

void handleIncomingMessage(uint8_t client_fd, uint8_t operation, paquetes_Buffer *buffer, void *args)
{
    struct HandleMessageArgs *my_args = args;

    if (operation == NEW_MESSAGE)
    {
        struct Message message;
        decode_message(buffer, &message);
        addMessageEntry(message.name, message.msg, my_args->msg_history, my_args->msg_history_size);

        // print message if not in writing mode active
        if (*my_args->show_messages)
            printMessage(&message);
    }
}

void getIncomingMessages(struct HandleMessageArgs *args)
{
    int res = 0;
    while (res != -1)
    {
        res = soquetes_read(args->server_fd, handleIncomingMessage, args);
    };
    printf("socket connection closed, you'll have to rejoin...\n");
    // close conn
    struct HandleMessageArgs *my_args = args;
    soquetes_closeServerConn(my_args->server_fd);
}

void handleJoin(uint8_t client_fd, uint8_t operation, paquetes_Buffer *buffer, void *args)
{
    if (operation == JOIN_SUCCESS)
        *((int *)args) = 0;
}

int join(int server_fd, char *name)
{
    paquetes_Packet *packet = encode_join(name);
    int joined = -1;
    int res = soquetes_sendToServer(server_fd, packet, handleJoin, &joined);
    if (res == -1 || joined == -1)
        return -1;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printf("You must provide a name \n");
        return 1;
    }
    char *name = argv[1];

    int server_fd = soquetes_connectToTcpServer(HOST, PORT);
    if (server_fd == -1)
    {
        printf("err while connecting to server: [%s]\n", strerror(errno));
        return 1;
    }
    int res = join(server_fd, name);
    if (res == -1)
    {
        printf("could not join\n");
        return 1;
    }
    printf("you joined as %s\n", name);

    // fixed size of 100 message, this should be dynamic...
    struct Message **msg_history = malloc(sizeof(struct Message) * 100);
    int msg_history_size = 0;
    int show_messages = 1;
    if (msg_history == NULL)
    {
        printf("could not allocate memory to msg history structure\n");
        return 1;
    }

    struct HandleMessageArgs args;
    args.server_fd = server_fd;
    args.show_messages = &show_messages;
    args.msg_history_size = &msg_history_size;
    args.msg_history = msg_history;

    // now that we have joined we will receiving message
    // create a thread that reads any incoming message
    pthread_t thread;
    pthread_create(&thread, NULL, (void *)getIncomingMessages, &args);
    pthread_detach(thread);

    while (1)
    {
        char *input = malloc(100);

        // live chat mode
        if (show_messages)
        {
            printf("\npress ENTER to send a message\n\n");
            printAllMessages(msg_history, msg_history_size);
            scanf("%99[^\n]", input);
            show_messages = 0;
            clearInputBuffer();
        };

        // user has pressed enter, enter in writing mode
        system("clear");
        printf("write your message: ");
        scanf("%99[^\n]", input);
        system("clear");

        // send message if it has content
        if (strcmp(input, ""))
        {
            addMessageEntry("you", input, msg_history, &msg_history_size);

            // encode and send to the server
            paquetes_Packet *packet = encode_message(name, input);
            int sent = soquetes_sendToServer(server_fd, packet, NULL, NULL);
            if (sent == -1)
                printf("\nsocket connection closed, you'll have to rejoin...\n");
        }

        show_messages = 1;
        clearInputBuffer();
    }

    return 0;
}