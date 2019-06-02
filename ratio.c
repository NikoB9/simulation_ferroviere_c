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


int main(int argc, char** argv){


  //la difference en seconde correspond au temps d'execution 
  //nous devons le passer en temps simule 
	
	time(&deb);
	int i;
	for(i=0;i<=60;i++){
		time(&fin);
		// secondes = secondes*60*24;
		double diff = difftime(fin, deb);
		printf("%f\n", diff);

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


		usleep(1000000);
	}
	

}