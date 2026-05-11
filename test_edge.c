// SPDX-License-Identifier: GPL-3.0
/*
 * test_edge.c
 * Edge cases: carros com 0 assentos, carros enormes, casos exatos, carros de 1 assento.
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
 
void ah(int x) { fprintf(stderr,"timeout!\n"); exit(1); }
 
int main(void) {
    struct station st;
    station_init(&st);
    signal(SIGALRM, ah);
 
    /* Caso 1 */
    printf("Teste 1: carro vazio sem passageiros... ");
    fflush(stdout);
    alarm(2);
    station_load_car(&st, 0);
    alarm(0);
    printf("OK\n");
 
    /* Caso 2 */
    printf("Teste 2: carros com 0 assentos com passageiros esperando... ");
    fflush(stdout);
    for (int i = 0; i < 10; i++) {
        pthread_t t;
        pthread_create(&t, NULL, passenger_thread, &st);
        pthread_detach(t);
    }
    usleep(100000);
    alarm(3);
    for (int i = 0; i < 5; i++) station_load_car(&st, 0);
    alarm(0);
    printf("OK\n");
 
    /* Caso 3 */
    printf("Teste 3: 10 passageiros, carro com 100 assentos... ");
    fflush(stdout);
    alarm(5);
    load_car_returned = 0;
    struct la args = {&st, 100};
    pthread_t lt;
    pthread_create(&lt, NULL, load_thread, &args);
    pthread_detach(lt);
 
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
    if (!load_car_returned) {
        fprintf(stderr, "ERRO: load_car nao retornou com assentos sobrando!\n");
        exit(1);
    }
    alarm(0);
    printf("OK\n");
 
    /* Caso 4 */
    printf("Teste 4: 20 passageiros, carro com exatamente 20 assentos... ");
    fflush(stdout);
    for (int i = 0; i < 20; i++) {
        pthread_t t;
        pthread_create(&t, NULL, passenger_thread, &st);
        pthread_detach(t);
    }
    usleep(200000);
 
    alarm(5);
    load_car_returned = 0;
    struct la a2 = {&st, 20};
    pthread_create(&lt, NULL, load_thread, &a2);
    pthread_detach(lt);
 
    reaped = 0;
    while (reaped < 20) {
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
 
    /* Caso 5 */
    printf("Teste 5: 30 passageiros, 30 carros de 1 assento cada... ");
    fflush(stdout);
    for (int i = 0; i < 30; i++) {
        pthread_t t;
        pthread_create(&t, NULL, passenger_thread, &st);
        pthread_detach(t);
    }
    usleep(200000);
    alarm(15);
    for (int c = 0; c < 30; c++) {
        load_car_returned = 0;
        struct la a3 = {&st, 1};
        pthread_create(&lt, NULL, load_thread, &a3);
        pthread_detach(lt);
 
        while (threads_completed == 0) usleep(100);
        station_on_board(&st);
        __sync_sub_and_fetch(&threads_completed, 1);
 
        for (int i = 0; i < 1000 && !load_car_returned; i++) usleep(1000);
        if (!load_car_returned) { fprintf(stderr,"ERRO carro %d\n", c); exit(1); }
    }
    alarm(0);
    printf("OK\n");
 
    printf("\nTODOS OS EDGE CASES PASSARAM!\n");
    return 0;
}