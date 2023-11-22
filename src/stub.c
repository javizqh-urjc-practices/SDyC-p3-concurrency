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
#define INFO(msg) fprintf(stderr,"PROXY INFO: %s\n",msg)
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
int server_ratio;
int active_ratio;
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
pthread_cond_t readers_ratio;
int counter = 0;
int is_writing = 0;
int queued_writers = 0;
int queued_readers = 0;
// -----------------------------------------------------------------------------

// ---------------------------- Initialize sockets -----------------------------
int load_config_client(char ip[MAX_IP_SIZE], int port, int action) {
    // Set global variables ----------------------------------------------------
    client_port = port;
    strcpy(client_ip, ip);
    client_action = action;

    return 1; 
}

int load_config_server(int port, enum modes priority, int max_n_threads,
                       char * counter_file, int ratio) {
    const int enable = 1;
    struct sockaddr_in servaddr;
    int sockfd, counter_fd;
    socklen_t len;
    struct sockaddr * addr = malloc(sizeof(struct sockaddr));
    char * buff = malloc(sizeof(long));

    if (addr == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(addr, 0, sizeof(struct sockaddr));

    if (buff == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(buff, 0, sizeof(long));

    // Create socket and liste -------------------------------------------------
    setbuf(stdout, NULL);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    len = sizeof(servaddr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        ERROR("Socket failed");
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

    // Init mutex and semaphores -----------------------------------------------
    if (sem_init(&sem, 0, max_n_threads) == -1) {
        err(EXIT_FAILURE, "failed to create semaphore");
    }

    pthread_mutex_init(&mutex_var, NULL);
    pthread_mutex_init(&mutex_readers, NULL);
    pthread_mutex_init(&mutex_writers, NULL);
    pthread_cond_init(&writing, NULL);
    pthread_cond_init(&readers_prio, NULL);
    pthread_cond_init(&writers_prio, NULL);
    pthread_cond_init(&readers_ratio, NULL);

    // Read counter file -------------------------------------------------------
    if ((counter_fd = open(counter_file, O_RDONLY)) < 0) {
        INFO("Unable to open counter output file, creating new one.");
        counter_fd = open(counter_file, O_CREAT | O_RDONLY, 0777);
    }

    if (read(counter_fd, buff, sizeof(20)) < 0) {
        ERROR("Unable to read counter file");
        counter = 0;
    } else {
        counter = atoi(buff);
    }

    free(buff);
    close(counter_fd);
    // Set global variables ----------------------------------------------------
    priority_server = priority;
    server_ratio = ratio;
    active_ratio = server_ratio;
    server_sockfd = sockfd;
    server_counter_file = counter_file;
    memcpy(addr, (struct sockaddr *) &servaddr, sizeof(struct sockaddr));
    server_addr = addr;
    server_len = len;

    return sockfd > 0;
}

int close_config_server() {
    // Close initial socket ----------------------------------------------------
    close(server_sockfd);
    // Destroy thread control structures ---------------------------------------
    sem_destroy(&sem);
    pthread_mutex_destroy(&mutex_var);
    pthread_mutex_destroy(&mutex_readers);
    pthread_mutex_destroy(&mutex_writers);
    pthread_cond_destroy(&writing);
    pthread_cond_destroy(&readers_prio);
    pthread_cond_destroy(&writers_prio);
    pthread_cond_destroy(&readers_ratio);
    // Free global variables ---------------------------------------------------
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

    // Create socket and connect -----------------------------------------------
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

    // Send request message ----------------------------------------------------
    if (send(sockfd, &req, sizeof(req), MSG_WAITALL) < 0) ERROR("Fail to send");

    // Listen for response messages --------------------------------------------
    if (recv(sockfd, &resp, sizeof(resp), MSG_WAITALL) < 0) {
        ERROR("Fail to received");
        perror("Fail to receive");
        close(sockfd);
        pthread_exit(NULL);
    }
    // Print response ----------------------------------------------------------
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

    // Close socket and exit ---------------------------------------------------
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
    char * buff = malloc(sizeof(long));

    if (buff == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(buff, 0, sizeof(long));

    // Listen for response messages --------------------------------------------
    if (recv(thread_info->sockfd, &req, sizeof(req), MSG_WAITALL) < 0) {
        ERROR("Fail to received");
        free(thread_info);
        sem_post(&sem);
        pthread_exit(NULL);
    }

    // Get current time --------------------------------------------------------
    clock_gettime(CLOCK_REALTIME, &current_time);
    // 2 types of processing ---------------------------------------------------
    switch (req.action)
    {
    case WRITE:
        // LOGICA DE ENTRADA A REGION CRITICA ----------------------------------
        pthread_mutex_lock(&mutex_var);
        queued_writers++;
        if (priority_server == READER) {
            // Check if we do not have readers
            if (queued_readers > 0) {
                pthread_cond_wait(&readers_prio, &mutex_var);
            }
        }
        pthread_mutex_unlock(&mutex_var);
        // LOGICA DE ENTRADA A REGION CRITICA ----------------------------------

        // REGION CRITICA ------------------------------------------------
        clock_gettime(CLOCK_MONOTONIC, &start);
        pthread_mutex_lock(&mutex_writers);
        clock_gettime(CLOCK_MONOTONIC, &end);
        is_writing = 1;
        counter++;
        
        if ((counter_fd = open(server_counter_file, O_TRUNC | O_WRONLY)) < 0) {
            INFO("Unable to open counter output file, creating new one.");
            counter_fd = open(server_counter_file, O_CREAT | O_WRONLY, 0777);
        }
        sprintf(buff, "%d", counter);
        if (write(counter_fd, buff, strlen(buff)) < 0) {
            ERROR("Unable to write counter file");
        }
        close(counter_fd);

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

        pthread_cond_broadcast(&writing);
        if (priority_server == READER) {
            if (queued_readers == 0) {
                pthread_mutex_unlock(&mutex_writers);
            } else if (queued_readers > 0 && server_ratio > 0) {
                // Readers ratio
                for (int i = 0; i <= server_ratio - 1; i++) {
                    pthread_cond_signal(&readers_ratio);
                }
            }
        } else {
            // Exam Part -------------------------------------------------------
            if (queued_readers > 0 && server_ratio > 0) {
                // Writers ratio
                active_ratio--;
                if (active_ratio <= 0) {
                    active_ratio = server_ratio; // Reset ratio
                    pthread_cond_signal(&writers_prio);
                } else {
                    pthread_mutex_unlock(&mutex_writers);
                }
            } else {
                pthread_mutex_unlock(&mutex_writers);
            }
        }
        // REGION CRITICA ------------------------------------------------------

        // LOGICA DE SALIDA DE REGION CRITICA ----------------------------------
        pthread_mutex_lock(&mutex_var);
        queued_writers--;
        if (queued_writers == 0) {
            pthread_cond_broadcast(&writers_prio);
        }
        pthread_mutex_unlock(&mutex_var);
        // LOGICA DE SALIDA DE REGION CRITICA ----------------------------------
        break;
    case READ:
        clock_gettime(CLOCK_MONOTONIC, &start);
        // LOGICA DE ENTRADA A REGION CRITICA ----------------------------------
        pthread_mutex_lock(&mutex_readers);
        queued_readers++;
        if (priority_server == WRITER) {
            if (queued_writers > 0) {
                pthread_cond_wait(&writers_prio, &mutex_readers);
            }
        }
        // Exam Part -----------------------------------------------------------
        // Check also if there are writers and ratio is positive
        if (priority_server == READER && queued_writers > 0 && 
            server_ratio > 0) {
            // Readers ratio
            pthread_cond_wait(&readers_ratio, &mutex_readers);
        }
        // ---------------------------------------------------------------------
        while (is_writing > 0) {
            pthread_cond_wait(&writing, &mutex_readers);
        }
        pthread_mutex_unlock(&mutex_readers);
        // LOGICA DE ENTRADA A REGION CRITICA ----------------------------------

        // REGION CRITICA ------------------------------------------------------
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
        // REGION CRITICA ------------------------------------------------------

        // LOGICA DE SALIDA DE REGION CRITICA ----------------------------------
        pthread_mutex_lock(&mutex_readers);
        queued_readers--;
        if (queued_readers == 0) {
            pthread_cond_broadcast(&readers_prio);
            if (priority_server == READER) {
                pthread_mutex_unlock(&mutex_writers);
            }
        }
        // Exam Part -----------------------------------------------------------
        // Check also if there are witers and ratio is positive
        if (queued_writers > 0 && server_ratio > 0) {
            if (priority_server == WRITER) {
                // Writers ratio
                pthread_mutex_unlock(&mutex_writers);
            } else {
                // Readers ratio
                active_ratio--;
                if (active_ratio <= 0) {
                    active_ratio = server_ratio; // Reset ratio
                    pthread_mutex_unlock(&mutex_writers);
                    pthread_cond_signal(&readers_prio);
                }
            }
        }
        pthread_mutex_unlock(&mutex_readers);
        // LOGICA DE SALIDA DE REGION CRITICA ----------------------------------
        break;
    }

    // Send respnse to client --------------------------------------------------
    if (resp.latency_time < 0) {
        // If 1.9 and 2.1 get 0.2
        resp.latency_time = NS_TO_S - resp.latency_time;
    }

    if (send(thread_info->sockfd, &resp, sizeof(resp), MSG_WAITALL) < 0) {
        ERROR("Failed to send");
    }
    // Free all memory and close sockets ---------------------------------------
    close(thread_info->sockfd);
    free(thread_info);
    // Allow another thread in -------------------------------------------------
    sem_post(&sem);
    pthread_exit(NULL);
}

int proccess_client() {
    int sockfd; 
    struct thread_info * thread_info = malloc(sizeof(struct thread_info));

    if (thread_info == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(thread_info, 0, sizeof(struct thread_info));

    // Accept connection -------------------------------------------------------
    if ((sockfd = accept(server_sockfd, server_addr, &server_len)) < 0) {
        ERROR("failed to accept socket");
        return 0;
    }

    thread_info->sockfd = sockfd;
    // Create only 600 threads -------------------------------------------------
    sem_wait(&sem);
    pthread_create(&thread_info->thread, NULL, proccess_client_thread,
                   thread_info);
    pthread_detach(thread_info->thread); // To free thread memory after finish
    // -------------------------------------------------------------------------
    return 1;
}
// -----------------------------------------------------------------------------
