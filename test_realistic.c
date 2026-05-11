// SPDX-License-Identifier: GPL-3.0
/*
 * test_realistic.c
 * Simulacao mais realista: passageiros e carros chegam em momentos imprevisiveis,
 * com delays variaveis em cada operacao. Roda em loop longo procurando bugs raros.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "sync_car.h"
 
#define TOTAL_PASSENGERS 500
#define MAX_SEATS 25
 
volatile int threads_completed = 0;
volatile int load_car_returned = 0;
 
void *passenger_thread(void *arg) {
    /* delay aleatorio antes de chegar */
    usleep((random() % 5000));
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
#define MIN(a,b) ((a)<(b)?(a):(b))
 
int main(void) {
    struct station st;
    station_init(&st);
    signal(SIGALRM, ah);
    srandom(time(NULL) ^ getpid());
 
    printf("Iniciando simulacao realista com %d passageiros...\n", TOTAL_PASSENGERS);
 
    /* dispara passageiros em lotes irregulares */
    int sent = 0;
    while (sent < TOTAL_PASSENGERS) {
        int batch = (random() % 20) + 1;
        if (sent + batch > TOTAL_PASSENGERS) batch = TOTAL_PASSENGERS - sent;
        for (int i = 0; i < batch; i++) {
            pthread_t t;
            pthread_create(&t, NULL, passenger_thread, &st);
            pthread_detach(t);
        }
        sent += batch;
        usleep((random() % 3000));
    }
 
    int passengers_left = TOTAL_PASSENGERS;
    int total_boarded = 0;
    int car_count = 0;
 
    while (passengers_left > 0) {
        alarm(15);
 
        /* delay aleatorio antes do carro chegar */
        usleep((random() % 5000));
 
        int seats = (random() % MAX_SEATS) + 1;
        load_car_returned = 0;
        struct la args = {&st, seats};
        pthread_t lt;
        pthread_create(&lt, NULL, load_thread, &args);
        pthread_detach(lt);
 
        int to_reap = MIN(passengers_left, seats);
        int reaped = 0;
 
        while (reaped < to_reap) {
            if (load_car_returned) {
                fprintf(stderr, "ERRO: carro %d partiu antes!\n", car_count);
                exit(1);
            }
            if (threads_completed > 0) {
                /* delay aleatorio entre detectar e chamar on_board */
                if (random() % 3 == 0) usleep(random() % 100);
                reaped++;
                station_on_board(&st);
                __sync_sub_and_fetch(&threads_completed, 1);
            }
        }
 
        for (int i = 0; i < 2000 && !load_car_returned; i++) usleep(1000);
        if (!load_car_returned) {
            fprintf(stderr, "ERRO: carro %d nao retornou\n", car_count);
            exit(1);
        }
 
        while (threads_completed > 0) {
            reaped++;
            __sync_sub_and_fetch(&threads_completed, 1);
        }
 
        if (reaped != to_reap) {
            fprintf(stderr, "ERRO carro %d: esperado %d, obtido %d\n",
                    car_count, to_reap, reaped);
            exit(1);
        }
 
        passengers_left -= reaped;
        total_boarded += reaped;
        car_count++;
        if (car_count % 10 == 0)
            printf("  ... %d carros, %d passageiros embarcados (%d restantes)\n",
                   car_count, total_boarded, passengers_left);
    }
 
    alarm(0);
    printf("\nSIMULACAO REALISTA OK: %d carros, %d passageiros\n",
           car_count, total_boarded);
    return 0;
}