#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define MAX_JUGADORES 10 // Máximo número de jugadores permitidos
#define PIPE_LECTURA 0
#define PIPE_ESCRITURA 1

// Estructura para almacenar la información de cada jugador
typedef struct {
    int id;             // Identificador del jugador
    float puntos;       // Puntos acumulados del jugador
    char estado[10];    // Estado del jugador ("jugando", "plantado", "abandonado")
} Jugador;

void repartir_cartas(int pipes_jugadores[][2], int cantidad_jugadores);
void recoger_decisiones(int pipes_jugadores[][2], Jugador jugadores[], int cantidad_jugadores);

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

    int pipes_jugadores[MAX_JUGADORES][2]; // Pipes para comunicación con jugadores
    Jugador jugadores[MAX_JUGADORES];

    // Crear pipes y procesos jugadores
    for (int i = 0; i < cantidad_jugadores; i++) {
        if (pipe(pipes_jugadores[i]) == -1) {
            perror("Error al crear el pipe");
            exit(EXIT_FAILURE);
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("Error al hacer fork");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) { // Proceso hijo (jugador)
            close(pipes_jugadores[i][PIPE_ESCRITURA]);
            srand(time(NULL) ^ (getpid()<<16)); // Seed para aleatoriedad
            float puntos = 0;
            while (1) {
                float carta;
                read(pipes_jugadores[i][PIPE_LECTURA], &carta, sizeof(float));
                puntos += carta;

                if (puntos > 7.5) {
                    write(pipes_jugadores[i][PIPE_ESCRITURA], "abandonado", 10);
                    break;
                }

                // Lógica simple aleatoria para tomar decisiones
                int decision = rand() % 3;
                if (decision == 0) {
                    write(pipes_jugadores[i][PIPE_ESCRITURA], "plantado", 8);
                    break;
                } else if (decision == 1) {
                    write(pipes_jugadores[i][PIPE_ESCRITURA], "jugando", 7);
                } else {
                    write(pipes_jugadores[i][PIPE_ESCRITURA], "abandonado", 10);
                    break;
                }
            }
            close(pipes_jugadores[i][PIPE_LECTURA]);
            exit(0);
        } else { // Proceso padre (servidor)
            close(pipes_jugadores[i][PIPE_LECTURA]);
            jugadores[i].id = i + 1;
            jugadores[i].puntos = 0;
            strcpy(jugadores[i].estado, "jugando");
        }
    }

    // Repartir cartas y recoger decisiones
    repartir_cartas(pipes_jugadores, cantidad_jugadores);
    recoger_decisiones(pipes_jugadores, jugadores, cantidad_jugadores);

    // Esperar a que terminen todos los procesos hijos
    for (int i = 0; i < cantidad_jugadores; i++) {
        if (wait(NULL) == -1) {
            perror("Error en wait");
        }
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

// Función para repartir cartas a los jugadores
void repartir_cartas(int pipes_jugadores[][2], int cantidad_jugadores) {
    for (int ronda = 0; ronda < 3; ronda++) { // Distribuir 3 rondas de cartas
        for (int i = 0; i < cantidad_jugadores; i++) {
            float carta = (rand() % 7) + 1; // Cartas de 1 a 7
            if (rand() % 10 < 3) { // 30% probabilidad de figura (0.5)
                carta = 0.5;
            }
            write(pipes_jugadores[i][PIPE_ESCRITURA], &carta, sizeof(float));
        }
    }
}

// Función para recoger las decisiones de los jugadores
void recoger_decisiones(int pipes_jugadores[][2], Jugador jugadores[], int cantidad_jugadores) {
    for (int i = 0; i < cantidad_jugadores; i++) {
        char decision[10];
        read(pipes_jugadores[i][PIPE_LECTURA], decision, sizeof(decision));
        strcpy(jugadores[i].estado, decision);
        if (strcmp(decision, "jugando") == 0) {
            float carta;
            read(pipes_jugadores[i][PIPE_LECTURA], &carta, sizeof(float));
            jugadores[i].puntos += carta;
        }
    }
}
