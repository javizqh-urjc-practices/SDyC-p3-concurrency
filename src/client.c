#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <err.h>
#include <pthread.h>

#include "stub.h"

#define N_ARGS 8

pthread_mutex_t mutex_a;

typedef struct args {
    char ip [MAX_IP_SIZE];
    int port;
    enum modes mode;
    int threads;
} * Args;

Args check_args(int argc, char *const *argv);

void usage() {
    fprintf(stderr, "usage: ./client --ip IP --port PORT --mode writer/reader --threads N_THREADS\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *const *argv) {
    
    Args arguments = check_args(argc, argv);
    pthread_t threads [arguments->threads];
    int thread_ids[arguments->threads];

    load_config_client(arguments->ip, arguments->port, arguments->mode);

    // Launch n threads
    for (int i = 0; i < arguments->threads; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, client_connection,
                       (void *) &thread_ids[i]);
    }

    // Wait for  n threads
    for (int i = 0; i < arguments->threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(arguments);
    return 0;
}

Args check_args(int argc, char *const *argv) {
    int c;
    int opt_index = 0;
    static struct option long_options[] = {
        {"ip",      required_argument, 0,  0 },
        {"port",    required_argument, 0,  0 },
        {"mode",    required_argument, 0,  0 },
        {"threads", required_argument, 0,  0 },
        {0,         0,                 0,  0 }
    };
    Args arguments = malloc(sizeof(struct args));

    if (arguments == NULL) err(EXIT_FAILURE, "Failed to allocate arguments");
    memset(arguments, 0, sizeof(struct args));

    if (argc - 1 != N_ARGS) {
        free(arguments);
        usage();
    }

    while (1) {
        opt_index = 0;

        c = getopt_long(argc, argv, "", long_options, &opt_index);
        if (c == -1)
            break;

        switch (c) {
        case 0:
            if (strcmp(long_options[opt_index].name, "ip") == 0) {
                strcpy(arguments->ip ,optarg);
            } else if (strcmp(long_options[opt_index].name, "mode") == 0) {

                if (strcmp(optarg ,MODE_READER_STR) == 0) {
                    arguments->mode = READER;
                } else if (strcmp(optarg ,MODE_WRITER_STR) == 0) {
                    arguments->mode = WRITER;
                } else {
                    free(arguments);
                    usage();
                }

            } else if (strcmp(long_options[opt_index].name, "port") == 0) {
                arguments->port = atoi(optarg);
                if (arguments->port <= 0) {
                    free(arguments);
                    usage();
                }                
            } else if (strcmp(long_options[opt_index].name, "threads") == 0) {
                arguments->threads = atoi(optarg);
                if (arguments->threads <= 0) {
                    free(arguments);
                    usage();
                }
            }
            break;
        default:
            printf("?? getopt returned character code 0%o ??\n", c);
        }
    }

    if (optind < argc) {
        free(arguments);
        usage();
    }
    return arguments;
}
