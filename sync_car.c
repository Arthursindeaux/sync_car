#include "sync_car.h"

void station_init(struct station *station){
    station->passageiro_waiting = 0;
    station->free_seats = 0;
    station->contador_embarcados = 0;
    pthread_mutex_init(&station->mutex,NULL);
    pthread_cond_init(&station->cond_carro, NULL);
    pthread_cond_init(&station->cond_passageiro, NULL);
}

void station_load_car(struct station *station, int count){
    pthread_mutex_lock(&station->mutex);

    station->free_seats = count;
    station->contador_embarcados = 0;

    while (station->free_seats > 0 && station->passageiro_waiting > 0) {
        pthread_cond_broadcast(&station->cond_passageiro);
        pthread_cond_wait(&station->cond_carro, &station->mutex);
    }

    int sentados = count - station->free_seats;
    while (station->contador_embarcados < sentados) {
        pthread_cond_wait(&station->cond_carro, &station->mutex);
    }

    station->free_seats = 0;
    pthread_mutex_unlock(&station->mutex);
}

void station_wait_for_car(struct station *station){
    pthread_mutex_lock(&station->mutex);
    station->passageiro_waiting++;

    while(station->free_seats == 0){
        pthread_cond_wait(&station->cond_passageiro, &station->mutex);
    }
    
    station->free_seats--;
    station->passageiro_waiting--;   
    pthread_cond_signal(&station->cond_carro); 
    pthread_mutex_unlock(&station->mutex);
}

void station_on_board(struct station *station){

    pthread_mutex_lock(&station->mutex);
    
    station->contador_embarcados ++;
    pthread_cond_signal(&station->cond_carro);

   pthread_mutex_unlock(&station->mutex);
}

