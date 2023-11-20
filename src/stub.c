/**
 * @file stub.c
 * @author Javier Izquierdo Hernandez (j.izquierdoh.2021@alumnos.urjc.es)
 * @brief 
 * @version 0.1
 * @date 2023-11-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "stub.h"

#define ERROR(msg) fprintf(stderr,"PROXY ERROR: %s\n",msg)
#define NS_TO_S 1000000000
#define NS_TO_MICROS 1000
#define MICROS_TO_MS 1000
#define MAX_MS_SLEEP_INTERVAL 150
#define MIN_MS_SLEEP_INTERVAL 75

struct request {
    enum operations action;
    unsigned int id;
};

// ------------------------ Global variables for client ------------------------
int client_port;
int client_action;
char client_ip[MAX_IP_SIZE];
// ------------------------ Global variables for server ------------------------
int priority_server;
int server_sockfd;
struct sockaddr *server_addr;
socklen_t server_len;
char * server_counter_file;

struct thread_info {
    pthread_t thread;
    int sockfd;
};


sem_t sem;
pthread_mutex_t mutex_var;
pthread_mutex_t mutex_readers;
pthread_mutex_t mutex_writers;
pthread_cond_t writing;
pthread_cond_t readers_prio;
pthread_cond_t writers_prio;
int counter = 0;
int is_writing = 0;
int queued_writers = 0;
int queued_readers = 0;
// -----------------------------------------------------------------------------

// ---------------------------- Initialize sockets -----------------------------
int load_config_client(char ip[MAX_IP_SIZE], int port, int action) {
    client_port = port;
    strcpy(client_ip, ip);
    client_action = action;

    return 1; 
}

int load_config_server(int port, enum modes priority, int max_n_threads,
                       char * counter_file) {
    const int enable = 1;
    struct sockaddr_in servaddr;
    int sockfd, counter_fd;
    socklen_t len;
    struct sockaddr * addr = malloc(sizeof(struct sockaddr));
    char * buff = malloc(sizeof(20));

    if (addr == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(addr, 0, sizeof(struct sockaddr));

    if (buff == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(buff, 0, sizeof(20));

    setbuf(stdout, NULL);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    len = sizeof(servaddr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        fprintf(stderr, "Socket failed\n");
        return 0;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
        &enable, sizeof(int)) < 0) {
        ERROR("Setsockopt(SO_REUSEADDR) failed");
        return 0;
    }

    if (bind(sockfd, (const struct sockaddr *) &servaddr, len) < 0) {
        ERROR("Unable to bind");
        return 0;
    }

    if (listen(sockfd, max_n_threads) < 0) {
        ERROR("Unable to listen");
        return 0;
    }

    if (sem_init(&sem, 0, max_n_threads) == -1) {
        err(EXIT_FAILURE, "failed to create semaphore");
    }

    pthread_mutex_init(&mutex_var, NULL);
    pthread_mutex_init(&mutex_readers, NULL);
    pthread_mutex_init(&mutex_writers, NULL);
    pthread_cond_init(&writing, NULL);
    pthread_cond_init(&readers_prio, NULL);
    pthread_cond_init(&writers_prio, NULL);

    if ((counter_fd = open(counter_file, O_RDWR)) < 0) {
        ERROR("Unable to open counter output file");
        free(buff);
        return 0;
    }

    if (read(counter_fd, buff, sizeof(20)) < 0) {
        ERROR("Unable to read counter file");
        counter = 0;
    } else {
        counter = atoi(buff);
    }

    free(buff);
    close(counter_fd);

    priority_server = priority;
    server_sockfd = sockfd;
    server_counter_file = counter_file;
    memcpy(addr, (struct sockaddr *) &servaddr, sizeof(struct sockaddr));
    server_addr = addr;
    server_len = len;

    return sockfd > 0;
}

int close_config_server() {
    close(server_sockfd);
    sem_destroy(&sem);
    pthread_mutex_destroy(&mutex_var);
    pthread_mutex_destroy(&mutex_readers);
    pthread_mutex_destroy(&mutex_writers);
    pthread_cond_destroy(&writing);
    pthread_cond_destroy(&readers_prio);
    pthread_cond_destroy(&writers_prio);
    free(server_addr);
    return 1;
}
// -----------------------------------------------------------------------------

// --------------------------- Functions for client ----------------------------
void * client_connection(void * arg) {
    int id = *(int *) arg;
    struct in_addr addr;
    struct sockaddr_in servaddr;
    int sockfd;
    socklen_t len;
    struct response resp;
    struct request req;
    req.action = client_action;
    req.id = id;


    if (inet_pton(AF_INET, (char *) client_ip, &addr) < 0) {
        return NULL;
    }

    setbuf(stdout, NULL);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr.s_addr;
    servaddr.sin_port = htons(client_port);

    len = sizeof(servaddr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("Socket failed\n");
        return NULL;
    }

    if (connect(sockfd, (const struct sockaddr *) &servaddr, len) < 0){
        ERROR("Unable to connect");
        return NULL;
    }

    if (send(sockfd, &req, sizeof(req), MSG_WAITALL) < 0) ERROR("Fail to send");

    // Listen for response messages
    if (recv(sockfd, &resp, sizeof(resp), MSG_WAITALL) < 0) {
        ERROR("Fail to received");
        perror("Fail to receive");
        close(sockfd);
        pthread_exit(NULL);
    }
    switch (client_action) {
    case READER:
        printf("[Cliente #%d] Lector, contador=%d, tiempo=%ld ns.\n", id,
                resp.counter, resp.latency_time);
        break;
    case WRITER:
        printf("[Cliente #%d] Escritor, contador=%d, tiempo=%ld ns.\n", id,
                resp.counter, resp.latency_time);
        break;
    }


    close(sockfd);
    pthread_exit(NULL);
}
// ---------------------------- Function for server ----------------------------
void * proccess_client_thread(void * arg) {
    struct thread_info * thread_info = (struct thread_info *) arg;
    struct request req;
    struct response resp;
    struct timespec current_time, start, end;
    int counter_fd;
    char * buff = malloc(sizeof(20));

    if (buff == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(buff, 0, sizeof(20));

    // Listen for response messages
    if (recv(thread_info->sockfd, &req, sizeof(req), MSG_WAITALL) < 0) {
        ERROR("Fail to received");
        free(thread_info);
        sem_post(&sem);
        pthread_exit(NULL);
    }

    clock_gettime(CLOCK_REALTIME, &current_time);
    switch (req.action)
    {
    case WRITE:
        clock_gettime(CLOCK_MONOTONIC, &start);
        pthread_mutex_lock(&mutex_var);
        queued_writers++;
        if (priority_server == READER) {
            // Check if we do not have readers
            while (queued_readers > 0) {
                pthread_cond_wait(&readers_prio, &mutex_var);
            }
        }
        pthread_mutex_unlock(&mutex_var);

        // REGION CRITICA ------------------------------------------------
        pthread_mutex_lock(&mutex_writers);
        clock_gettime(CLOCK_MONOTONIC, &end);
        is_writing = 1;
        counter++;
        if ((counter_fd = open(server_counter_file, O_TRUNC | O_WRONLY)) < 0) {
            ERROR("Unable to open counter output file");
        } else {
            sprintf(buff, "%d", counter);
            if (write(counter_fd, buff, strlen(buff)) < 0) {
                ERROR("Unable to write counter file");
            }
            close(counter_fd);
        }
        printf("[%ld.%.6ld][ESCRITOR #%d] modifica contador con valor %d\n",
                current_time.tv_sec, current_time.tv_nsec / NS_TO_MICROS,
                req.id, counter);
        resp.action = req.action;
        resp.counter = counter;
        resp.latency_time = end.tv_nsec - start.tv_nsec;

        // Sleep between 150 and 75 miliseconds
        usleep((rand() % (MAX_MS_SLEEP_INTERVAL - MIN_MS_SLEEP_INTERVAL)
            + MIN_MS_SLEEP_INTERVAL) * MICROS_TO_MS);
        is_writing = 0;
        if (priority_server == READER) {
            pthread_cond_broadcast(&writing);
            if (queued_readers == 0) {
                pthread_mutex_unlock(&mutex_writers);
            }
        } else {
            pthread_mutex_unlock(&mutex_writers);
        }
        // REGION CRITICA ------------------------------------------------

        pthread_mutex_lock(&mutex_var);
        queued_writers--;
        if (queued_writers == 0) {
            pthread_cond_broadcast(&writers_prio);
        }
        pthread_mutex_unlock(&mutex_var);
        break;
    case READ:
        clock_gettime(CLOCK_MONOTONIC, &start);
        // LOGICA DE ENTRADA A REGION CRITICA -----------------------------
        pthread_mutex_lock(&mutex_readers);
        queued_readers++;
        if (priority_server == WRITER) {
            while (queued_writers > 0) {
                pthread_cond_wait(&writers_prio, &mutex_readers);
            }
        }
        while (is_writing > 0) {
            pthread_cond_wait(&writing, &mutex_readers);
        }
        pthread_mutex_unlock(&mutex_readers);
        // LOGICA DE ENTRADA A REGION CRITICA -----------------------------
        // REGION CRITICA ------------------------------------------------
        clock_gettime(CLOCK_MONOTONIC, &end);

        printf("[%ld.%.6ld][LECTOR #%d] lee contador con valor %d\n",
                current_time.tv_sec, current_time.tv_nsec / NS_TO_MICROS,
                req.id, counter);
        resp.action = req.action;
        resp.counter = counter;
        resp.latency_time = end.tv_nsec - start.tv_nsec;

        // Sleep between 150 and 75 miliseconds
        usleep((rand() % (MAX_MS_SLEEP_INTERVAL - MIN_MS_SLEEP_INTERVAL)
            + MIN_MS_SLEEP_INTERVAL) * MICROS_TO_MS);
        // REGION CRITICA ------------------------------------------------

        pthread_mutex_lock(&mutex_readers);
        queued_readers--;
        if (queued_readers == 0) {
            pthread_cond_broadcast(&readers_prio);
            if (priority_server == READER) {
                pthread_mutex_unlock(&mutex_writers);
            }
        }
        pthread_mutex_unlock(&mutex_readers);
        break;
    }

    if (resp.latency_time < 0) {
        resp.latency_time = NS_TO_S - resp.latency_time;
    }

    if (send(thread_info->sockfd, &resp, sizeof(resp), MSG_WAITALL) < 0) {
        ERROR("Failed to send");
    }

    sem_post(&sem);
    close(thread_info->sockfd);
    free(thread_info);
    pthread_exit(NULL);
}

int proccess_client() {
    int sockfd; 
    // struct sockaddr * addr = server_addr;
    struct thread_info * thread_info = malloc(sizeof(struct thread_info));
    if (thread_info == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(thread_info, 0, sizeof(struct thread_info));

    if ((sockfd = accept(server_sockfd, server_addr, &server_len)) < 0) {
        ERROR("failed to accept socket");
        return 0;
    }

    // 1. Accept connection
    thread_info->sockfd = sockfd;
    sem_wait(&sem);
    pthread_create(&thread_info->thread, NULL, proccess_client_thread, thread_info);
    pthread_detach(thread_info->thread);

    return 1;
}
// -----------------------------------------------------------------------------
