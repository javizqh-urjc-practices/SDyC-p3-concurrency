#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <err.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "proxy.h"

#define N_ARGS 4
#define MAX_PRIORITY_SIZE 16
#define MAX_THREADS 600

typedef struct args {
    int port;
    enum modes priority;
} * Args;

Args check_args(int argc, char *const *argv);

void usage() {
    fprintf(stderr, "usage: ./server --port PORT --priority writer/reader\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *const *argv) {
    int file_fd;

    Args arguments = check_args(argc, argv);

    file_fd = open("server_output.txt", O_RDWR);

    if (file_fd < 0) {
        fprintf(stderr, "usage: file server_output.txt does not exist\n");
        free(arguments);
        exit(EXIT_FAILURE);
    }
        
    load_config_server(arguments->port, arguments->priority, MAX_THREADS, 
                       file_fd);

    close(file_fd);

    // Launch n threads with a maximum amount of 400
    while (1) {
        proccess_client();
    }

    free(arguments);
    close_config_server();

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
                if (strcmp(optarg ,MODE_READER_STR) == 0) {
                    arguments->priority = READER;
                } else if (strcmp(optarg ,MODE_WRITER_STR) == 0) {
                    arguments->priority = WRITER;
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
