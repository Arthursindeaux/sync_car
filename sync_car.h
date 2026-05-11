#include <pthread.h>

struct station {
    int passageiro_waiting;
    int free_seats;
    int contador_embarcados;
    pthread_mutex_t mutex;
    pthread_cond_t cond_carro;      
    pthread_cond_t cond_passageiro; 
};

void station_init(struct station *station);
void station_load_car(struct station *station,int assentoVagao);
void station_wait_for_car(struct station *station);
void station_on_board(struct station *station);
