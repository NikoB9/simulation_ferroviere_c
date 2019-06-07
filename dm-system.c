#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

//temps de pause
//1 minutes par personne
#define PASSAGE_GUICHET 1.
//1 minutes par personne
#define TEMPS_MONTEE_TRAIN 1.
//10 minutes
#define TRAIN_EN_GARE 10.
//Pour simplifier 1minute de simulation équivaut à 24h de la vrai vie
//donc 1 minutes simulée vaut 1/(24*60) minute de la vie réelle
#define RATIO_MINUTE 1./(24*60)

typedef struct train train;
struct train
{
  char *numero;
  int nbPassagers;
  int nombrePlaces;
  int gareActuelle;
  //1 si trajet alle 0 si retour
  int alle;
};

typedef struct gare gare;
struct gare
{
  char *nom;
  double recette;
  int nbVoyageurs;
  int nombreGuichets;
};

typedef struct ligne ligne;
struct ligne
{
  char *nom;
  train trains[5];
  gare gares[];
};


typedef struct voyageur voyageur;
struct voyageur
{
  int identifiantClient;
  gare derniereGareDePassage;
  gare garesDePassage[];
};


//Sémaphores pour limiter les accès
//les montées descentes de trains, les guichets, les tunnels...
sem_t semaphoreGuichet;

/*time_t debut, fin;
clock_t start, finish;
*/ 

//horloge
double duration;
double debutMicrosecondes;


/*************************************************************
**  CREATION DE LA MAP
**************************************************************/

//Tableau de noms de Gares 
char *nomGare[] =
{
  "Saint-Lazare",
  "Montparnasse",
  "Gare du Nord",
  "Massy Palaiseau",
  "Lieusaint",
  "Lyon Part-dieu",
  "Nantes",
  "Angers",
  "Tours",
  "Marseille",
  "Montpellier",
  "Roissy",
  "Nice",
  "Bordeaux",
  "Caen",
  "Rouen",
  "Vannes",
  "Brest",
  "Strasbourg",
  "Avignon",
  "Adainville",
  "Saint-Rémy-Les-Chebreuses",
  "Saint-Brieuc",
  "La Rochelle",
  "Rennes",
  "Gap"
};

 
/*Renvoie en long le Temps en microseconds */
long double getMicrotime(){
  struct timeval currentTime;
  gettimeofday(&currentTime, NULL);
  return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}


//on ramène donc en microseconde pour nos besoin en divisant par 10^-6
/*************************************************************
** RATIO
**Inputs : nombre de minutes de la vie réelle (float)
**Ouputs : nombre de microseconde pour la simulation (float)
**************************************************************/
double ratioMinsEnMs(double nbMinutes){

  return nbMinutes * RATIO_MINUTE * 60 * 1000000;

}

/*************************************************************
** affichage du temps  
**Inputs : nombre de secondes (float)
**Ouputs : Temps affiché (char*)
**************************************************************/
void afficheTemps(double diff){

  //la difference en seconde correspond au temps d'execution 
  //nous devons le passer en temps simule 

  /*printf("%f\n", (double) start/CLOCKS_PER_SEC);
  printf("%f\n", (double) finish/CLOCKS_PER_SEC);
  printf("%f\n", (double) diff);*/

  diff=diff*1440;
  int heure = diff/(60*60);

  if(heure==24){
    exit(0);
  }

  diff=diff-heure*60*60;
  int minutes = diff/(60);
  diff=diff-minutes*60;
  int secondes = diff;

  time_t dateJour;
  time(&dateJour);
  struct tm tm = *localtime(&dateJour);

  printf("___%d/%d/%d__:__%d:%d:%d___\n", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, heure, minutes, (int) secondes);
}

/*************************************************************
**Génère proprement un nombre aléatoire dans un interval
**Inputs : debut intervalle (float), fin intervalle (float)
**Ouputs : nombre aléatoire (float)
**************************************************************/
double nbAleatoire(double deb, double fin){

  return (rand()/(double)RAND_MAX ) * (fin-deb) + deb;

}

/*************************************************************
**Génère un char contenant le numéro aléatoire d'un train 
**Inputs : 
**Ouputs : nombre aléatoire entier sous forme de chaine de caractère de taille 6
**************************************************************/
char randomNomTrainGlobal[6];
void numTrainRandom(){
  int i =0;
  for (int i = 0; i < 6; ++i)
  {
    // (char)rand()%9 => une valeur en code ascii compris entre 0 et 9 entière
    char tempChar = ('0'+(char)(rand()%9));
    randomNomTrainGlobal[i]=tempChar;
  }
}

 
/*************************************************************
** file d'attente des passagers à la billeterie
**************************************************************/
void*  payerBillet (void* infos) {

  //récupère nom de la gare
  //char* inf = *((char**) infos);
  char** info = (char**) infos;

  char* numclient = info[1];
  //a determiner en fonction de l'intervale de prix attribué à la gare
  int prixBillet = nbAleatoire(0,3000);
  
  printf("Le client n°%s fait la queue au guichet de la gare %s\n", numclient, info[0]);

  //Opération prendre la ressource sur le sémaphore
  sem_wait(&semaphoreGuichet);
  printf("Le client n°%s paye son billet %d€\n", numclient, prixBillet);
    usleep(ratioMinsEnMs(PASSAGE_GUICHET)); //Temps pour payer
    printf("Le client n°%s a payé son billet\n", numclient);
    //fflush(stdout);
  //Opération vendre la ressource sur le sémaphore
    sem_post(&semaphoreGuichet);
  usleep(ratioMinsEnMs(TEMPS_MONTEE_TRAIN)); //Temps de pour que le passager monte dans le train


  //Faire une fonction pour comptabiliser le nombre de passager dans le train
  printf("Le client n°%s est dans le train\n", numclient);


  //Recuperation de la duree en secondes 
  duration=(getMicrotime()-debutMicrosecondes)/1000000;
  afficheTemps(duration);

  /*int* result = malloc(sizeof(int));
    *result = (10);
    return result;*/

  pthread_exit(0);
}

int main(int argc, char** argv)
{
  //Initialiation de l'horloge
  debutMicrosecondes=getMicrotime();

   //Initialisation Gare 
  gare GaresLigne1[5];
  gare GaresLigne2[5];
  gare GaresLigne3[5];
  gare GaresLigne4[5];
  gare GaresLigne5[5];
  int i;
  for(i=0; i<5; i++){
    //Remplissage des noms de Gare 
    //avec nom prédéfinis dans nomGare[], 
    //une recette à 0.0 et un nombre de Voyageurs à 0 
    //entre 5 et 20 guichets
     GaresLigne1[i] = (gare) {nomGare[i], 0.0, 0, nbAleatoire(5, 20)};
     GaresLigne2[i] = (gare) {nomGare[i+5],0.0, nbAleatoire(5, 20)};
     GaresLigne3[i] = (gare) {nomGare[i+10],0.0, nbAleatoire(5, 20)};
     GaresLigne4[i] = (gare) {nomGare[i+15],0.0,15, nbAleatoire(5, 20)};
     GaresLigne5[i] = (gare) {nomGare[i+20],0.0,8, nbAleatoire(5, 20)};
  }

  //Création des trains
  train TrainsLigne1[5];
  train TrainsLigne2[5];
  train TrainsLigne3[5];
  train TrainsLigne4[5];
  train TrainsLigne5[5];
  for (int i = 0; i < 5; ++i)
  {
    TrainsLigne1[i]=(train){(char*)numTrainRandom(), 0, 150,-1, 1};
    TrainsLigne2[i]=(train){(char*)numTrainRandom(), 0, 150,-1, 1};
    TrainsLigne3[i]=(train){(char*)numTrainRandom(), 0, 150,-1, 1};
    TrainsLigne4[i]=(train){(char*)numTrainRandom(), 0, 150,-1, 1};
    TrainsLigne5[i]=(train){(char*)numTrainRandom(), 0, 150,-1, 1};

    printf("%s\n", TrainsLigne1[i].numero);
  }

  //Création des lignes pour remplir la map du réseau ferrovier 
  ligne Lignes[5];
  Lignes[0] = (ligne) {"Frisson", TrainsLigne1, GaresLigne1};
  Lignes[1] = (ligne) {"Magie", TrainsLigne2, GaresLigne2};
  Lignes[2] = (ligne) {"Grand tour",TrainsLigne3, GaresLigne3};
  Lignes[3] = (ligne) {"La citadine", TrainsLigne4, GaresLigne4};
  Lignes[4] = (ligne) {"La paysanne", TrainsLigne5, GaresLigne5};





  if( argc == 4 )
  {
    //Taille 4 car le premier ne sert à rien 
    //Recupération des données 
    int nbrePassagers = atoi(argv[1]);
    int montantMin = atoi(argv[2]);
    int montantMax = atoi(argv[3]);
    printf("%d passagers voyagerons aujourd'hui, le montant minimum d'un billet sera de %d€ et le montant maximum de %d€.\n", nbrePassagers, montantMin, montantMax);
    //printf("%f\n", ratioMinsEnMs(PASSAGE_GUICHET));
    //printf("%f\n",ratioMinsEnMs(TEMPS_MONTEE_TRAIN));


    int nbPassagersGare1a24 = nbrePassagers / 25;
    int nbPassagersGare25 = nbrePassagers % 25;
    int gareNum = 0;

    for (int ligneI = 0; ligneI < 5; ++ligneI)
    {
      
      for (int gareI = 0; gareI < 5; ++gareI)
      {

        //nombre de passager à envoyer dans chaque gare
        int nbPassagers;
        if (gareNum != 25)
        {
          nbPassagers = nbPassagersGare1a24;
          gareNum += 1;
        }
        else{
          nbPassagers = nbPassagersGare25;
        }

        /***********
**
**
*
*
*
**CREER DES VOYAGEURS AVEC UN NUMERO ET UNE CAGNOTTE
*
*
*
**
**
        *********/
        

          //nombre de guichet de la gare
          sem_init(&semaphoreGuichet, 0, Lignes[ligneI].gares[gareI].nombreGuichets);

          //on fait payer et embarquer les passagers
          for(int numPassager = 0; numPassager < nbPassagers; numPassager++){


            char* infoThread[2] = {"",""};
            void* result;

            infoThread[0] = Lignes[ligneI].gares[gareI].nom;
            char str[12];
            sprintf(str, "%d", numPassager);
            infoThread[1] = str; 

            pthread_t gare;

            //printf("gare : %s numPassager : %s \n", infoThread[0], infoThread[1]);

            if (pthread_create(&gare, NULL, payerBillet, &infoThread))
            {
              perror("pthread_create");
              exit(EXIT_FAILURE);
            }
            if (pthread_join(gare, &result))
            {
              perror("pthread_join");
              exit(EXIT_FAILURE);
            }


          }


      }

    }


  

    printf("\nTous les passagers ont bien payés\n");
    
    //time(&fin);
    //printf("%f secondes \n", difftime(fin, debut));
    /*finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC; */


    //Recuperation de la duree en secondes 
    duration=(getMicrotime()-debutMicrosecondes)/1000000;
    afficheTemps(duration);


    //Détruire la semaphore pour les guichets
    sem_destroy(&semaphoreGuichet);


  }
  else if( argc > 4 ){
    printf("Probleme avec arguments passes en params ...\n");
    printf("Il y a %d arguments en trop.\n", argc-4);
  }
  else{
    printf("Probleme avec arguments passes en params ...\n");
    printf("Il manque %d arguments.\n", 4-argc);
  }
  return 0;
}
