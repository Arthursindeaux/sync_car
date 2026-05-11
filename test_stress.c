// SPDX-License-Identifier: GPL-3.0
/*
 * test_stress.c
 * Stress test: 5 rodadas, 1000 passageiros por rodada, carros pequenos.
 * Detecta vazamentos de estado entre carros e race conditions sob alta carga.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "sync_car.h"
 
#define TOTAL_PASSENGERS 1000
#define MAX_SEATS 30
#define ROUNDS 5
 
volatile int threads_completed = 0;
volatile int load_car_returned = 0;
 
void *passenger_thread(void *arg) {
    struct station *s = (struct station *) arg;
    station_wait_for_car(s);
    __sync_add_and_fetch(&threads_completed, 1);
    return NULL;
}
 
struct load_args { struct station *s; int seats; };
 
void *load_car_thread(void *args) {
    struct load_args *a = (struct load_args *) args;
    station_load_car(a->s, a->seats);
    load_car_returned = 1;
    return NULL;
}
 
void alarm_handler(int x) {
    fprintf(stderr, "DEADLOCK detectado!\n");
    exit(1);
}
 
#define MIN(a,b) ((a)<(b)?(a):(b))
 
int main(void) {
    struct station station;
    station_init(&station);
    signal(SIGALRM, alarm_handler);
    srandom(time(NULL) ^ getpid());
 
    for (int round = 0; round < ROUNDS; round++) {
        printf("=== Round %d ===\n", round + 1);
        int passengers_left = TOTAL_PASSENGERS;
 
        for (int i = 0; i < TOTAL_PASSENGERS; i++) {
            pthread_t tid;
            pthread_create(&tid, NULL, passenger_thread, &station);
            pthread_detach(tid);
        }
 
        int boarded = 0;
        while (passengers_left > 0) {
            alarm(10);
            int seats = (random() % MAX_SEATS) + 1;
            load_car_returned = 0;
            struct load_args args = {&station, seats};
            pthread_t lt;
            pthread_create(&lt, NULL, load_car_thread, &args);
            pthread_detach(lt);
 
            int to_reap = MIN(passengers_left, seats);
            int reaped = 0;
            while (reaped < to_reap) {
                if (load_car_returned) {
                    fprintf(stderr, "ERRO: load_car retornou cedo!\n");
                    exit(1);
                }
                if (threads_completed > 0) {
                    reaped++;
                    station_on_board(&station);
                    __sync_sub_and_fetch(&threads_completed, 1);
                }
            }
 
            for (int i = 0; i < 1000 && !load_car_returned; i++)
                usleep(1000);
 
            if (!load_car_returned) {
                fprintf(stderr, "ERRO: load_car nao retornou\n");
                exit(1);
            }
 
            while (threads_completed > 0) {
                reaped++;
                __sync_sub_and_fetch(&threads_completed, 1);
            }
 
            if (reaped != to_reap) {
                fprintf(stderr, "ERRO: esperado %d, embarcaram %d\n", to_reap, reaped);
                exit(1);
            }
            passengers_left -= reaped;
            boarded += reaped;
        }
        printf("Round %d OK: %d passageiros embarcados\n", round + 1, boarded);
    }
    printf("\nSTRESS TEST PASSOU!\n");
    return 0;
}