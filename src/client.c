#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <err.h>
#include <pthread.h>

#include "proxy.h"

#define N_ARGS 8
#define MAX_IP_SIZE 16
#define MODE_READER_STR "reader"
#define MODE_WRITER_STR "writer"

enum modes {
    WRITER = 0,
    READER
};

typedef struct args {
    char ip [MAX_IP_SIZE];
    int port;
    enum modes mode;
    int threads;
} * Args;

Args check_args(int argc, char *const *argv);

// Thread specific functions
enum modes execution_mode;
void * thread_function(void *arg);

void usage() {
    fprintf(stderr, "usage: ./client --ip IP --port PORT --mode writer/reader --threads N_THREADS\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *const *argv) {
    
    Args arguments = check_args(argc, argv);
    pthread_t threads [arguments->threads];
    int thread_ids[arguments->threads];
    
    execution_mode = arguments->mode;

    // load_config_client(arguments->ip, arguments->port);

    // Launch n threads
    for (int i = 0; i < arguments->threads; i++) {
        thread_ids[i] = i;
        pthread_create(&threads[i], NULL, thread_function,
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

void * thread_function(void *arg) {
    int id = *(int *) arg;
    switch (execution_mode) {
    case READER:
        // reader();
        // response(); // Returns X and Y values
        printf("[Cliente #%d] Lector, contador=X, tiempo=Y ns.\n", id);
        break;
    case WRITER:
        // writer();
        // response(); // Returns X and Y values
        printf("[Cliente #%d] Escritor, contador=X, tiempo=Y ns.\n", id);
        break;
    }
    pthread_exit(NULL);
}
