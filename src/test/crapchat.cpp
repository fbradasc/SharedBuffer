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

#include "SharedBuffer.h"

SharedBuffer shb;

#if defined(DEBUG_MAIN)
#define TRACE(format, ...)      printf("%s:%d - " format, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define TRACE(format, ...)
#endif


void server()
{
    shb.dump();

    printf("\nYou’re producer. Waiting for customers...\n");

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

void client()
{
    shb.dump();

    printf("\n");

    const char p[] = "|/-\\";

    for (int i=0; !shb.grab(); i++)
    {
        i %= sizeof(p) - 1;

        printf("Another client is attached, waiting %c\r", p[i]);
        fflush(stdout);

        sleep(1);
    }

    // Put an empty string in the shared memory.
    //
    ((char*)shb.data())[0] = '\0';

    // Unlock the producer to signal they can talk.
    //
    printf("You’re consumer. Signalling to producer...\n");

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

void show_help(int argc, char* argv[])
{
    printf("Usage:\n\n%s <-s|-c> <buffer_name>\n\n", argv[0]);
}

int main(int argc, char* argv[])
{
    puts("Welcome to CrapChat! Type ‘\\quit’ to exit.\n");

    TRACE("argc=%d\n",argc);
    for (int i=0; i<argc; i++)
    {
        TRACE("argv[%d]=%s\n",i,argv[i]);
    }

    // Get shared memory segment id off the command line.
    //
    if (argc == 3)
    {
        // Produce (allocate) a shared buffer
        //
        if ((strcmp(argv[1],"-s")==0) && shb.map(argv[2],BUFSIZ,true))
        {
            // Write out the shared memory segment id so the other who
            // wants to chat with us can know.
            //
            server();
        }
        else
        if ((strcmp(argv[1],"-c")==0) && shb.map(argv[2]))
        {
            client();
        }
#if 0
        else if (strcmp(argv[1],"-k")==0)
        {
            SharedBuffer::destroy(argv[2]);
        }
#endif
        else
        {
            show_help(argc, argv);
        }
    }
    else
    {
        show_help(argc, argv);
    }
}
