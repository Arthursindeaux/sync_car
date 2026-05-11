// SPDX-License-Identifier: GPL-3.0
/*
 * test_late_arrivals.c (corrigido)
 *
 * Cenarios validos pela spec do CalTrain:
 *   - Se NAO ha passageiros esperando quando o carro chega, ele parte imediato.
 *   - Se passageiros chegam APOS o carro estar parado (e ja ha outros esperando),
 *     o carro continua esperando ate encher ou a fila esvaziar.
 *
 * O cenario "carro chega antes de qualquer passageiro" foi removido,
 * porque a spec exige que o carro parta IMEDIATAMENTE nesse caso.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
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
 
void ah(int x) { fprintf(stderr,"DEADLOCK!\n"); exit(1); }
 
int main(void) {
    struct station st;
    station_init(&st);
    signal(SIGALRM, ah);
 
    /* Cenario A: carro chega sem passageiros, deve partir imediato */
    printf("Teste A: carro sem passageiros parte imediato... ");
    fflush(stdout);
    alarm(2);
    station_load_car(&st, 5);  /* deve retornar imediato */
    alarm(0);
    printf("OK\n");
 
    /* Cenario B: 3 passageiros + carro de 10, mais 7 chegam durante load */
    printf("Teste B: passageiros chegam aos poucos durante load... ");
    fflush(stdout);
    alarm(15);
 
    /* primeiro 3 passageiros */
    for (int i = 0; i < 3; i++) {
        pthread_t t;
        pthread_create(&t, NULL, passenger_thread, &st);
        pthread_detach(t);
    }
    usleep(100000);
 
    load_car_returned = 0;
    struct la b_args = {&st, 10};
    pthread_t lt;
    pthread_create(&lt, NULL, load_thread, &b_args);
    pthread_detach(lt);
 
    usleep(50000);
 
    /* mais 4 passageiros enquanto carro espera */
    for (int i = 0; i < 4; i++) {
        pthread_t t;
        pthread_create(&t, NULL, passenger_thread, &st);
        pthread_detach(t);
        usleep(10000);
    }
 
    usleep(50000);
 
    /* mais 3 passageiros — total: 10 */
    for (int i = 0; i < 3; i++) {
        pthread_t t;
        pthread_create(&t, NULL, passenger_thread, &st);
        pthread_detach(t);
        usleep(10000);
    }
 
    int reaped = 0;
    while (reaped < 10) {
        if (load_car_returned) {
            fprintf(stderr, "ERRO: load_car partiu antes dos 10!\n");
            exit(1);
        }
        if (threads_completed > 0) {
            reaped++;
            station_on_board(&st);
            __sync_sub_and_fetch(&threads_completed, 1);
        }
    }
    for (int i = 0; i < 1000 && !load_car_returned; i++) usleep(1000);
    if (!load_car_returned) { fprintf(stderr,"ERRO\n"); exit(1); }
    alarm(0);
    printf("OK\n");
 
    /* Cenario C: 5 passageiros + carro de 20, deve partir quando fila esvaziar */
    printf("Teste C: 5 passageiros, carro de 20, parte com fila vazia... ");
    fflush(stdout);
    alarm(10);
 
    for (int i = 0; i < 5; i++) {
        pthread_t t;
        pthread_create(&t, NULL, passenger_thread, &st);
        pthread_detach(t);
    }
    usleep(100000);
 
    load_car_returned = 0;
    struct la c_args = {&st, 20};
    pthread_create(&lt, NULL, load_thread, &c_args);
    pthread_detach(lt);
 
    reaped = 0;
    while (reaped < 5) {
        if (load_car_returned) {
            fprintf(stderr, "ERRO: load_car partiu antes dos 5!\n");
            exit(1);
        }
        if (threads_completed > 0) {
            reaped++;
            station_on_board(&st);
            __sync_sub_and_fetch(&threads_completed, 1);
        }
    }
    /* fila esvaziou — carro deve partir mesmo com 15 assentos vazios */
    for (int i = 0; i < 2000 && !load_car_returned; i++) usleep(1000);
    if (!load_car_returned) {
        fprintf(stderr, "ERRO: load_car NAO retornou com fila vazia!\n");
        exit(1);
    }
    alarm(0);
    printf("OK\n");
 
    printf("\nTESTES DE CHEGADA ATRASADA PASSARAM!\n");
    return 0;
}