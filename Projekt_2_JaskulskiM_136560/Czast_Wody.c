//gcc -pthread -o CW Czast_Wody.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define Prod_size_set 20 // ilosc producentow
#define Copmound_size_set 200 // ilosc wyprodukowanych zwiazkow

int H_amount, O_amount;
int Copmound_amount;

pthread_mutex_t prod_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex dla produkcji

pthread_cond_t H_cond = PTHREAD_COND_INITIALIZER; // zatrzymanie gdy H gdy nie ma komplementarnych pierwiastków
pthread_cond_t O_cond = PTHREAD_COND_INITIALIZER; // zatrzymanie gdy O gdy nie ma komplementarnych pierwiastków

int NR;

void *producent(void *arg){
    int nr_w = NR++; // tworzenie id wątku
    while(1==1){//Copmound_amount < Copmound_size_set){
        sleep(1);
        srand(time(0));
        // Producent H
        int  ster;
        if(((int)rand()+1)%30 > 10) ster = 1; // H
        else ster = 0; // O
        
        if(nr_w == 0 || nr_w == 2) ster=1; // Sztuczne wprowadzenie wyprodukowanych pierwiastków
        if(nr_w == 1) ster=0; 

        if(ster){
            pthread_mutex_lock(&prod_mutex);
                H_amount++;

                if(H_amount > 1 && O_amount > 0){ // Jeśli mamy już 2 wodór i 1 tlen
                    printf("### %d H - %d O Produkuje %d Tlen!\n", H_amount, O_amount, Copmound_amount+1);
                    H_amount -= 2;
                    O_amount--;
                    Copmound_amount++;
                    pthread_cond_signal(&H_cond); 
                    pthread_cond_signal(&O_cond); 
                }
                else{ // Jeśli trzeba poczekać na skłądniki
                    printf(" %d H - %d O\n", H_amount, O_amount);
                    pthread_cond_wait(&H_cond, &prod_mutex); // Zarzymanie na warunku istnienia wodoru i pozwala innemu watkowi tu dojść. Odblokuje w przypadku 2 wodorów i 1 tyelnu
                }
            pthread_mutex_unlock(&prod_mutex);
        }
        // Producent O
        else {
            pthread_mutex_lock(&prod_mutex);
                O_amount++;

                if(H_amount > 1 && O_amount > 0){ // Jeśli mamy już 2 wodór i 1 tlen
                    printf("### %d H - %d O Produkuje %d Tlen!\n", H_amount, O_amount, Copmound_amount+1);
                    H_amount -= 2;
                    O_amount--;
                    Copmound_amount++;
                    pthread_cond_signal(&H_cond); 
                    pthread_cond_signal(&H_cond); 
                }
                else{ // Jeśli trzeba poczekać na skłądniki
                    printf(" %d H - %d O\n", H_amount, O_amount);
                    pthread_cond_wait(&O_cond, &prod_mutex); // Zarzymanie na warunku istneinia tylenu i pozwala innemu watkowi tu dojść. Odblokuje w przypadku 2 wodorów i 1 tyelnu
                }
            pthread_mutex_unlock(&prod_mutex);
        }
    }
}

int main(){
    NR = 0;
    H_amount = 0; // wartości początkowe
    O_amount = 0; 
    Copmound_amount = 0;
    pthread_t tab_thread[Prod_size_set];

    for(int i=0; i<Prod_size_set; i++){
        if ( pthread_create( &tab_thread[i], NULL, producent,NULL) ){
            printf("błąd przy tworzeniu wątku %d\n", i); 
            abort(); 
        }
    }

    for(int i=0; i< Prod_size_set; i++){
        pthread_join(tab_thread[i], NULL);
    }
}