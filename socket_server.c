#include "socket_server.h"
#include "cethreads.h" // Para cethread_create, etc.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> // Para sockaddr_in
#include <arpa/inet.h>  // Para inet_ntoa

// Variables globales para el servidor
static int server_socket_fd = -1;
static client_info_t connected_client;
static int server_listen_port;
static volatile bool server_running = false;
static int g_hilo_servidor_id = -1; // ID del cethread del servidor

// Mutex para proteger el acceso a connected_client.socket_fd al enviar datos
// Asumimos que tienes un sistema de mutex como cethread_mutex_t y sus funciones
// Si no, necesitarás implementar o usar pthreads directamente para esto.
// Por simplicidad y dado tu proyecto, usaremos cethread_mutex_t.
// Asegúrate de que `struct&enum.h` o `cethreads.h` definan cethread_mutex_t
// y que tengas funciones como cethread_mutex_init, lock, unlock, destroy.

// Si tu cethreads.h ya define cethread_mutex_t, no necesitas redefinirlo.
// De lo contrario, necesitarías incluir la definición o usar pthreads.
// Aquí asumiré que cethread_mutex_t está disponible y funciona.
static cethread_mutex_t client_socket_mutex;


void server_init(int port) {
    server_listen_port = (port > 0 && port < 65536) ? port : DEFAULT_PORT;
    connected_client.socket_fd = -1;
    connected_client.is_connected = false;
    server_running = false;
    // Inicializar el mutex para el socket del cliente
    if (cethread_mutex_init(&client_socket_mutex, NULL) != 0) {
        perror("Error inicializando mutex para socket de cliente");
        // Manejar el error apropiadamente, quizás no iniciar el servidor.
    }
    printf("Servidor inicializado en el puerto %d.\n", server_listen_port);
}

void* server_thread_func(void* arg) {
    struct sockaddr_in server_addr;
    int opt = 1;

    // 1. Crear el socket del servidor
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("Error al crear el socket del servidor");
        server_running = false;
        return NULL;
    }

    // 2. (Opcional) Configurar para reutilizar la dirección/puerto
    if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) falló");
        close(server_socket_fd);
        server_running = false;
        return NULL;
    }

    // Configurar la dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Escuchar en todas las interfaces disponibles
    server_addr.sin_port = htons(server_listen_port);

    // 3. Vincular (Bind) el socket
    if (bind(server_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al vincular el socket del servidor");
        close(server_socket_fd);
        server_running = false;
        return NULL;
    }

    // 4. Escuchar (Listen) por conexiones entrantes
    if (listen(server_socket_fd, MAX_CLIENTS) < 0) {
        perror("Error al poner el socket en modo escucha");
        close(server_socket_fd);
        server_running = false;
        return NULL;
    }

    printf("Servidor escuchando en el puerto %d...\n", server_listen_port);
    server_running = true;

    while (server_running) {
        socklen_t client_addr_len = sizeof(connected_client.address);
        int new_socket_fd;

        printf("Esperando conexión de un cliente...\n");

        // 5. Aceptar una conexión entrante
        // accept() es bloqueante. Si server_running se vuelve false, necesitamos una forma de desbloquearlo.
        // Una forma es cerrar server_socket_fd desde server_stop().
        new_socket_fd = accept(server_socket_fd, (struct sockaddr *)&connected_client.address, &client_addr_len);

        if (!server_running) { // Verificar después de accept, por si se detuvo mientras estaba bloqueado
            if (new_socket_fd >= 0) close(new_socket_fd);
            break;
        }

        if (new_socket_fd < 0) {
            if (server_running) { // Solo mostrar error si no nos estamos deteniendo
                 perror("Error al aceptar la conexión del cliente");
            }
            continue; // Continuar esperando si el servidor aún debe correr
        }

        // Si ya hay un cliente conectado, rechazar el nuevo (o manejarlo como prefieras)
        cethread_mutex_lock(&client_socket_mutex);
        if (connected_client.is_connected) {
            printf("Ya hay un cliente conectado. Rechazando nueva conexión de %s.\n", inet_ntoa(connected_client.address.sin_addr));
            close(new_socket_fd);
            cethread_mutex_unlock(&client_socket_mutex);
            continue;
        }

        connected_client.socket_fd = new_socket_fd;
        connected_client.is_connected = true;
        cethread_mutex_unlock(&client_socket_mutex);

        printf("Cliente conectado desde %s:%d (socket fd: %d)\n",
               inet_ntoa(connected_client.address.sin_addr),
               ntohs(connected_client.address.sin_port),
               connected_client.socket_fd);

        // Mantener la conexión hasta que el cliente se desconecte o el servidor se detenga
        // Podrías tener un bucle aquí para recibir datos del cliente si fuera necesario,
        // o simplemente mantenerlo vivo para enviar datos desde la simulación.
        // Para este ejemplo, solo esperamos a que el cliente se desconecte o el servidor pare.
        char temp_buffer[1]; // Para detectar desconexión
        while(server_running && connected_client.is_connected) {
            // Intentar leer 0 bytes no bloqueante, o usar select/poll para estado del socket.
            // Una forma simple de detectar desconexión es si recv devuelve 0 o -1.
            // Haremos recv bloqueante aquí; si se necesita más sofisticación, usar select/poll.
            ssize_t n = recv(connected_client.socket_fd, temp_buffer, sizeof(temp_buffer)-1, 0);
            if (n == 0) {
                printf("Cliente (fd: %d) desconectado.\n", connected_client.socket_fd);
                break;
            } else if (n < 0) {
                // Podría ser un error real o porque el servidor se está deteniendo y cerramos el socket.
                if (server_running) { // Solo si no nos estamos deteniendo
                    perror("Error en recv() del cliente");
                }
                break;
            }
            // Si recibiste datos (n > 0), puedes procesarlos aquí.
            // printf("Recibido del cliente (fd: %d): %.*s\n", connected_client.socket_fd, (int)n, temp_buffer);
        }

        // Limpiar información del cliente
        cethread_mutex_lock(&client_socket_mutex);
        if (connected_client.socket_fd != -1) {
            close(connected_client.socket_fd);
        }
        connected_client.socket_fd = -1;
        connected_client.is_connected = false;
        cethread_mutex_unlock(&client_socket_mutex);
        printf("Conexión con cliente (anterior fd) cerrada.\n");

    } // Fin del bucle while(server_running)

    if (server_socket_fd != -1) {
        close(server_socket_fd);
        server_socket_fd = -1;
    }
    printf("Hilo del servidor terminado.\n");
    return NULL;
}

void server_start_listening_thread(void) {
    if (server_running) {
        printf("El servidor ya está corriendo.\n");
        return;
    }
    if (cethread_create(&g_hilo_servidor_id, server_thread_func, NULL) != 0) {
        fprintf(stderr, "Error creando el hilo del servidor de sockets.\n");
        g_hilo_servidor_id = -1;
    } else {
        printf("Hilo del servidor de sockets creado con ID %d.\n", g_hilo_servidor_id);
        // No se hace join aquí, el hilo corre en segundo plano.
        // El join se haría en server_stop si es necesario esperar a que termine limpiamente.
    }
}

void server_stop(void) {
    printf("Intentando detener el servidor...\n");
    server_running = false; // Señal para que el bucle principal del hilo termine

    // Si hay un cliente conectado, cerrar su socket para desbloquear recv
    cethread_mutex_lock(&client_socket_mutex);
    if (connected_client.socket_fd != -1) {
        // shutdown(connected_client.socket_fd, SHUT_RDWR); // Opcional, para indicar cierre al cliente
        close(connected_client.socket_fd);
        connected_client.socket_fd = -1;
        connected_client.is_connected = false;
        printf("Socket del cliente cerrado desde server_stop.\n");
    }
    cethread_mutex_unlock(&client_socket_mutex);

    // Cerrar el socket de escucha principal para desbloquear accept()
    if (server_socket_fd != -1) {
        // shutdown(server_socket_fd, SHUT_RD); // Para desbloquear accept
        close(server_socket_fd); // Esto debería hacer que accept() retorne un error
        server_socket_fd = -1; // Marcar como cerrado
        printf("Socket de escucha del servidor cerrado desde server_stop.\n");
    }

    // Esperar a que el hilo del servidor termine (opcional, pero buena práctica)
    if (g_hilo_servidor_id != -1) {
        printf("Esperando que el hilo del servidor (%d) termine...\n", g_hilo_servidor_id);
        cethread_join(g_hilo_servidor_id, NULL);
        printf("Hilo del servidor (%d) ha terminado.\n", g_hilo_servidor_id);
        g_hilo_servidor_id = -1;
    }
    cethread_mutex_destroy(&client_socket_mutex); // Destruir el mutex al final
    printf("Servidor detenido.\n");
}

bool server_is_client_connected(void) {
    bool status;
    cethread_mutex_lock(&client_socket_mutex);
    status = connected_client.is_connected;
    cethread_mutex_unlock(&client_socket_mutex);
    return status;
}

int server_send_data_to_client(const char *data, int length) {
    int bytes_sent = -1;
    if (!data || length <= 0) {
        return -1;
    }

    cethread_mutex_lock(&client_socket_mutex);
    if (connected_client.is_connected && connected_client.socket_fd != -1) {
        bytes_sent = send(connected_client.socket_fd, data, length, 0);
        if (bytes_sent < 0) {
            perror("Error al enviar datos al cliente");
            // Aquí podrías considerar que el cliente se desconectó si hay error
            // y marcar connected_client.is_connected = false;
            // close(connected_client.socket_fd);
            // connected_client.socket_fd = -1;
        } else if (bytes_sent < length) {
            printf("Advertencia: No se enviaron todos los datos al cliente (%d de %d bytes)\n", bytes_sent, length);
        } else {
            printf("Datos enviados al cliente (fd: %d): %.*s\n", connected_client.socket_fd, length, data);
        }
    } else {
        // printf("No hay cliente conectado para enviar datos.\n");
        bytes_sent = -2; // Código para indicar "no conectado"
    }
    cethread_mutex_unlock(&client_socket_mutex);
    return bytes_sent;
}