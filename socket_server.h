#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include <stdbool.h>
#include <pthread.h>     // Para pthread_t y pthread_mutex_t
#include <netinet/in.h>  // Para sockaddr_in

#define DEFAULT_PORT 8080
#define MAX_CLIENTS 1 // Para este ejemplo, manejaremos un solo cliente a la vez
#define SERVER_BUFFER_SIZE 1024

// Estructura para mantener la información del cliente conectado
typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    bool is_connected;
} client_info_t;


// Funciones de la biblioteca del servidor
void server_init(int port);
int server_start_listening_thread(void); // Inicia el servidor en un nuevo pthread
void server_stop(void);
bool server_is_client_connected(void);
int server_send_data_to_client(const char *data, int length);

// Función que se ejecutará en el hilo del servidor (uso interno)
void* server_thread_func(void* arg);

#endif // SOCKET_SERVER_H