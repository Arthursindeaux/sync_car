// SPDX-License-Identifier: GPL-3.0
/*
 * test_capacity.c
 *
 * Verifica que o carro NUNCA embarca mais passageiros do que tem assentos,
 * mesmo quando a fila tem muito mais gente esperando.
 *
 * Estrategia: enche a estacao com 100 passageiros, manda carros de capacidade
 * variavel e conta exatamente quantos station_on_board() o carro aceitou antes
 * de partir. Se um unico carro recebeu mais passageiros do que tinha assentos,
 * a implementacao tem bug de overflow.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "sync_car.h"
 
#define TOTAL_PASSENGERS 100
 
volatile int threads_completed = 0;
volatile int load_car_returned = 0;
 
void *passenger_thread(void *arg) {
    station_wait_for_car((struct station *)arg);
    __sync_add_and_fetch(&threads_completed, 1);
    return NULL;
}
 
struct la { struct station *s; int n; };
void *load_thread(void *a) {
    struct la *x = a;
    station_load_car(x->s, x->n);
    load_car_returned = 1;
    return NULL;
}
 
void ah(int x) { fprintf(stderr,"TIMEOUT/DEADLOCK!\n"); exit(1); }
 
#define MIN(a,b) ((a)<(b)?(a):(b))
 
int main(void) {
    struct station st;
    station_init(&st);
    signal(SIGALRM, ah);
    srandom(time(NULL) ^ getpid());
 
    /* 100 passageiros entram na estacao */
    printf("Disparando %d passageiros...\n", TOTAL_PASSENGERS);
    for (int i = 0; i < TOTAL_PASSENGERS; i++) {
        pthread_t t;
        pthread_create(&t, NULL, passenger_thread, &st);
        pthread_detach(t);
    }
    usleep(300000);
 
    int left = TOTAL_PASSENGERS;
    int car_id = 0;
 
    /* sempre que a fila for maior que os assentos, o carro DEVE encher exato */
    while (left > 0) {
        alarm(10);
        int seats = (random() % 15) + 1;   /* 1 a 15 assentos */
        int expected = MIN(left, seats);
 
        load_car_returned = 0;
        struct la args = {&st, seats};
        pthread_t lt;
        pthread_create(&lt, NULL, load_thread, &args);
        pthread_detach(lt);
 
        int boarded = 0;
        /* embarca exatamente "expected" passageiros */
        while (boarded < expected) {
            if (load_car_returned) {
                fprintf(stderr, "ERRO carro %d: partiu cedo (boarded=%d/%d)\n",
                        car_id, boarded, expected);
                exit(1);
            }
            if (threads_completed > 0) {
                boarded++;
                station_on_board(&st);
                __sync_sub_and_fetch(&threads_completed, 1);
            }
        }
 
        /* espera o carro partir */
        for (int i = 0; i < 2000 && !load_car_returned; i++) usleep(1000);
        if (!load_car_returned) {
            fprintf(stderr, "ERRO carro %d: nao partiu\n", car_id);
            exit(1);
        }
 
        /* CRITICO: se threads_completed > 0 aqui, significa que mais passageiros
         * "embarcaram" (station_wait_for_car retornou) depois que o carro ja deveria
         * estar cheio. Isso e bug de capacidade. */
        usleep(50000);
        if (threads_completed > 0) {
            fprintf(stderr, "ERRO carro %d: %d passageiros extras embarcaram alem dos %d assentos!\n",
                    car_id, threads_completed, seats);
            exit(1);
        }
 
        left -= boarded;
        car_id++;
        if (car_id % 10 == 0)
            printf("  carro %d, restam %d passageiros\n", car_id, left);
    }
 
    alarm(0);
    printf("\nCAPACIDADE OK: %d carros, capacidade sempre respeitada.\n", car_id);
    return 0;
}