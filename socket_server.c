#include "socket_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>  // Para inet_ntoa
#include <signal.h>     // Para manejo de SIGPIPE

// Variables globales para el servidor
static int server_socket_fd = -1;
static client_info_t connected_client;
static int server_listen_port;
static volatile bool server_running = false;
static pthread_t server_thread_id; // ID del pthread del servidor

static pthread_mutex_t client_socket_mutex; // Mutex POSIX


void server_init(int port) {
    server_listen_port = (port > 0 && port < 65536) ? port : DEFAULT_PORT;
    connected_client.socket_fd = -1;
    connected_client.is_connected = false;
    server_running = false;
    server_thread_id = 0; // Inicializar a un valor no válido

    // Ignorar SIGPIPE para que send() retorne error en lugar de terminar el programa
    signal(SIGPIPE, SIG_IGN);

    if (pthread_mutex_init(&client_socket_mutex, NULL) != 0) {
        perror("Error inicializando mutex para socket de cliente");
        // Considerar manejo de error más robusto
    }
    printf("Servidor inicializado en el puerto %d.\n", server_listen_port);
}

void* server_thread_func(void* arg) {
    struct sockaddr_in server_addr;
    int opt = 1;

    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("Error al crear el socket del servidor");
        server_running = false;
        return NULL;
    }

    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) falló");
        close(server_socket_fd);
        server_socket_fd = -1;
        server_running = false;
        return NULL;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_listen_port);

    if (bind(server_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al vincular el socket del servidor");
        close(server_socket_fd);
        server_socket_fd = -1;
        server_running = false;
        return NULL;
    }

    if (listen(server_socket_fd, MAX_CLIENTS) < 0) {
        perror("Error al poner el socket en modo escucha");
        close(server_socket_fd);
        server_socket_fd = -1;
        server_running = false;
        return NULL;
    }

    printf("Servidor escuchando en el puerto %d...\n", server_listen_port);
    server_running = true;

    while (server_running) {
        socklen_t client_addr_len = sizeof(connected_client.address);
        int new_socket_fd;

        //printf("Esperando conexión de un cliente...\n"); // Puede ser muy verboso
        new_socket_fd = accept(server_socket_fd, (struct sockaddr *)&connected_client.address, &client_addr_len);

        if (!server_running) {
            if (new_socket_fd >= 0) close(new_socket_fd);
            break;
        }

        if (new_socket_fd < 0) {
            if (server_running) { // Solo mostrar error si no nos estamos deteniendo (errno EBADF or EINVAL if socket closed)
                 // perror("Error al aceptar la conexión del cliente"); // Puede ser ruidoso si se cierra el socket intencionalmente
            }
            continue;
        }

        pthread_mutex_lock(&client_socket_mutex);
        if (connected_client.is_connected) {
            printf("Cliente ya conectado. Rechazando nueva conexión de %s.\n", inet_ntoa(connected_client.address.sin_addr));
            close(new_socket_fd);
            pthread_mutex_unlock(&client_socket_mutex);
            continue;
        }

        connected_client.socket_fd = new_socket_fd;
        connected_client.is_connected = true;
        pthread_mutex_unlock(&client_socket_mutex);

        printf("Cliente conectado desde %s:%d (socket fd: %d)\n",
               inet_ntoa(connected_client.address.sin_addr),
               ntohs(connected_client.address.sin_port),
               connected_client.socket_fd);

        char temp_buffer[SERVER_BUFFER_SIZE]; // Buffer para recibir datos del cliente si es necesario
        while(server_running && connected_client.is_connected) {
            // Intentar leer del cliente. Esto también detectará desconexiones.
            ssize_t n = recv(connected_client.socket_fd, temp_buffer, sizeof(temp_buffer)-1, 0);
            if (n == 0) { // Cliente cerró la conexión ordenadamente
                printf("Cliente (fd: %d) desconectado.\n", connected_client.socket_fd);
                break;
            } else if (n < 0) { // Error en recv
                if (server_running) { // Solo si no nos estamos deteniendo
                    // perror("Error en recv() del cliente"); // Puede ser normal si cerramos el socket
                }
                break;
            }
            // Si se reciben datos del cliente (n > 0), se pueden procesar aquí.
            // temp_buffer[n] = '\0';
            // printf("Recibido del cliente (fd: %d): %s\n", connected_client.socket_fd, temp_buffer);
        }

        pthread_mutex_lock(&client_socket_mutex);
        if (connected_client.socket_fd != -1) {
            close(connected_client.socket_fd);
        }
        connected_client.socket_fd = -1;
        connected_client.is_connected = false;
        pthread_mutex_unlock(&client_socket_mutex);
        if (server_running) { // Solo imprimir si no es parte del cierre del servidor
           printf("Conexión con cliente (anterior fd) cerrada.\n");
        }
    }

    if (server_socket_fd != -1) {
        close(server_socket_fd);
        server_socket_fd = -1;
    }
    printf("Hilo del servidor terminado.\n");
    return NULL;
}

int server_start_listening_thread(void) {
    if (server_running) {
        printf("El servidor ya está corriendo.\n");
        return 0;
    }
    if (pthread_create(&server_thread_id, NULL, server_thread_func, NULL) != 0) {
        perror("Error creando el hilo del servidor de sockets");
        server_thread_id = 0;
        return -1;
    }
    // pthread_detach(server_thread_id); // Opcional: si no planeas hacer join explícito.
                                      // Pero es mejor hacer join en server_stop.
    printf("Hilo del servidor de sockets creado con ID %lu.\n", (unsigned long)server_thread_id);
    return 0;
}

void server_stop(void) {
    printf("Intentando detener el servidor...\n");


    pthread_mutex_lock(&client_socket_mutex);
    if (connected_client.socket_fd != -1) {
        // shutdown es una forma más "amable" de indicar el cierre antes de close()
        shutdown(connected_client.socket_fd, SHUT_RDWR);
        close(connected_client.socket_fd);
        connected_client.socket_fd = -1; // Marcar como cerrado
        connected_client.is_connected = false;
        printf("Socket del cliente cerrado desde server_stop.\n");
    }
    pthread_mutex_unlock(&client_socket_mutex);

    // Primero, indicar al hilo que debe detenerse
    server_running = false; // El hilo del servidor revisará esta variable

    // Luego, cerrar el socket de escucha principal. Esto hará que `accept()`
    // en el hilo del servidor retorne (probablemente con error), permitiendo
    // que el bucle while(server_running) termine.
    if (server_socket_fd != -1) {
        // shutdown(server_socket_fd, SHUT_RDWR); // Puede ayudar a desbloquear accept
        close(server_socket_fd); // Esto es crucial para desbloquear accept
        server_socket_fd = -1; // Marcar como cerrado
        printf("Socket de escucha del servidor cerrado desde server_stop.\n");
    }

    if (server_thread_id != 0) {
        printf("Esperando que el hilo del servidor (%lu) termine...\n", (unsigned long)server_thread_id);
        pthread_join(server_thread_id, NULL); // Esperar a que el hilo termine su ejecución
        printf("Hilo del servidor (%lu) ha terminado.\n", (unsigned long)server_thread_id);
        server_thread_id = 0;
    }
    pthread_mutex_destroy(&client_socket_mutex);
    printf("Servidor detenido.\n");
}

bool server_is_client_connected(void) {
    bool status;
    pthread_mutex_lock(&client_socket_mutex);
    status = connected_client.is_connected;
    pthread_mutex_unlock(&client_socket_mutex);
    return status;
}

int server_send_data_to_client(const char *data, int length) {
    int bytes_sent = -1;
    if (!data || length <= 0) {
        return -1;
    }

    pthread_mutex_lock(&client_socket_mutex);
    if (connected_client.is_connected && connected_client.socket_fd != -1) {
        // Usar MSG_NOSIGNAL es una buena práctica en Linux para evitar SIGPIPE
        // Si no está disponible, asegúrate de haber llamado signal(SIGPIPE, SIG_IGN);
        #ifdef __linux__
        bytes_sent = send(connected_client.socket_fd, data, length, MSG_NOSIGNAL);
        #else
        bytes_sent = send(connected_client.socket_fd, data, length, 0);
        #endif

        if (bytes_sent < 0) {
            perror("Error al enviar datos al cliente");
            // Considerar que el cliente se desconectó si send falla (ej. EPIPE)
            // close(connected_client.socket_fd);
            // connected_client.socket_fd = -1;
            // connected_client.is_connected = false;
        } else if (bytes_sent < length) {
            printf("Advertencia: No se enviaron todos los datos al cliente (%d de %d bytes)\n", bytes_sent, length);
        } else {
            // Puede ser muy verboso, comentar si no es necesario para depuración
            // printf("Datos enviados al cliente (fd: %d): %.*s\n", connected_client.socket_fd, length, data);
        }
    } else {
        bytes_sent = -2; // Código para indicar "no conectado" o "datos no enviados"
    }
    pthread_mutex_unlock(&client_socket_mutex);
    return bytes_sent;
}