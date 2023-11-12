/**
 * @file proxy.c
 * @author Javier Izquierdo Hernandez (j.izquierdoh.2021@alumnos.urjc.es)
 * @brief 
 * @version 0.1
 * @date 2023-11-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "proxy.h"

#define ERROR(msg) fprintf(stderr,"PROXY ERROR: %s\n",msg)
#define MAX_MS_SLEEP_INTERVAL 150
#define MIN_MS_SLEEP_INTERVAL 75

struct request {
    enum operations action;
    unsigned int id;
};

// ------------------------ Global variables for client ------------------------
int client_port;
char client_ip[MAX_IP_SIZE];
// ------------------------ Global variables for server ------------------------
int priority_server;
int server_sockfd;
struct sockaddr *server_addr;
socklen_t server_len;

struct thread_info {
    pthread_t thread;
    int sockfd;
};


sem_t sem;
pthread_mutex_t mutex;
// -----------------------------------------------------------------------------

// ---------------------------- Initialize sockets -----------------------------
int load_config_client(char ip[MAX_IP_SIZE], int port) {
    client_port = port;
    strcpy(client_ip, ip);

    return 1; 
}

int load_config_server(int port, enum modes priority, int max_n_threads) {
    const int enable = 1;
    struct sockaddr_in servaddr;
    int sockfd;
    socklen_t len;

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

    if (listen(sockfd, 5) < 0) {
        ERROR("Unable to listen");
        return 0;
    }

    if (sem_init(&sem, 0, max_n_threads) == -1) {
        err(EXIT_FAILURE, "failed to create semaphore");
    }

    pthread_mutex_init(&mutex, NULL);

    priority_server = priority;
    server_sockfd = sockfd;
    server_addr = (struct sockaddr *) &servaddr;
    server_len = len;

    return sockfd > 0;
}
// -----------------------------------------------------------------------------

// --------------------------- Functions for client ----------------------------
struct response *response(int sockfd) {
    struct response * resp = malloc(sizeof(struct response));
    if (resp == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(resp, 0, sizeof(struct response));
    struct response responmal;

    // Listen for response messages
    if (recv(sockfd, &responmal, sizeof(struct response), MSG_WAITALL) < 0) {
        free(resp);
        ERROR("Fail to received");
        perror("Fail to receive");
        return NULL;
    }
    close(sockfd);
    return resp;
}

struct response * client_connexion(int id, int action) {
    struct in_addr addr;
    struct sockaddr_in servaddr;
    int sockfd;
    socklen_t len;
    struct request req;
    req.action = action;
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
        fprintf(stderr, "Socket failed\n");
        return NULL;
    }

    if (connect(sockfd, (const struct sockaddr *) &servaddr, len) < 0){
        ERROR("Unable to connect");
        return NULL;
    }

    if (send(sockfd, &req, sizeof(req), MSG_WAITALL) < 0) ERROR("Fail to send");

    return response(sockfd);
}

struct response * reader(int id) {
    return client_connexion(id, READ);
}

struct response * writer(int id) {
    return client_connexion(id, WRITE);
}
// ---------------------------- Function for server ----------------------------
void * proccess_client_thread(void * arg) {
    struct thread_info * thread_info = (struct thread_info *) arg;
    int counter = 0;
    struct request req;
    struct response resp;
    struct timespec current_time;

    // Listen for response messages
    if (recv(thread_info->sockfd, &req, sizeof(req), MSG_WAITALL) < 0) {
        ERROR("Fail to received");
        free(thread_info);
        sem_post(&sem);
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mutex);
    clock_gettime(CLOCK_REALTIME, &current_time);
    switch (req.action)
    {
    case WRITE:
        printf("[%ld.%ld][ESCRITOR #%d] modifica contador con valor %d\n",
                current_time.tv_sec, current_time.tv_nsec, req.id, counter);
        break;
    case READ:
        printf("[%ld.%ld][LECTOR #%d] modifica contador con valor %d\n",
                current_time.tv_sec, current_time.tv_nsec, req.id, counter);
        break;
    default:
        break;
    }

    resp.action = req.action;
    resp.counter = counter;
    resp.latency_time = 0;

    // Sleep between 150 and 75 miliseconds
    usleep(rand() % (MAX_MS_SLEEP_INTERVAL - MIN_MS_SLEEP_INTERVAL)
           + MIN_MS_SLEEP_INTERVAL);

    if (send(thread_info->sockfd, &resp, sizeof(resp), MSG_WAITALL) < 0) {
        ERROR("Failed to send");
    } 

    close(thread_info->sockfd);
    free(thread_info);
    sem_post(&sem);
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}

int proccess_client() {
    int sockfd; 
    struct sockaddr * addr = server_addr;
    struct thread_info * thread_info = malloc(sizeof(struct thread_info));
    if (thread_info == NULL) err(EXIT_FAILURE, "failed to alloc memory");
    memset(thread_info, 0, sizeof(struct thread_info));

    if ((sockfd = accept(server_sockfd, addr, &server_len)) < 0) {
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
