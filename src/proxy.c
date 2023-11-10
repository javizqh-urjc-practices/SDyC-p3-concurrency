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

sem_t sem;
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

    // Listen for response messages
    if (recv(sockfd, resp, sizeof(struct response), MSG_WAITALL) < 0) {
        free(resp);
        ERROR("Fail to received");
        return NULL;
    }

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
        return 0;
    }

    setbuf(stdout, NULL);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr.s_addr;
    servaddr.sin_port = htons(client_port);

    len = sizeof(servaddr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        fprintf(stderr, "Socket failed\n");
        return 0;
    }

    if (connect(sockfd, (const struct sockaddr *) &servaddr, len) < 0){
        ERROR("Unable to connect");
        return 0;
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
    int sockfd;
    int counter = 0;
    struct request req;
    struct response resp;

    // 1. Accept connection
    if ((sockfd = accept(server_sockfd, server_addr, &server_len)) < 0) {
        ERROR("failed to accept socket");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    // Listen for response messages
    if (recv(sockfd, &req, sizeof(req), MSG_WAITALL) < 0) {
        ERROR("Fail to received");
        sem_post(&sem);
        pthread_exit(NULL);
    }

    switch (req.action)
    {
    case WRITE:
        printf("[SEC.MICRO][ESCRITOR #%d] modifica contador con valor %d\n",
                req.id, counter);
        break;
    case READ:
        printf("[SEC.MICRO][LECTOR #%d] lee contador con valor %d\n",
                req.id, counter);
        break;
    default:
        break;
    }

    resp.action = req.action;
    resp.counter = counter;
    resp.latency_time = 0;

    sleep(1);

    if (send(sockfd, &resp, sizeof(resp), MSG_WAITALL) < 0) ERROR("Fail to send");

    sem_post(&sem);
    pthread_exit(NULL);
}

pthread_t threads[850]; // TODO: this is stupid
int thread_id = 0;

int proccess_client() {

    printf("Processing\n");
    sem_wait(&sem);
    pthread_create(&threads[thread_id], NULL, proccess_client_thread, NULL);
    thread_id++;
    pthread_detach(threads[thread_id]);

    return 1;
}
// -----------------------------------------------------------------------------