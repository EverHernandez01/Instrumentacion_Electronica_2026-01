#include <stdio.h>
#include <stdlib.h>

#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUFFER_SIZE 50
#define MAX_INTENTOS 3

/* =========================================================
   VARIABLES GLOBALES
   ========================================================= */

/* 
 * Máquina de estados:
 * 0 -> Ingresar mínimo
 * 1 -> Ingresar máximo
 * 2 -> Juego iniciado
 */
int estadoJuego = 0;

/* Límites del rango */
long valorMin = 0;
long valorMax = 0;

/* Número secreto */
long numeroSecreto = 0;

/* Intentos restantes */
int intentosRestantes = MAX_INTENTOS;


/* =========================================================
   FUNCIÓN PRINCIPAL
   ========================================================= */
void app_main(void)
{
    char buffer[BUFFER_SIZE];
    long entrada = 0;

    printf("\n====================================\n");
    printf("   JUEGO: ADIVINA EL NUMERO\n");
    printf("====================================\n");

    printf("Comunicacion UART usando monitor serial\n");
    printf("Ingrese el valor MINIMO del rango:\n");

    while (1)
    {
        /*
         * Leer entrada desde UART.
         */
        if (fgets(buffer, BUFFER_SIZE, stdin) != NULL)
        {
            /*
             * Convertir texto a número entero.
             */
            entrada = atol(buffer);

            /* =================================================
               MÁQUINA DE ESTADOS
               ================================================= */
            switch (estadoJuego)
            {

                /* =============================================
                   ESTADO 0:
                   INGRESAR VALOR MÍNIMO
                   ============================================= */
                case 0:

                    valorMin = entrada;

                    printf("Valor minimo establecido en: %ld\n",
                           valorMin);

                    printf("Ahora ingrese el valor MAXIMO del rango:\n");

                    estadoJuego = 1;

                    break;


                /* =============================================
                   ESTADO 1:
                   INGRESAR VALOR MÁXIMO
                   ============================================= */
                case 1:

                    valorMax = entrada;

                    /*
                     * Validar rango.
                     */
                    if (valorMax <= valorMin)
                    {
                        printf("ERROR:\n");
                        printf("El valor maximo debe ser mayor al minimo.\n");
                        printf("Ingrese nuevamente el valor MAXIMO:\n");
                    }
                    else
                    {
                        printf("Valor maximo establecido en: %ld\n",
                               valorMax);

                        /*
                         * Generar número aleatorio.
                         */
                        numeroSecreto =
                            valorMin +
                            (esp_random() %
                            (valorMax - valorMin + 1));

                        /*
                         * Reiniciar intentos.
                         */
                        intentosRestantes = MAX_INTENTOS;

                        printf("\n====================================\n");
                        printf("JUEGO INICIADO\n");
                        printf("====================================\n");

                        printf("Adivina el numero entre %ld y %ld\n",
                               valorMin,
                               valorMax);

                        printf("Intentos disponibles: %d\n",
                               intentosRestantes);

                        estadoJuego = 2;
                    }

                    break;


                /* =============================================
                   ESTADO 2:
                   FASE DE JUEGO
                   ============================================= */
                case 2:

                    printf("\nTu intento: %ld\n", entrada);

                    /*
                     * Comparar intento con el número secreto.
                     */
                    if (entrada > numeroSecreto)
                    {
                        printf("El numero secreto es MENOR.\n");

                        intentosRestantes--;
                    }
                    else if (entrada < numeroSecreto)
                    {
                        printf("El numero secreto es MAYOR.\n");

                        intentosRestantes--;
                    }
                    else
                    {
                        /*
                         * El usuario ganó.
                         */
                        printf("\n====================================\n");
                        printf("EXCELENTE. Adivinaste el numero secreto: %ld\n",
                               numeroSecreto);

                        printf("====================================\n");

                        printf("Vamos a jugar de nuevo.\n");
                        printf("Ingrese un nuevo valor MINIMO:\n");

                        estadoJuego = 0;

                        break;
                    }

                    /*
                     * Mostrar intentos restantes.
                     */
                    printf("Intentos restantes: %d\n",
                           intentosRestantes);

                    /*
                     * Verificar si perdió.
                     */
                    if (intentosRestantes <= 0)
                    {
                        printf("\n====================================\n");
                        printf("GAME OVER\n");
                        printf("El numero secreto era: %ld\n",
                               numeroSecreto);

                        printf("====================================\n");

                        printf("Vamos a jugar de nuevo.\n");
                        printf("Ingrese un nuevo valor MINIMO:\n");

                        estadoJuego = 0;
                    }

                    break;
            }
        }

        /*
         * Pequeño retardo para evitar
         * uso excesivo de CPU.
         */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}