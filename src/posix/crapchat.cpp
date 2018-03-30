/* vim:set ts=4 sw=4 noai sr sta et cin:
 * crapchat.c
 * by Keith Gaughan <kmgaughan@eircom.net>
 *
 * A really crappy chat application demoing Shared Memory and
 * Semaphores.
 *
 * Copyright (c) Keith Gaughan, 2003.
 * This software is free; you can redistribute it and/or modify it
 * under the terms of the Design Science License (DSL). If you
 * didn’t recieve a copy of the DSL with this software, one can be
 * obtained at <http://www.dsl.org/copyleft/dsl.txt>.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "UServer.h"
#include "UClient.h"
#include "SharedBuffer.h"

#if defined(ANDROID)
#define ION_SOCKET_PATH "/data/local/tmp/ion_socket"
#else
#define ION_SOCKET_PATH "/tmp/ion_socket"
#endif

UServer scon;
UClient ccon;

SharedBuffer shb;

#if defined(ANDROID)
#include <sys/syscall.h>

#define my_sigaction(a,b,c)   syscall(__NR_sigaction,a,b,c)
#else
#define my_sigaction        sigaction
#endif

#if defined(DEBUG_MAIN)
#define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif


void my_handler(int sig)
{
    TRACE("\n");

    shb.unmap();

    exit(-1);
}

void worker()
{
    for (;;)
    {
        // Wait the consumer
        //
        shb.wait();

        // Print the reply, if any.
        //
        if (strlen((char*)shb.data()) > 0)
        {
            printf("Reply: %s", (char*)shb.data());
        }

        // Hand over to the producer.
        //
        shb.notify();
    }
}

void client(int fd)
{
    if (shb.map(fd))
    {
        shb.dump();

        while (!shb.grab())
        {
            printf("Another client is attached, waiting\n");
            sleep(1);
        }

        // Put an empty string in the shared memory.
        //
        ((char*)shb.data())[0] = '\0';

        // Unlock the producer to signal they can talk.
        //
        puts("You’re consumer. Signalling to producer...");

        shb.notify();

        for (;;)
        {
            // Wait until the consumer is ready.
            //
            shb.wait();

            // Get your response.
            //
            printf("> ");

            fgets((char*)shb.data(), shb.size(), stdin);

            // Hand over to the consumer.
            //
            shb.notify();

            // Do you want to exit?
            //
            if (strcmp((char*)shb.data(), "\\quit\n") == 0)
            {
                break;
            }
        }
    }
}

void * server(void * m)
{
    TRACE(" - ENTER\n");

    pthread_detach(pthread_self());

    int fd = (int)m;

    while (true)
    {
        srand(time(NULL));

        vector<string> vstr = scon.getMessage(':');

        size_t pnum = vstr.size();

        if ((pnum == 1) && (vstr[0] == "G")) // get
        {
            TRACE(" - Send: %d\n", fd);

#if defined(ANDROID)
            scon.send_fd(fd);
#else
            scon.send_iv(fd);
#endif
        }

        usleep(1000);
    }

    scon.detach();
}

void * dispatcher(void * m)
{
    pthread_detach(pthread_self());

    pthread_t msg;

    scon.setup(ION_SOCKET_PATH);

    if (pthread_create(&msg, NULL, server, m) == 0)
    {
        scon.receive();
    }
}

int main(int argc, char* argv[])
{
    puts("Welcome to CrapChat! Type ‘\\quit’ to exit.\n");

#if 1
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    my_sigaction(SIGINT , &sigIntHandler, NULL);
    my_sigaction(SIGTERM, &sigIntHandler, NULL);
    my_sigaction(SIGQUIT, &sigIntHandler, NULL);
    my_sigaction(SIGABRT, &sigIntHandler, NULL);
    my_sigaction(SIGSEGV, &sigIntHandler, NULL);
    my_sigaction(SIGFPE , &sigIntHandler, NULL);
#else
    signal(SIGINT , &my_handler);
    signal(SIGTERM, &my_handler);
    signal(SIGQUIT, &my_handler);
    signal(SIGABRT, &my_handler);
    signal(SIGSEGV, &my_handler);
    signal(SIGFPE , &my_handler);
#endif
    // Get shared memory segment id off the command line.
    //
    if (argc == 2)
    {
        // Produce (allocate) a shared buffer
        //
        if ((strcmp(argv[1],"-s")==0) && shb.map(0,BUFSIZ,true))
        {
            // Write out the shared memory segment id so the other who
            // wants to chat with us can know.
            //

            shb.dump();

            pthread_t tdis;

            if (pthread_create(&tdis, NULL, dispatcher, (void *)shb.id()) == 0)
            {
                puts("Waiting for the consumer...");

                worker();
            }
        }
#if 0
        else if (strcmp(argv[1],"-k")==0)
        {
            SharedBuffer::destroy(argv[2]);
        }
#endif
    }
    else
    {
        // We’ve a value! That means we’re the consumer
        //
        if (ccon.setup(ION_SOCKET_PATH))
        {
            ostringstream oss;

            oss << "G";

            ccon.send_tv(oss.str());

            int fd = -1;

#if defined(ANDROID)
            fd = ccon.recv_fd();
#else
            fd = ccon.recv_iv();
#endif
            client(fd);
        }
    }
}
