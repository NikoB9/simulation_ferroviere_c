#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>


//horloge 
time_t deb, fin;
long double debut;

/*Renvoie en long le Temps en microseconds */
long double getMicrotime(){
  struct timeval currentTime;
  gettimeofday(&currentTime, NULL);
  return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}



void function(){
		int i;
	for(i=0;i<=60;i++){
		/*time(&fin);
		printf("Debut : %d\n", deb);
		printf("fin : %d\n", fin );
		// secondes = secondes*60*24;
		float diff = difftime(fin, deb);
		printf("%f\n", diff);*/

		long double diff=getMicrotime()-debut;
		printf("%f\n", (double)diff);
		diff=diff/1000000;
		diff= diff*1440;
  //temps reel
		int heure = diff/(60*60);
		diff=diff-heure*60*60;
		int minutes = diff/(60);
		diff=diff-minutes*60;
		int secondes = diff;
		
  // //Conversion
		// heure=heure/(24*60);
		// minutes=minutes/(24*60);
		// secondes=secondes/(60*24*60);

		printf("___%d:%d:%d___\n", heure, minutes, secondes);


		usleep(1000070);
	}
}


int main(int argc, char** argv){


  //la difference en seconde correspond au temps d'execution 
  //nous devons le passer en temps simule 
	
	time(&deb);
	debut=getMicrotime();

	function();
	

}