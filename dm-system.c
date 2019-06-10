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
#define DUREE_DEPLACEMENT_TRAIN_ENTRE_GARE 10.

//bin ne sert à rien... Juste à placer des scanf dans différents endroit du code pour 
//mettre en pause l'éxécution lors du déboguage
int bin;

typedef struct train train;
struct train
{
  char *numero;
  int nbPassagers;
  int nombrePlaces;
  int gareActuelle;
  //1 si trajet aller 0 si retour
  int aller;
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
  gare gares[5];
};


typedef struct voyageur voyageur;
struct voyageur
{
  int identifiantClient;
  int cagnote;
  gare derniereGareDePassage;
  gare garesDePassage[];
};

typedef struct contenerTrainGares contenerTrainGares;
struct contenerTrainGares
{
  train* Train;
  ligne* Ligne;
};


//Sémaphores pour limiter les accès
//les montées descentes de trains, les guichets, les tunnels...
sem_t semaphoreGuichet;
sem_t semaphoreGare;
//Cette liste de semaphore permet d'assurer à un train d'avoir accès à l'unique quais d'embarquement
sem_t semaphoreGares[20];

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
//Nous n'utilisons que 20 gares pour avoir exactement 2 gares qui ont des interchangements sur une meme ligne
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
  "Avignon"/*,
  "Adainville",
  "Saint-Rémy-Les-Chebreuses",
  "Saint-Brieuc",
  "La Rochelle",
  "Rennes",
  "Gap"
*/};



 
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
char randomNomTrainGlobal[25][6];
char* numTrainRandom(){
  for (int k = 0; k < 25; ++k)
  {
    for (int i = 0; i < 5; ++i)
    {
      // (char)rand()%9 => une valeur en code ascii compris entre 0 et 9 entière
      char tempChar = ('0'+(char)(rand()%9));
      randomNomTrainGlobal[k][i]=tempChar;
    }
    //ajout du caractère '\0' pour signaler la fin du "string"
    randomNomTrainGlobal[k][5]='\0';
    //printf("ligne %d  : %s\n",k, randomNomTrainGlobal[k] );
  }
    

  return randomNomTrainGlobal;
}

/*************************************************************
** affiche l'état du train   
**Inputs : Train 
**Ouputs : 
**************************************************************/
void trainDisp(train Train){
  printf("Le Train numero %s a %d passager(s). Il contient %d place(s) et est a la gare %s, sens %s\n", 
    Train.numero, Train.nbPassagers, Train.nombrePlaces,( Train.gareActuelle!=-1 ? nomGare[Train.gareActuelle] : "--Depot--" ), ((Train.aller==1) ? "aller" : "retour"));
}

/*************************************************************
** affiche l'état de la gare    
**Inputs : Gare 
**Ouputs : 
**************************************************************/
void gareDisp(gare Gare){
  printf("La gare %s a comme recette %f, %d voyageur et %d guichet(s)\n",Gare.nom, Gare.recette, Gare.nbVoyageurs, Gare.nombreGuichets );
 }
 

 /*************************************************************
** affiche l'état d'une ligne     
**Inputs : Ligne
**Ouputs : 
**************************************************************/
void LigneDisp(ligne Ligne){
  printf("La ligne %s a les trains suivants : \n",Ligne.nom);
  for (int i = 0; i < 5; ++i)
  {
    printf(" ---> ");
    trainDisp(Ligne.trains[i]);
  }
  printf("et les Gares suivantes : \n");
  for (int i = 0; i < 5; ++i)
  {
    printf(" ---> ");
    gareDisp(Ligne.gares[i]);
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

 /*************************************************************
** Initialise les semaphores des gares     
**Inputs : 
**Ouputs : 
**************************************************************/
void initSemaphoreGares(){
  for (int i = 0; i < 20; ++i)
  {
    sem_init(&semaphoreGares[i], 0, 2);
  }
}

 /*************************************************************
** Initialise les semaphores des gares     
**Inputs : 
**Ouputs : 
**************************************************************/
void destroySemaphoreGares(){
  for (int i = 0; i < 20; ++i)
  {
    sem_destroy(&semaphoreGares[i]);
  }
}

/*************************************************************
** gère les entrées de gares     
**Inputs : infos => un train et sa prochaine destination de gare en char*
**Ouputs : 
**************************************************************/
void* TrainArriveGare(void* infos)
{
  //Récupération des données 
  contenerTrainGares* contener= (contenerTrainGares*)infos; 
  train* leTrain = contener->Train;
  //int idGareDest = (int)contener->idGareDest; 
  
  //Prise du semaphore 
  sem_wait(&semaphoreGare);

  //Une fois prise, transfert du train
  //leTrain->gareActuelle=idGareDest;
  //train à la nouvelle gare 

  //TODO : traitement en gare ICI -----------------------------
  //descente des passagers 
  //montee des passagers 

  //Libère le sémaphore  
  sem_post(&semaphoreGare);
  usleep(ratioMinsEnMs(DUREE_DEPLACEMENT_TRAIN_ENTRE_GARE));
 // return idGareDest;
}

void* AvancementTrain(void *infos){
  //Récupération des données 
  contenerTrainGares* contener= (contenerTrainGares*)infos; 
  train* leTrain = contener->Train;
  ligne* laLigne = contener->Ligne;

//Recherche de l'id Correspondant
  int idGareDest = leTrain->gareActuelle;
  printf("%d\n", !1);
  if(idGareDest==4 || idGareDest==0){leTrain->aller=!leTrain->aller;}
  usleep(100000);
  //printf("Ancienne dest : %d  zncienne gare %s  ",idGareDest, nomGare[idGareDest]);
  if(leTrain->aller==0){
    //on veut revenir en arrière donc on va soustraire 1 à l'indice de gare
    //Calcul général : on soustrait 1 à l'indice
    //Mais si on est à l'indice 0 on doit revenir à l'indice 4 donc 
    // 0-1 = -1 => +5 = 4 => %5 =4 
    idGareDest=(idGareDest-1+5)%5;
  }
  else{
    //On avance on fait le modulo de 5 pour le cas du débordement
    idGareDest=(idGareDest+1)%5;
  }

  //Avancement du train 
  //Une fois prise, transfert du train
  leTrain->gareActuelle=idGareDest; 
  //printf("Prochaine dest : %d  %d  nouvelle gare  :%s\n",idGareDest, leTrain->aller,nomGare[idGareDest] );
    
}


int main(int argc, char** argv)
{
  printf("%s\n\n", "Debut programme." );
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
    //La gare de la ligne N+1 a comme première gare la dernière gare de la ligne N
    //principe de chevauchement des gares.
     GaresLigne1[i] = (gare) {nomGare[i], 0.0, 0, nbAleatoire(5, 20)};
     GaresLigne2[i] = (gare) {nomGare[i+4],0.0, nbAleatoire(5, 20)};
     GaresLigne3[i] = (gare) {nomGare[i+8],0.0, nbAleatoire(5, 20)};
     GaresLigne4[i] = (gare) {nomGare[i+12],0.0,15, nbAleatoire(5, 20)};
     GaresLigne5[i] = (gare) {nomGare[(i+16)%20],0.0,8, nbAleatoire(5, 20)};
  }

  //Création des trains
  train TrainsLigne1[5];
  train TrainsLigne2[5];
  train TrainsLigne3[5];
  train TrainsLigne4[5];
  train TrainsLigne5[5];

  //remplissage du tableau randomNomTrainGlobal de valeurs aléatoire
  numTrainRandom();

  //printf("value :here  : %s\n", randomNomTrainGlobal[0] );

  for (int i = 0; i < 5; i++)
  {
    TrainsLigne1[i]=(train){&randomNomTrainGlobal[i], 0, 150,(int)nbAleatoire(0,4), (int)nbAleatoire(0,2)};
    TrainsLigne2[i]=(train){&randomNomTrainGlobal[i+5], 0, 150,(int)nbAleatoire(4,8), (int)nbAleatoire(0,2)};
    TrainsLigne3[i]=(train){&randomNomTrainGlobal[i+10], 0, 150,(int)nbAleatoire(8,12), (int)nbAleatoire(0,2)};
    TrainsLigne4[i]=(train){&randomNomTrainGlobal[i+15], 0, 150,(int)nbAleatoire(12,16), (int)nbAleatoire(0,2)};
    TrainsLigne5[i]=(train){&randomNomTrainGlobal[i+20], 0, 150,(int)nbAleatoire(16,20), (int)nbAleatoire(0,2)};

    //printf("%s\n", TrainsLigne1[i].numero);
  }

  //Création des lignes pour remplir la map du réseau ferrovier 
  ligne Lignes[5];
  Lignes[0] = (ligne){"Frisson", (train*) TrainsLigne1, (gare*) GaresLigne1};
  Lignes[1] = (ligne){"Magie", (train*) TrainsLigne2, (gare*) GaresLigne2};
  Lignes[2] = (ligne){"Grand tour", (train*) TrainsLigne3, (gare*) GaresLigne3};
  Lignes[3] = (ligne){"La citadine", (train*) TrainsLigne4, (gare*) GaresLigne4};
  Lignes[4] = (ligne){"La paysanne", (train*) TrainsLigne5, (gare*) GaresLigne5};

  //Transvaser un tableau dans un tableau ne marche pas 
  for (int i = 0; i < 5; ++i)
  {
    //Ligne[0]
    Lignes[0].trains[i]=TrainsLigne1[i];
    Lignes[0].gares[i]=GaresLigne1[i];

    //Ligne[1]
    Lignes[1].trains[i]=TrainsLigne1[i];
    Lignes[1].gares[i]=GaresLigne1[i];

    //Ligne[2]
    Lignes[2].trains[i]=TrainsLigne2[i];
    Lignes[2].gares[i]=GaresLigne2[i];

    //Ligne[3]
    Lignes[3].trains[i]=TrainsLigne3[i];
    Lignes[3].gares[i]=GaresLigne3[i];

    //Ligne[4]
    Lignes[4].trains[i]=TrainsLigne4[i];
    Lignes[4].gares[i]=GaresLigne4[i];
  }

  printf("----------------------------------  Etat initial du Reseau ferrovier  ----------------------------------\n\n");
  for (int i = 0; i < 5; ++i)
  {
    printf("\n");
    LigneDisp(Lignes[i]);
    printf("\n");
  }
  printf("\n-------------------------------  Fin de l'affichage du Reseau ferrovier  -------------------------------\n\n");

  
  //scanf("%d",&bin);

  if( argc == 4 )
  {
    //Initialisation des semaphores des gares 
    initSemaphoreGares();

    //Gestion des trains sur la ligne 
    //initialisation de la semaphore Gare
    //chaque gare n'a que 2 voies

    //TODO 
    //CORRECTION BUG PK SEUL LE PREMIER TRAIN EST MODIFIE ?? 
    //GESTION DE LA SEMAPHORE 

    sem_init(&semaphoreGare, 0, 2);

    for (int k = 0; k < 1; ++k)
    {//Pour chaque ligne
      for (int l = 0; l < 5; ++l)
      {//pour chaque train de la ligne
        int i=l%5;
        //i correspond à l'indice des trains dans la ligne d'où le modulo 5
        //68 est le nombre d'appel de la fonction avanceTrain
        LigneDisp(Lignes[k]);
        //Creation du thread pour le train
        pthread_t AvanceTrain;

        //creation du contener 
        contenerTrainGares contener;
        contener.Train = &Lignes[k].trains[i];
        contener.Ligne = &Lignes[k];


        void* result = pthread_create(&AvanceTrain, NULL, AvancementTrain, &contener);
      
        if (result)
        {
          perror("pthread_create");
          exit(EXIT_FAILURE);
        }
        if (pthread_join(AvanceTrain, &result))
        {
          perror("pthread_join");
          exit(EXIT_FAILURE);
        }






        /*//Recupération de la prochaine destination du train 
        char *gareNameActuel = Lignes[0].trains[i].gareActuelle; 
        int idGareDest=0;
        for (int l = 0; l < 20; ++l)
        {
          if( Lignes[0].trains[i].gareActuelle!=-1 &&
                strcmp(nomGare[l],nomGare[Lignes[0].trains[i].gareActuelle])==0 )
          {
            idGareDest=l;    
          }
          else if(Lignes[0].trains[i].gareActuelle==-1){
            idGareDest=0*4;
          }
        }
        idGareDest=1;

        if(Lignes[0].trains[i].aller==0){
          //on veut revenir en arrière donc on va soustraire 1 à l'indice de gare
          //Calcul général : on soustrait 1 à l'indice
          //Mais si on est à l'indice 0 on doit revenir à l'indice 19 donc 
          // 0-1 = -1 => +20 = 19 => %20 =19 
          idGareDest=(idGareDest-1+20)%20;
        }
        else{
          //On avance on fait le modulo de 20 pour le cas du débordement
          idGareDest=(idGareDest+1)%20;
        }

        //Création du contener 
        contenerTrainGares contener;
        contener.idGareDest = idGareDest;
        contener.Train = &Lignes[0].trains[i];
        

        //création du thread
        pthread_t gareGestionTrain;
        void* result = pthread_create(&gareGestionTrain, NULL, TrainArriveGare, &contener);
      
        if (result)
        {
          perror("pthread_create");
          exit(EXIT_FAILURE);
        }
        if (pthread_join(gareGestionTrain, &result))
        {
          perror("pthread_join");
          exit(EXIT_FAILURE);
        }
      

        LigneDisp(Lignes[0]);*/
      }
    }
      
    //destruction des semaphores gares 
    destroySemaphoreGares();
    //trainDisp(Lignes[0].trains[0]);
    //Supposons que la gare de départ est Montparnasse indice 1
    /*char* gareName="Bordeaux";
    int idGareDest=0;
    for (int i = 0; i < 20; ++i)
    {
      if(strcmp(nomGare[i],gareName)==0){
        idGareDest=i;      
      }
      
    }

    contenerTrainGares contener;
    contener.idGareDest = idGareDest;
    contener.Train = &Lignes[0].trains[0];
    /*printf("Main %d\n",&Lignes[0].trains[0]);
    printf("Main gare actuelle %d\n",&Lignes[0].trains[0].gareActuelle);*/
    

   /* pthread_t gareGestionTrain;

    void* result = pthread_create(&gareGestionTrain, NULL, TrainArriveGare, &contener);
    if (result)
    {
      perror("pthread_create");
      exit(EXIT_FAILURE);
    }
    if (pthread_join(gareGestionTrain, &result))
    {
      perror("pthread_join");
      exit(EXIT_FAILURE);
    }
    trainDisp(Lignes[0].trains[0]);
    //destruction du semaphore
    sem_destroy(&semaphoreGare);*/
    /*for (int i = 0; i < 5; ++i)
    {
      //pour chaque gare 

    }*/
  }
  



  //if( argc == 4 ) TO REPLACE 
  if( argc == 0 )
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


        //création des voyageurs avec un numero et une cagnote 


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

        //gare laGare = (gare) Lignes[0].gares[1]; <<-----<< Pourquoi gares[1] ?? 
        gare laGare = (gare) Lignes[ligneI].gares[gareI];

        printf("nombre de guichets en gare : %d\n", laGare.nombreGuichets);
        printf("nom gare : %s\n", laGare.nom);
        
        
        //scanf("%d",&bin);


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

          /*struct voyageur
          {
            int identifiantClient;
            gare derniereGareDePassage;
            gare garesDePassage[];
            int cagnote;
          };*/

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

    printf("\n\n%s\n", "Fin programme." );
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