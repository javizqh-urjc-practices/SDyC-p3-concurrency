#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <err.h>
#include <semaphore.h>
#include <pthread.h>

#include "proxy.h"

#define N_ARGS 4
#define MAX_PRIORITY_SIZE 16
#define MAX_THREADS 400

typedef struct args {
    int port;
    char priority [MAX_PRIORITY_SIZE];
} * Args;

Args check_args(int argc, char *const *argv);

// Thread specific functions
sem_t sem;
void * thread_function(void *arg);

void usage() {
    fprintf(stderr, "usage: ./server --port PORT --priority writer/reader\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *const *argv) {

    Args arguments = check_args(argc, argv);

    if (sem_init(&sem, 0, MAX_THREADS) == -1) {
        free(arguments);
        err(EXIT_FAILURE, "failed to create semaphore");
    }
        
    // load_config_server(arguments->port, arguments->priority, max_threads);

    // Launch n threads with a maximum amount of 400
    while (1) {
        // new_thread();

        // To proxy
        // pthread_t thread;
        // sem_wait(&sem);
        // pthread_create(&thread, NULL, thread_function, NULL);
        // pthread_detach(thread);
    }

    free(arguments);

    return 0;
}

Args check_args(int argc, char *const *argv) {
    int c;
    int opt_index = 0;
    Args arguments = malloc(sizeof(struct args));

    if (arguments == NULL) err(EXIT_FAILURE, "Failed to allocate arguments");
    memset(arguments, 0, sizeof(struct args));

    static struct option long_options[] = {
        {"port",    required_argument, 0,  0 },
        {"priority",    required_argument, 0,  0 },
        {0,         0,                 0,  0 }
    };
    
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
            if (strcmp(long_options[opt_index].name, "priority") == 0) {
                strcpy(arguments->priority ,optarg);
            } else if (strcmp(long_options[opt_index].name, "port") == 0) {
                arguments->port = atoi(optarg);
                if (arguments->port <= 0) {
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
    printf("Thread function\n");
    sleep(3);

    sem_post(&sem);
    pthread_exit(NULL);
}

