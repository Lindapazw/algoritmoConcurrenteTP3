#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_JUGADORES 10 // Máximo número de jugadores permitidos
#define PORT 8080

// Estructura para almacenar la información de cada jugador
typedef struct {
    int id;             // Identificador del jugador
    float puntos;       // Puntos acumulados del jugador
    char estado[20];    // Estado del jugador ("jugando", "plantado", "abandonado")
} Jugador;

void manejar_jugador(int socket_cliente, int id_jugador);
void repartir_cartas(int sockets_jugadores[], int cantidad_jugadores, Jugador jugadores[]);
void recoger_decisiones(int sockets_jugadores[], Jugador jugadores[], int cantidad_jugadores);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s N\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int cantidad_jugadores = atoi(argv[1]);
    if (cantidad_jugadores > MAX_JUGADORES) {
        fprintf(stderr, "El número máximo de jugadores es %d\n", MAX_JUGADORES);
        exit(EXIT_FAILURE);
    }

    int servidor_fd, nuevo_socket;
    struct sockaddr_in direccion_servidor, direccion_cliente;
    int opt = 1;
    int addrlen = sizeof(direccion_cliente);

    if ((servidor_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(servidor_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Error en setsockopt");
        exit(EXIT_FAILURE);
    }

    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(PORT);

    if (bind(servidor_fd, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        perror("Error en bind");
        exit(EXIT_FAILURE);
    }

    if (listen(servidor_fd, cantidad_jugadores) < 0) {
        perror("Error en listen");
        exit(EXIT_FAILURE);
    }

    int sockets_jugadores[MAX_JUGADORES];
    Jugador jugadores[MAX_JUGADORES];

    // Aceptar conexiones de los jugadores
    for (int i = 0; i < cantidad_jugadores; i++) {
        if ((nuevo_socket = accept(servidor_fd, (struct sockaddr *)&direccion_cliente, (socklen_t *)&addrlen)) < 0) {
            perror("Error en accept");
            exit(EXIT_FAILURE);
        }

        sockets_jugadores[i] = nuevo_socket;
        jugadores[i].id = i + 1;
        jugadores[i].puntos = 0;
        strcpy(jugadores[i].estado, "jugando");

        pid_t pid = fork();
        if (pid == 0) { // Proceso hijo
            close(servidor_fd);
            manejar_jugador(nuevo_socket, jugadores[i].id);
            exit(0);
        }
    }

    // Repartir cartas y recoger decisiones
    repartir_cartas(sockets_jugadores, cantidad_jugadores, jugadores);
    recoger_decisiones(sockets_jugadores, jugadores, cantidad_jugadores);

    // Esperar a que terminen todos los procesos hijos
    for (int i = 0; i < cantidad_jugadores; i++) {
        wait(NULL);
    }

    // Decidir y mostrar el ganador
    int ganador = -1;
    float max_puntos = 0;
    for (int i = 0; i < cantidad_jugadores; i++) {
        printf("Jugador %d: Puntos = %.1f, Estado = %s\n", jugadores[i].id, jugadores[i].puntos, jugadores[i].estado);
        if (jugadores[i].puntos <= 7.5 && jugadores[i].puntos > max_puntos && strcmp(jugadores[i].estado, "plantado") == 0) {
            max_puntos = jugadores[i].puntos;
            ganador = jugadores[i].id;
        }
    }

    if (ganador != -1) {
        printf("El ganador es el Jugador %d con %.1f puntos\n", ganador, max_puntos);
    } else {
        printf("No hay ganador\n");
    }

    return 0;
}

void manejar_jugador(int socket_cliente, int id_jugador) {
    srand(time(NULL) ^ (getpid() << 16)); // Seed para aleatoriedad
    float puntos = 0;
    while (1) {
        float carta;
        if (recv(socket_cliente, &carta, sizeof(float), 0) <= 0) {
            perror("Error al leer carta");
            exit(EXIT_FAILURE);
        }
        puntos += carta;

        printf("Jugador %d recibió una carta de valor %.1f. Puntos actuales: %.1f\n", id_jugador, carta, puntos);

        if (puntos > 7.5) {
            if (send(socket_cliente, "abandonado", 10, 0) == -1) {
                perror("Error al enviar estado");
                exit(EXIT_FAILURE);
            }
            break;
        }

        // Lógica simple aleatoria para tomar decisiones
        int decision = rand() % 3;
        if (decision == 0) {
            if (send(socket_cliente, "plantado", 8, 0) == -1) {
                perror("Error al enviar estado");
                exit(EXIT_FAILURE);
            }
            break;
        } else if (decision == 1) {
            if (send(socket_cliente, "jugando", 7, 0) == -1) {
                perror("Error al enviar estado");
                exit(EXIT_FAILURE);
            }
        } else {
            if (send(socket_cliente, "abandonado", 10, 0) == -1) {
                perror("Error al enviar estado");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }
}

void repartir_cartas(int sockets_jugadores[], int cantidad_jugadores, Jugador jugadores[]) {
    for (int ronda = 0; ronda < 3; ronda++) { // Distribuir 3 rondas de cartas
        for (int i = 0; i < cantidad_jugadores; i++) {
            float carta = (rand() % 7) + 1; // Cartas de 1 a 7
            if (rand() % 10 < 3) { // 30% probabilidad de figura (0.5)
                carta = 0.5;
            }
            printf("Repartiendo carta de valor %.1f al Jugador %d\n", carta, i + 1);
            if (send(sockets_jugadores[i], &carta, sizeof(float), 0) == -1) {
                perror("Error al repartir carta");
            }
        }
    }
}

void recoger_decisiones(int sockets_jugadores[], Jugador jugadores[], int cantidad_jugadores) {
    for (int i = 0; i < cantidad_jugadores; i++) {
        char decision[20] = {0};
        if (recv(sockets_jugadores[i], decision, sizeof(decision) - 1, 0) > 0) {
            decision[sizeof(decision) - 1] = '\0'; // Asegurar el fin de cadena
            strcpy(jugadores[i].estado, decision);
            printf("Jugador %d decidió: %s\n", jugadores[i].id, jugadores[i].estado);
            if (strcmp(decision, "jugando") == 0) {
                float carta;
                if (recv(sockets_jugadores[i], &carta, sizeof(float), 0) > 0) {
                    jugadores[i].puntos += carta;
                    printf("Jugador %d recibió una carta de valor %.1f. Puntos actuales: %.1f\n", jugadores[i].id, carta, jugadores[i].puntos);
                }
            }
        } else {
            printf("Error leyendo decisión del jugador %d\n", jugadores[i].id);
        }
    }
}
