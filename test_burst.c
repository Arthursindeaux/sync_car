// SPDX-License-Identifier: GPL-3.0
/*
 * test_burst.c
 *
 * Burst test: usa pthread_barrier_t pra liberar TODOS os passageiros de um lote
 * + o carro no MESMO instante. Maximiza chance de expor races entre
 * station_wait_for_car() e station_load_car().
 *
 * Estrategia: a cada ciclo, o carro tem capacidade >= burst, entao todos sobem
 * e o carro parte com fila vazia. Sem sobras entre ciclos.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "sync_car.h"
 
#define BURST_SIZE 50
#define CYCLES 15
 
volatile int threads_completed = 0;
volatile int load_car_returned = 0;
pthread_barrier_t gate;
 
void *passenger_thread(void *arg) {
    pthread_barrier_wait(&gate);
    station_wait_for_car((struct station *)arg);
    __sync_add_and_fetch(&threads_completed, 1);
    return NULL;
}
 
struct la { struct station *s; int n; };
void *load_thread(void *a) {
    struct la *x = a;
    pthread_barrier_wait(&gate);
    station_load_car(x->s, x->n);
    load_car_returned = 1;
    return NULL;
}
 
void ah(int x) { fprintf(stderr,"TIMEOUT/DEADLOCK!\n"); exit(1); }
 
int main(void) {
    struct station st;
    station_init(&st);
    signal(SIGALRM, ah);
    srandom(time(NULL) ^ getpid());
 
    for (int cycle = 0; cycle < CYCLES; cycle++) {
        int seats = BURST_SIZE + (random() % 20);  /* sempre >= BURST_SIZE */
 
        printf("Ciclo %d: %d passageiros + carro de %d assentos simultaneos... ",
               cycle+1, BURST_SIZE, seats);
        fflush(stdout);
 
        alarm(15);
        pthread_barrier_init(&gate, NULL, BURST_SIZE + 1);
 
        load_car_returned = 0;
        struct la args = {&st, seats};
 
        for (int i = 0; i < BURST_SIZE; i++) {
            pthread_t t;
            pthread_create(&t, NULL, passenger_thread, &st);
            pthread_detach(t);
        }
        pthread_t lt;
        pthread_create(&lt, NULL, load_thread, &args);
        pthread_detach(lt);
 
        int boarded = 0;
        while (boarded < BURST_SIZE) {
            if (load_car_returned) {
                fprintf(stderr, "\nERRO: carro partiu com %d/%d embarques\n",
                        boarded, BURST_SIZE);
                exit(1);
            }
            if (threads_completed > 0) {
                boarded++;
                station_on_board(&st);
                __sync_sub_and_fetch(&threads_completed, 1);
            }
        }
 
        for (int i = 0; i < 2000 && !load_car_returned; i++) usleep(1000);
        if (!load_car_returned) {
            fprintf(stderr, "\nERRO: carro nao retornou apos fila esvaziar\n");
            exit(1);
        }
 
        pthread_barrier_destroy(&gate);
        alarm(0);
        printf("OK\n");
    }
 
    printf("\nBURST OK: %d ciclos simultaneos sem race.\n", CYCLES);
    return 0;
}