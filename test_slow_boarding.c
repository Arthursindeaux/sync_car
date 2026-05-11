// SPDX-License-Identifier: GPL-3.0
/*
 * test_slow_boarding.c
 *
 * Simula passageiro que demora pra "sentar" (delay entre wait_for_car retornar
 * e on_board ser chamado). Verifica que o carro NAO parte ate todos os
 * station_on_board() esperados terem sido chamados.
 *
 * Bug que isso detecta: implementacoes que sinalizam o carro com base apenas
 * em wait_for_car ter retornado, sem realmente esperar o on_board.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "sync_car.h"
 
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
 
int main(void) {
    struct station st;
    station_init(&st);
    signal(SIGALRM, ah);
    srandom(time(NULL) ^ getpid());
 
    const int rounds = 8;
    const int passengers_per_round = 10;
 
    for (int r = 0; r < rounds; r++) {
        printf("Round %d: %d passageiros com on_board lento... ", r+1, passengers_per_round);
        fflush(stdout);
        alarm(20);
 
        for (int i = 0; i < passengers_per_round; i++) {
            pthread_t t;
            pthread_create(&t, NULL, passenger_thread, &st);
            pthread_detach(t);
        }
        usleep(100000);
 
        load_car_returned = 0;
        struct la args = {&st, passengers_per_round};
        pthread_t lt;
        pthread_create(&lt, NULL, load_thread, &args);
        pthread_detach(lt);
 
        int boarded = 0;
        while (boarded < passengers_per_round) {
            if (load_car_returned) {
                fprintf(stderr, "\nERRO: carro partiu apos %d embarques (de %d) — on_board nao foi respeitado!\n",
                        boarded, passengers_per_round);
                exit(1);
            }
            if (threads_completed > 0) {
                /* delay grande entre wait_for_car retornar e on_board ser chamado.
                 * Simula passageiro real andando ate o assento. */
                usleep(50000 + (random() % 50000));
 
                /* checa novamente: se carro partiu durante o delay, e bug */
                if (load_car_returned) {
                    fprintf(stderr, "\nERRO: carro partiu durante o on_board lento!\n");
                    exit(1);
                }
                boarded++;
                station_on_board(&st);
                __sync_sub_and_fetch(&threads_completed, 1);
            }
        }
 
        for (int i = 0; i < 2000 && !load_car_returned; i++) usleep(1000);
        if (!load_car_returned) {
            fprintf(stderr, "\nERRO: carro nao retornou apos todos embarcarem\n");
            exit(1);
        }
        alarm(0);
        printf("OK\n");
    }
 
    printf("\nSLOW BOARDING OK: carro respeita on_board em todas as rodadas.\n");
    return 0;
}