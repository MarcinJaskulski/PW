#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define MAX 4


static struct sembuf buf;

void podnies(int semid, int semnum){
   buf.sem_num = semnum;
   buf.sem_op = 1;
   buf.sem_flg = 0;
   if (semop(semid, &buf, 1) == -1){
      perror("Podnoszenie semafora");
      exit(1);
   }
}

void opusc(int semid, int semnum){
   buf.sem_num = semnum;
   buf.sem_op = -1;
   buf.sem_flg = 0;
   if (semop(semid, &buf, 1) == -1){
      perror("Opuszczenie semafora");
      exit(1);
   }
}


main(){
   int shmid, semid, i;
   int *buf;
   
   semid = semget(45281, 2, IPC_CREAT|0600);
   if (semid == -1){
      perror("Utworzenie tablicy semaforow");
      exit(1);
   }
   if (semctl(semid, 0, SETVAL, (int)MAX) == -1){
      perror("Nadanie wartosci semaforowi 0");
      exit(1);
   }
   if (semctl(semid, 1, SETVAL, (int)0) == -1){
      perror("Nadanie wartosci semaforowi 1");
      exit(1);
   }
   
   shmid = shmget(45281, MAX*sizeof(int), IPC_CREAT|0600);
   if (shmid == -1){
      perror("Utworzenie segmentu pamieci wspoldzielonej");
      exit(1);
   }
   
   buf = (int*)shmat(shmid, NULL, 0);
   if (buf == NULL){
      perror("Przylaczenie segmentu pamieci wspoldzielonej");
      exit(1);
   }
   if(fork()==0){
      for (i=0; i<10; i++){
         opusc(semid, 0);
         buf[i%MAX] = i;
         printf("Prod - Numer: %5d   Wartosc: %5d\n", i%MAX, buf[i%MAX]);
         podnies(semid, 1);
      }
   }
   else{
      for (i=0; i<10; i++){
         opusc(semid, 1);
         printf("   Kons - Numer: %5d   Wartosc: %5d\n", i%MAX, buf[i%MAX]);
         podnies(semid, 0);
      }

   }
}
