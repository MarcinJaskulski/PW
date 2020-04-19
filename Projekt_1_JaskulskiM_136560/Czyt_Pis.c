#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define N 20 // ilosc procesow
#define K 6 // wielkosc polki
#define dziela 15 // ile dziel ma byc napisanych

struct buf_elem {
   long mtype;
   int mvalue; // zawiera id procesu w kolejce glownej
};

static struct sembuf buf_sem;

void podnies(int semid, int semnum){
   buf_sem.sem_num = semnum;
   buf_sem.sem_op = 1;
   buf_sem.sem_flg = 0;
   if (semop(semid, &buf_sem, 1) == -1){
      perror("Blad: Podnoszenie semafora");
      exit(1);
   }
}

void opusc(int semid, int semnum){
   buf_sem.sem_num = semnum;
   buf_sem.sem_op = -1;
   buf_sem.sem_flg = 0;
   if (semop(semid, &buf_sem, 1) == -1){
      perror("Blad: Opuszczenie semafora");
      exit(1);
   }
}

int main(){
   int mainPid, shmid, shmid_rol, shmid_polki, semid, msgid, i; 
   int nr_p;
   int * buf;
   int * tab_rol;
   int * tab_polki;

   mainPid = getpid();
  
   // --- PAMIEC WSPOLDZIELONA ---
   shmid = shmget(IPC_PRIVATE, 3*sizeof(int), IPC_CREAT|0600);
   if (shmid == -1){
      perror("Blad: Utworzenie segmentu pamieci wspoldzielonej");
      exit(1);
   }
   
   buf = (int*)shmat(shmid, NULL, 0);
   if (buf == NULL){
      perror("Blad: Przylaczenie segmentu pamieci wspoldzielonej");
      exit(1);
   }
   
   buf[0] = 0; // ilosc wyprodukowanych dziel
   buf[1] = 0; // startowa liczba osob w czytelnii
   buf[2] = 0; // liczba dziel na polce

   // ------ Tablica rol ------ każdy proces ma swoją role
   
   shmid_rol = shmget(IPC_PRIVATE, N*sizeof(int), IPC_CREAT|0600);
   if (shmid_rol == -1){
      perror("Blad: Utworzenie segmentu pamieci wspoldzielonej dla tablicy rol");
      exit(1);
   }
   
   tab_rol = (int*)shmat(shmid_rol, NULL, 0);
   if (tab_rol == NULL){
      perror("Blad: Przylaczenie segmentu pamieci wspoldzielonej dla tablicy rol");
      exit(1);
   }

   // ------ Tablica z id kolejek danej półki ------
   
   shmid_polki = shmget(IPC_PRIVATE, K*sizeof(int), IPC_CREAT|0600);
   if (shmid_polki == -1){
      perror("Blad: Utworzenie segmentu pamieci wspoldzielonej polki");
      exit(1);
   }
   
   tab_polki = (int*)shmat(shmid_polki, NULL, 0);
   if (tab_polki == NULL){
      perror("Blad: Przylaczenie segmentu pamieci wspoldzielonej polki");
      exit(1);
   }

   // --- SEMAFORY ---
   semid = semget(IPC_PRIVATE, 2, IPC_CREAT|IPC_EXCL|0600);
   if (semid == -1){
       perror("Blad: Utworzenie tablicy semaforow");
       exit(1);
   }
   if (semctl(semid, 0, SETVAL, (int)1) == -1){ // Semafor Pisania - na początku nie ma pisarza w środku
      perror("Blad: Nadanie wartosci semaforowi 0");
      exit(1);
   }
   if (semctl(semid, 1, SETVAL, (int)1) == -1){ // Semafor Czytania
      perror("Blad: Nadanie wartosci semaforowi 1");
      exit(1);
   }

   // --- KOLEJKI KOMUNIKATOW ---
   msgid = msgget(IPC_PRIVATE, IPC_CREAT|IPC_EXCL|0770); // kolejka dzel
   if (msgid == -1){
      perror("Blad: Utworzenie kolejki dziel");
      exit(1);
   }

   for(i=0; i<K; i++){
      tab_polki[i] = msgget(IPC_PRIVATE, IPC_CREAT|IPC_EXCL|0770); // kolejki polek
      if (tab_polki[i] == -1){
	      perror("Blad: Utworzenie kolejki polek");
	      exit(1);
      }
      printf(" tab_polki[%d]: %2d\n", i, tab_polki[i]);
   }
   
   // --- TWORZENIE N PROCESOW ---	
   for(i=0; i<N; i++){
      if(fork() == 0){
	      nr_p = i;
	      tab_rol[nr_p] = 1; // 0 - pisarz, 1 - czytelnik
	      if(i==0 || i == 1)tab_rol[nr_p] = 0;
	      break;
      }
   }

   // ---Wykonywanie programu --- Proces macierzysty jest pudłem, a w nim są potomnem ktore robia za postaci
   if(mainPid != getpid() ){

      printf("Jestem proces nr. %d. Moja rola startowa: %d \n", nr_p, tab_rol[nr_p]);
      sleep(2);
      
      struct msqid_ds dane; // do pobrania danych o kolejce (ile jest tam procesow)
      struct buf_elem dzielo; // do kolejki glownej
     
      while(buf[0] < dziela){ // dopoki nie wyprodukujemy odpowiedniej ilosci dziel
         // Zmiana roli
         srand(time(0));
         int pom = tab_rol[nr_p];
         if( (int)rand()%20 > 10) tab_rol[nr_p] = 1 - tab_rol[nr_p]; 
         //if(pom != tab_rol[nr_p]) printf("!!!ZMIANA ROLI!!! %d - %d\n", nr_p, tab_rol[nr_p]); 

         // Proces czytelnika
         if(tab_rol[nr_p] == 1 && buf[2] > 0) {  // Jak nie ma czego czytać, to po co wchodzić

            //Mechanizm pozwalajacy wejść czytelnikom
            opusc(semid, 1);
            buf[1]++;
            if (buf[1] == 1) opusc(semid, 0);
            podnies(semid,1);

            //printf("                Proces %2d jest %2d w czytelnii\n", nr_p, buf[1]);
            for(i=0; i<K; i++){ // iteracja po półlkach i szukanie swojego nr_p
               if(msgrcv(tab_polki[i], &dzielo, sizeof(dzielo.mvalue), nr_p, IPC_NOWAIT) != -1){ // sprawdzenie, czy jest cos do czytania i na ktorej polce
                  printf("---Proces %2d czyta z polki nr. %2d dzielo nr. %2d\n", nr_p, i, (int)dzielo.mvalue);
                                    
                  //Sprawdzenie, czy można usunąć z głownej kolejki
                  msgctl(tab_polki[i], IPC_STAT, &dane);

                  if (dane.msg_qnum == 0){ // oznacza, ze nie ma nic juz w kolejce
                     int nr_dziela = dzielo.mvalue;
                     if(msgrcv(msgid, &dzielo, sizeof(dzielo.mvalue), nr_dziela, IPC_NOWAIT) != -1){; // usuniecie dziela z polki o danymi typie! // bo mogą dwa procesy probowac usunac
                        printf("###Dzielo nr. %2d zostało usuniete z polki nr. %d przez proces %2d\n", nr_dziela, i, nr_p);
                        buf[2]--; // zmniejsznie liczby dziel na polce
                     }
                  }

                  break; // tylko jedno dzieło czytamy
               }
            }

            opusc(semid, 1);
            buf[1]--;
            if (buf[1] == 0) podnies(semid, 0);
            podnies(semid,1);
         }

         //Proces pisarza
         if(tab_rol[nr_p] == 0 && (buf[2] < K)){ // jeśli jest jakies dzielo do napisania
            opusc(semid, 0); // oznaczenie, ze pisarz chce wejsc do czytelni
            //printf("                                  Pisarz nr. %d probuje wejsc\n", nr_p); 
            if((buf[1] == 0) && (buf[2] < K) ){ // jeśli nikogo nie ma i jest dzielo do napisania // Moglo sie to zmeinic bo wczesniej orientacyjnie tylko patrzy i wtym czasie, gdy czeka az jego mutex sie zwolni, to ktos inny moze napisac dzielo
               buf[1]++; // zwiekszenie liczby osob w czytelni
               
               int wolna_kol_polki;
               struct msqid_ds dane;
               for(i=0; i<K+1; i++){ // szukanie wolnej polki
                  if(i == K){
                     perror("BLAD: Nie ma wolnej polki");
                     exit(1);
                  }
                  msgctl(tab_polki[i], IPC_STAT, &dane);
                  //printf("                               dane.msg_qnum: %2d - tab_polki[%d]: %2d\n", (int)dane.msg_qnum, i, tab_polki[i]);
                  if(dane.msg_qnum == 0){
                     wolna_kol_polki = i;
                     break;
                  }
               }           
               
               int blokada = 0;
               for(i=0; i<N; i++){ // sprawdzenie kto jest obecnie czytelnikiem - i -> reprezentuje nr procesu
                  if(tab_rol[i] == 1 ){

                     if(blokada == 0){ // jednorazowe dodanie dziala do kolejki glownej, jesli jest chociaz jeden odbiorca
                        blokada = 1;

                        dzielo.mtype = ++buf[0]; // zwiekszenie liczby wyprodukowanych dziel -> koniec petli

                        msgsnd(msgid, &dzielo, sizeof(dzielo.mvalue), 0);
                        printf("Proces %2d dodal dzielo nr. %2d na polke nr. %2d ->", nr_p, (int)dzielo.mtype, wolna_kol_polki);
                        buf[2]++; // zwiekszenie liczby dziel na polce obecnie
                     }
                     dzielo.mvalue = buf[0];
                     dzielo.mtype  = i; // jesli czytelnik, to jego nr trafi do kolejki miejsca na polce
                     msgsnd(tab_polki[wolna_kol_polki], &dzielo, sizeof(dzielo.mvalue), 0); // dodanie czytelnika do kolejki miejsca polki
                     //printf("%2d - tab_polki[%d]: %2d",i, wolna_kol_polki, tab_polki[wolna_kol_polki]);
                     printf("%2d ",i);
                  }
               }
               printf("\n");
               
               buf[1]--; // ilosc osob w czytelni
            }
            //else printf("Nie wszedl\n");
            podnies(semid, 0); // pisarz wychodzi z czytelni
         }
      }
      exit(1);
   }
   
   // --- Czekanie na zakonczenie potomkow ---
   for(i=0; i<N; i++)
	   wait(NULL);
}
