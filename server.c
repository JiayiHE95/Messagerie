#include <stdio.h>               // Entrée/sortie standard
#include <sys/socket.h>          // Opérations sur les sockets
#include <arpa/inet.h>           // Conversion d'adresses IP
#include <stdlib.h>              // Fonctions de base
#include <string.h>              // Manipulation de chaînes de caractères
#include <pthread.h>             // Gestion des threads
#include <sys/sem.h>             // Sémaphores système
#include <errno.h>               // Codes d'erreur
#include <sys/ipc.h>             // Clés IPC
#include <sys/types.h>           // Types de données de base
#include <semaphore.h>           // Sémaphores
#include <ctype.h>               // Traitement des caractères
#include <stdio.h>               // Entrée/sortie standard
#include <dirent.h>              // Manipulation des répertoires
#include <unistd.h>              // Accès au système
#include <signal.h>              // Gestion des signaux
#include <time.h>                // Manipulation de l'heure

// Codes d'échappement ANSI pour les couleurs du texte
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"
#define BRIGHT_GREEN "\033[92m"

#define chemin_Abs "./server_files/"
#define NMAXCLI 3
#define maxChaine 20
#define PORTGLOBAL 3458

// Structure de message utilisée pour les communications.
typedef struct Message {
    char commande[20];   /**< La commande associée au message */
    int taille_msg;      /**< La taille du message */
    char msg[2000];      /**< Le contenu du message */
    int taille_pseudo;   /**< La taille du pseudo de l'émetteur */
    char pseudo[50];     /**< Le pseudo de l'émetteur */
    int idclient;        /**< L'ID du client qui envoie le message */
} Message;

// Variables globales pour la gestion des clients et des communications
int dS;                           /**< Le descripteur de socket principal */
int *tab_dSc;                     /**< Tableau des descripteurs de socket des clients */
char tab_pseudo[NMAXCLI][100];    /**< Tableau des pseudos des clients */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  /**< Mutex pour la synchronisation */
sem_t server_sem;                 /**< Sémaphore pour la synchronisation */

// Variables globales pour la gestion des fichiers
int p_recv = 0;                   /**< Port de réception du fichier */
int p_send = 0;                   /**< Port d'envoi du fichier */
int dS_recv;                      /**< Descripteur de socket pour la réception du fichier */
int dS_send;                      /**< Descripteur de socket pour l'envoi du fichier */
char server_files[100][50];       /**< Tableau des noms de fichiers du serveur */
int nb_file = 0;                  /**< Nombre de fichiers sur le serveur */

// Variables globales pour la gestion des chaînes
int tab_chaine_libre[maxChaine];  /**< Tableau des chaînes libres */
int tab_cap_chaine[maxChaine];    /**< Tableau des capacités réelles de chaque chaîne */
int tab_num_chaine[NMAXCLI];      /**< Tableau des numéros de chaîne pour chaque client */

/**
 * @brief Fonction qui permet de rechercher l'index d'un client à partir de son descripteur de socket.
 * 
 * La fonction recherche l'index dans le tableau des descripteurs de socket correspondant au client qui a envoyé un message.
 * Si aucun client ne correspond, la fonction retourne -1.
 *
 * @param dsc Le descripteur de socket du client.
 * @return L'index correspondant au client dans le tableau des descripteurs de socket, ou -1 si le client n'est pas trouvé.
 */
int getDscIndex(int dsc) {
    int index = -1;
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < NMAXCLI; i++) {
        if (tab_dSc[i] == dsc && tab_dSc[i] != -1) {
            index = i;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    return index;
}

/**
 * @brief Vérifie si un nom de fichier existe déjà dans le tableau `server_files`.
 *        Si le nom de fichier existe, retourne -1. Sinon, ajoute le nom de fichier
 *        au tableau et retourne 0.
 *
 * @param filename Le nom de fichier à vérifier.
 * @return -1 si le nom de fichier existe déjà, 0 sinon.
 */
int verif_nom_file(char * filename){
    pthread_mutex_lock(&mutex);
    for (int i=0;i<nb_file;i++){
        if (strcmp(server_files[i],filename)==0){
            printf(RED"File %s existe déjà\n"RESET, filename);
            pthread_mutex_unlock(&mutex);
            return -1;
        }
    }
    printf(GREEN"Nouveau file\n"RESET);
    strcpy(server_files[nb_file], filename);
    nb_file++;
    pthread_mutex_unlock(&mutex);
    return 0;
}

/**
 * @brief Fonction qui permet de rechercher une place libre dans le serveur.
 *
 * La fonction recherche un index dans le tableau des descripteurs ayant la valeur -1, indiquant une place libre.
 * Si le serveur est plein, la fonction retourne -1.
 *
 * @return L'index correspondant à une place libre dans le tableau des descripteurs, ou -1 si le serveur est plein.
 */
int getDscLibre() {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < NMAXCLI; i++) {
        if (tab_dSc[i] == -1) {
            pthread_mutex_unlock(&mutex);
            return i;
        }
    }
    pthread_mutex_unlock(&mutex);
    return -1; // Le tableau est plein
}

/**
 * @brief Fonction qui permet de rechercher une place libre pour une chaîne pour pouvoir créer cette chaine.
 * @return L'index correspondant à une place libre dans le tableau des chaînes, ou -1 si toutes les chaînes sont occupées.
 */
int getDscLibre_pourChaine () {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i< maxChaine; i++) {
      if (tab_chaine_libre[i] == -1 && i != 0) {
        pthread_mutex_unlock(&mutex);
        return i;
      }
    }
    pthread_mutex_unlock(&mutex);
    return -1;
}

/**
 * @brief Fonction qui permet de rechercher l'index d'un pseudo dans le tableau des pseudos.
 *
 * @param pseudo Le pseudo à rechercher.
 * @return L'index correspondant au pseudo dans le tableau des pseudos, ou NMAXCLI si le pseudo n'est pas trouvé.
 */
int getPseudoIndex(char* pseudo){

    for (int i = 0; i< NMAXCLI; i++) {
        size_t len = strlen(tab_pseudo[i]);
        char p[100];
        strcpy(p,tab_pseudo[i]);
        if (strcmp(p, pseudo) == 0) {
              return i;
        }
    }
    return NMAXCLI;
}

/**
 * @brief Fonction qui enregistre le pseudo du client.
 *
 * @param dsc Le descripteur de socket du client.
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */

int savePseudo(int dsc){
  
  // Réception du message du client
  // On reçoit en premier la taille du message
  int taille_msg;
  int reception_taille = recv(dsc, &taille_msg, 4, 0);
  if (reception_taille == -1) {
    perror("Erreur de réception de la taille du message du client");
    return -1;
  }

  // On reçoit après le contenu du message (pseudo)
  char pseudo [(taille_msg)] ;
  int reception = recv(dsc, pseudo, taille_msg, 0);
  if (reception == -1) {
    perror("Erreur de réception du pseudo du client");
    return -1;
  }

  printf("Pseudo du client %d : %s",getDscIndex(dsc),pseudo);

  //vérification de l'unicité du pseudo avec le mutex
    int dsIndex=getDscIndex(dsc);
    char* p = strtok(pseudo, "\n");

    for (int i = 0; i< NMAXCLI; i++) {
        if (tab_pseudo[i]!=NULL ) {
          if(strcmp(tab_pseudo[i], p)==0){
              // dans le cas ou le pseudo n'est pas bon
            int pseudo_exist = 500;
            send (dsc, &pseudo_exist, sizeof(int), 0);
            return -1;
          }
        }
    }

    //on crée les liens avec le client dans le cas ou le pseudo est ok
    pthread_mutex_lock(&mutex);
    strcpy(tab_pseudo[dsIndex], p);
    pthread_mutex_unlock(&mutex);
    int pseudo_code = 200;
    send (dsc, &pseudo_code, sizeof(int), 0);

    return 0;
}


/**
 * @brief Fonction qui permet d'envoyer un message privé à un client.
 *
 * @param p_receveur L'index du client destinataire dans le tableau des descripteurs.
 * @param p_envoyeur L'index du client envoyeur dans le tableau des descripteurs.
 * @param msg Le message à envoyer.
 */
void mp(int p_receveur, int p_envoyeur, char* msg) {
  // Vérifier que le destinataire existe et est connecté
  if (p_receveur < NMAXCLI && tab_dSc[p_receveur] != -1) {

    // Préparer le message à envoyer
    Message message;
    message.idclient = p_envoyeur;
    message.taille_pseudo = strlen(tab_pseudo[p_envoyeur]);
    strcpy(message.pseudo, tab_pseudo[p_envoyeur]);
    message.taille_msg = strlen(msg);
    strcpy(message.msg, msg);

    // Envoyer le message au destinataire
    int s = send(tab_dSc[p_receveur], &message, sizeof(Message), 0);
    if (s == -1) {
      perror("Erreur d'envoi du message");
    } else {
      printf("Message du client %s envoyé au client %s:%s", tab_pseudo[p_envoyeur], tab_pseudo[p_receveur], msg);
    }

    } else {
          // Envoyer un message d'erreur au client qui a envoyé le message
          Message error_message;
          error_message.idclient = -1;
          error_message.taille_pseudo = strlen("Server"); // c'est le serveur qui envoi le message
          strcpy(error_message.pseudo, "Server");
          error_message.taille_msg = strlen("\nl'Utilisateur entré n'est pas connecté, utilisez @printUsers pour voir les utilisateurs connectés" );
          strcpy(error_message.msg, "\nl'Utilisateur entré n'est pas connecté, utilisez @printUsers pour voir les utilisateurs connectés");

          int s = send(tab_dSc[p_envoyeur], &error_message, sizeof(Message), 0);
          if (s == -1) {
              perror("Erreur d'envoi du message d'erreur");
          } else {
              printf("Message d'erreur envoyé au client %d : %s\n", p_envoyeur, error_message.msg);
          }
    }
}


/**
 * @brief Fonction qui envoie la liste des utilisateurs connectés à un client spécifié.
 * 
 * @param dsc Le descripteur de socket du client à qui envoyer la liste des utilisateurs.
 */

void printUsers(int dsc) {
  int server = NMAXCLI;
  int totalSize = 0;
  int num;
  char num_str[10];
  Message message;
  
  //préparer une liste de pseudo avec les pseudos stocké dans la table
  pthread_mutex_lock(&mutex);
  char concatenatedpseudos[NMAXCLI*50];
  concatenatedpseudos[0] = '\0'; 
  strcat(concatenatedpseudos, "Clients disponibles sur le serveur :\n");
  for (int i = 0; i< NMAXCLI; i++) {
    if (tab_dSc[i] != -1) {
      num = i;
      sprintf(num_str, "%d", num);
      strcat(concatenatedpseudos, "Client ");
      strcat(concatenatedpseudos, num_str);
      strcat(concatenatedpseudos, " : ");
      strcat(concatenatedpseudos, tab_pseudo[i]);
      strcat(concatenatedpseudos, "\n");
    }
  }

  totalSize=strlen(concatenatedpseudos);
  pthread_mutex_unlock(&mutex);
  
  // Construction du message à envoyer
  message.taille_msg = totalSize;
  strcpy(message.msg, concatenatedpseudos);
  message.taille_pseudo = strlen("Server");
  strcpy(message.pseudo, "Server");
  message.idclient = server; // le code serveur est utilisé pour identifier le message
  
  // Envoi du message
  int send_t = send(dsc, &message, sizeof(Message), 0);
  if (send_t == -1) {
      printf("Erreur lors de l'envoi de la liste des utilisateurs connectés\n");
      return;
  }
  
  int receiver_idx = getDscIndex(dsc);
  printf(YELLOW"Liste des utilisateurs connectés envoyée au client %d\n"RESET, receiver_idx);
  
}

/**
 * @brief Fonction qui envoie un message de demande de saisie message dans le cas ou l'on envoi un fichier.
 *
 * @param dsc Le descripteur de socket du client auquel envoyer le message de demande de saisie.
 */
void envoi_message_fin(int dsc){

  //Envoi du message de demande de saisie
  Message fin_message;
  fin_message.taille_pseudo = 0;
  fin_message.taille_msg = 0;
  fin_message.idclient = NMAXCLI;

  int send_ret = send(dsc,&fin_message,sizeof(Message),0);
    if (send_ret == -1) {
      perror("Erreur de l'envoi du message de demande de saisie ");
      return;
    }

  int receiver_idx = getDscIndex(dsc);

}

/**
 * @brief Envoie le contenu du fichier la liste des commandes disponibles ainsi que leur utilisation au client.
 *
 * @param dsc Le descripteur de socket du client.
 */
void allCmd(int dsc) {
    //ouverture du manuel en lecture
    FILE* file = fopen("allCmd.txt", "r");
    if (file == NULL) {
        printf(RED"Impossible d'ouvrir le fichier allCmd.\n"RESET);
        return;
    }

    //stock le contenu du fichier concaténé
    char data[2000] = {0} ;  
    //lignes du fichier 
    char ligne[100];
    //concaténation des lignes du fichier
    while (fgets(ligne, sizeof(ligne), file) != NULL) {
        strcat(data, ligne);
    }
  
    fclose(file);
    
    //envoi en une seule fois
    Message message;
    message.taille_pseudo = strlen("Server");
    strcpy(message.pseudo, "Server");
    message.idclient = NMAXCLI;
    strcpy(message.msg, data);
    message.taille_msg = strlen(data);

    int send_ret = send(dsc, &message, sizeof(Message), 0);
    if (send_ret == -1) {
        printf("Erreur lors de l'envoi du fichier.\n");
        return;
    }
}


/**
 * @brief Termine la connexion d'un client.
 *
 * Cette fonction est appelée lorsqu'un client se déconnecte. Elle verrouille le mutex
 * pour accéder aux variables partagées, met à jour le tableau de descripteurs de socket
 * et déverrouille le mutex. Ensuite, elle libère les ressources et se termine.
 *
 * @param dsc Le descripteur de socket du client qui se déconnecte.
 */
void fin (int dsc) {

  pthread_mutex_lock(&mutex);
  for (int i = 0; i<NMAXCLI; i++){
    if(dsc==tab_dSc[i]){
        printf(MAGENTA"Déconnection du client%d\n"RESET,i);
        tab_dSc[i]=-1;
        strcpy(tab_pseudo[i],"");
        break;
    }
  }
  pthread_mutex_unlock(&mutex);
  sem_post(&server_sem);
  close(dsc);
  close(dS_recv);
  close(dS_send);
  pthread_exit(NULL);
}

/**
 * @brief Fonction exécutée par le thread de gestion de l'envoi d'un fichier permettant l'envoi d'un fichier dans le cas ou
 * le client veut récupérer un fichier précis.
 *
 * @param args Un pointeur vers un tableau d'arguments, contenant le numéro de fichier.
 * @return NULL
 */
void* send_file(void* args){

    struct sockaddr_in aC;
    socklen_t lg = sizeof(struct sockaddr_in);
    sleep(2);
    int dsc = accept(dS_send, (struct sockaddr*) &aC,&lg);

    // récupératio du numero de fichier passé en paramètres
    int* params = (int*)args;
    int numFile = params[0];

    //verifier si le fichier demandé existe bien sur le serveur
    if (numFile<nb_file && numFile>=0){
        //si ça existe, on prépare le fichier à envoyer
        FILE *fp;
        char path[100];

        strcat(path,chemin_Abs);
        strcat(path,server_files[numFile]);
        fp = fopen(path, "r");
        if (fp == NULL) {
          perror("[-]Error in reading file.");
          exit(1);
        }
        
        char ligne[1000];
        Message message;
        strcpy(message.commande, server_files[numFile]);
        message.taille_pseudo = strlen("Server");
        strcpy(message.pseudo, "Server");

        rewind(fp);
        while (fgets(ligne, 1000, fp)!=NULL){
          message.taille_msg = strlen(ligne);
          strcpy(message.msg,ligne);

          int send_ret = send(dsc,&message,sizeof(Message),0);
          if (send_ret == -1) {
            printf("Erreur lors de l\'envoi de fichier pour la ligne%s\n",ligne);
            fclose(fp);
            break;
          }
          memset(&message, 0, sizeof(Message));
        }
        envoi_message_fin(dsc);
        printf(YELLOW"File envoyé\n"RESET);
    }else{
        //si le fichier existe pas, on envoie une erreur
        printf(RED"Fichier numéro %d n'existe pas sur le serveur\n"RESET, numFile);
        Message message;
        char *msg="error";
        strcpy(message.commande, msg);
        message.taille_pseudo = strlen("Server");
        strcpy(message.pseudo, "Server");
        send(dsc, &message, sizeof(struct Message), 0);
    }
    return 0;
}

/**
 * @brief Fonction exécutée par le thread pour recevoir un fichier envoyé par un client.
 *
 * Cette fonction accepte une connexion sur le socket `dS_recv` et reçoit un message
 * contenant le nom du fichier à recevoir.
 *
 * @param var Le descripteur de socket pour la connexion établie avec le client.
 * @return NULL.
 */
void* recv_file(void* var) {
  int ds = (long)var;
  struct sockaddr_in aC;
  socklen_t lg = sizeof(struct sockaddr_in);
  
  int dsc = accept(dS_recv, (struct sockaddr*) &aC,&lg);
  
  struct Message message;
  int res = recv(dsc, &message, sizeof(struct Message), 0);
  if (res == -1) {
    printf("Erreur lors de la réception du nom de fichier.\n");
    close(dsc);
    return NULL;
  }
  if(nb_file<100){
    char *filename=malloc(100*sizeof(char));
    strcpy(filename,message.commande);

    int verif = verif_nom_file(filename);

    if(verif==0){
      FILE *fp;
      //ajouter le file reçu dans la liste files
      char path[100];
      strcat(path,chemin_Abs);
      strcat(path,filename);

      fp = fopen(path, "w");
      if (fp == NULL) {
        printf("Erreur lors de l'ouverture du fichier pour écriture.\n");
        close(dsc);
        return NULL;
      }
      
      while (1) {
        res = recv(dsc, &message, sizeof(struct Message), 0);
        if (res == -1) {
          printf("Erreur lors de la réception de la ligne du fichier.\n");
          fclose(fp);
          close(dsc);
          return NULL;
        }
        else if (res == 0) {
          break;
        }

        fprintf(fp, "%s", message.msg);
      }
      fclose(fp);
      printf(YELLOW"File reçue\n"RESET);

    }else{
    Message message;
    char *msg=RED"Fichier que vous souhaitez envoyer existe déjà sur le serveur\n"RESET;
    message.taille_msg=strlen(msg);
    strcpy(message.msg, msg);
    message.taille_pseudo = strlen("Server");
    strcpy(message.pseudo, "Server");
    send(ds, &message, sizeof(Message), 0);
    envoi_message_fin(ds);
    }

  }else{
    Message message;
    char *msg=RED"Serveur est plein et ne peut plus recevoir de nouveaux fichiers\n"RESET;
    strcpy(message.msg, msg);
    message.taille_pseudo = strlen("Server");
    strcpy(message.pseudo, "Server");
    send(ds, &message, sizeof(Message), 0);
  }
  return 0;
}


/**
 * @brief Envoie la liste des fichiers disponibles sur le serveur à un client.
 *
 * @param dsc Le descripteur de socket pour la connexion établie avec le client.
 * @return NULL.
 */
void func_server_files (int dsc) {

    char concatenatedFiles[nb_file*50]; // +1 pour le caractère de fin de chaîne
    concatenatedFiles[0] = '\0'; // initialisation à une chaîne vide
    strcat(concatenatedFiles,"Files disponibles sur le serveur :\n");
    for (int i = 0; i < nb_file; i++) {
        char index [10];
        sprintf(index, "%d", i);
        strcat(concatenatedFiles, index);
        strcat(concatenatedFiles, " : ");
        strcat(concatenatedFiles, server_files[i]);
        strcat(concatenatedFiles, "\n");
    }
    int totalSize = strlen(concatenatedFiles);

    pthread_mutex_unlock(&mutex);
    
    // Création du message à envoyer
    Message message;
    strcpy(message.commande, "@server_files");
    message.idclient = NMAXCLI; // code de serveur
    message.taille_pseudo = strlen("Server");
    strcpy(message.pseudo,"Server");
    message.taille_msg = totalSize; // taille de la chaîne de commandes
    strncpy(message.msg, concatenatedFiles, sizeof(message.msg) - 1); // copie de la chaîne de commandes
    
    // Envoi du message
    int send_ret = send(dsc, &message, sizeof(Message), 0);
    if (send_ret == -1) {
        printf("Erreur lors de l'envoi des commandes\n");
        return;
    }
    
    int receiver_idx = getDscIndex(dsc);
    printf(YELLOW"Liste des fichier server envoyé au client %d\n"RESET, receiver_idx);
}

/**
 * @brief Permet à un client de rejoindre une chaîne de discussion.
 *
 * @param dsc Le descripteur de socket pour la connexion établie avec le client.
 * @param numChaine Le numéro de la chaîne à rejoindre.
 * @return NULL.
 */
void joindre_chaine(int dsc, int numChaine) {
  int clientOnLine=0;
        for (int i=0;i<maxChaine;i++){
          if(tab_num_chaine[i]==numChaine){
            clientOnLine+=1;
          }
        }
        int ind_dsc = getDscIndex(dsc);
        if (numChaine<maxChaine ) {
          if (tab_chaine_libre[numChaine] == 0) {
            if (clientOnLine<tab_cap_chaine[numChaine]) {
                tab_num_chaine[ind_dsc] = numChaine;
                char mess[100];
                sprintf(mess, "Vous êtes actuellement dans la chaine numéro %d, bonne discussions ! \n", numChaine);
                Message message;
                message.taille_msg = strlen(mess);
                strcpy(message.msg, mess);
                message.taille_pseudo = strlen("Server");
                strcpy(message.pseudo, "Server");
                send(dsc, &message, sizeof(Message), 0);
                message.idclient = NMAXCLI;
                
            }
            else{
              Message message;
              char* mes=RED"Chaine que vous voulez joindre est pleine\n"RESET;
              message.taille_pseudo = strlen("Server");
              strcpy(message.pseudo, "Server");
              strcpy(message.msg,mes);
              message.taille_msg = strlen(mes);
              send(dsc, &message, sizeof(Message), 0); // envoi du message pour indiquer que la commande n'existe pas
            }
          }
          else {
              char* msg_chaine = "la chaine que vous essayer de joindre n'existe pas \nla commande @show_chanels vous permet de voir la liste des chaines existantes\nla commande @create_chanel vous permet de créer une nouvelle chaine\n";
                Message message;
                message.taille_msg = strlen(msg_chaine);
                strcpy(message.msg, msg_chaine);
                message.taille_pseudo = strlen("Server");
                strcpy(message.pseudo, "Server");
                message.idclient = NMAXCLI;
                send(dsc, &message, sizeof(Message), 0);
              printf("la chaine du client n'existe pas ");
          }
        }
        else {
            char* msg_chaine = "le numéro maximum de la chaine est 20 \nla commande @show_chanels vous permet de voir la liste des chaines existantes\nla commande @create_chanel vous permet de créer une nouvelle chaine\n";
                Message message;
                message.taille_msg = strlen(msg_chaine);
                strcpy(message.msg, msg_chaine);
                message.taille_pseudo = strlen("Server");
                strcpy(message.pseudo, "Server");
                message.idclient = NMAXCLI;
                send(dsc, &message, sizeof(Message), 0);
              printf("la chaine du client n'existe pas ");
        }
        printf("nb client en ligne%d\n", clientOnLine);
        Message message;
        message.taille_pseudo = strlen("Server");
        strcpy(message.pseudo, "Server");
        message.idclient = NMAXCLI;
}

/**
 * @brief Permet de créer une nouvelle chaîne de discussion.
 * @param dsc Le descripteur de socket pour la connexion établie avec le client.
 * @param infochaine Les informations de la chaîne à créer (nom, thème, capacité).
 * @return NULL.
 */
void creer_chaine (int dsc, char* infochaine) {

  // ajout dans fichier chaine.txt

  //nom de la chaine entré par l'utilisateur
  char* temp = strtok(infochaine," ");
  char* nom_c = strtok(NULL," ");
  if (nom_c == NULL) {
        printf("Erreur : nom de la chaine manquant.\n");
    }

  char* theme_c = strtok(NULL, " ");
    if (theme_c == NULL) {
        printf("Erreur : theme de la chaine manquant.\n");
    }
    printf("Theme de la chaine : %s\n", theme_c);

    char* cap_c = strtok(NULL, "\n");
    if (cap_c == NULL) {
        printf("Erreur : capacité de la chaine manquante.\n");
    }

    int num_chaine = getDscLibre_pourChaine();
    if (num_chaine == -1) {
      char* mess= "le nombre maximal de chaines est déjà atteint\n @join_chanel pour en rejoindre une\n @delete_chanel pour en supprimer une\n";
      Message message;
      message.taille_msg = strlen(mess);
      strcpy(message.msg, mess);
      message.taille_pseudo = strlen("Server");
      strcpy(message.pseudo, "Server");
      message.idclient = NMAXCLI;
      send(dsc, &message, sizeof(Message), 0);
    }
    else {
    printf("Numéro de chaine : %d\n", num_chaine);
    char num_chaine_char[maxChaine];
    sprintf(num_chaine_char, "%d", num_chaine);

    char ligne_chaine[200];

    // Générer le nom du fichier
    char nom_fichier[50];
    sprintf(nom_fichier, "chaines_files/chaine%s.txt", num_chaine_char);

    // Ouvrir le fichier en mode écriture

    FILE* fichier_c = fopen(nom_fichier, "w");
    if (fichier_c == NULL) {
        printf("Erreur lors de la création du fichier.\n");
    }

    sprintf(ligne_chaine, "numéro : %s, nom : %s, capacité : %s, Theme : %s", num_chaine_char, nom_c, cap_c, theme_c);
    printf("Ligne à écrire dans le fichier : %s\n", ligne_chaine);
    fprintf(fichier_c, "%s\n", ligne_chaine);
    tab_chaine_libre[num_chaine] = 0;
    tab_cap_chaine[num_chaine] = atoi(cap_c);

    fclose(fichier_c);
    printf("La ligne est écrite dans le fichier.\n");
      
      char mess[300];
      sprintf(mess, "Vous avez créez la chaine dont voici les informations : \n %s\n", ligne_chaine);
      Message message;
      message.taille_msg = strlen(mess);
      strcpy(message.msg, mess);
      message.taille_pseudo = strlen("Server");
      strcpy(message.pseudo, "Server");
      message.idclient = NMAXCLI;
      send(dsc, &message, sizeof(Message), 0);
    }
}

/**
 * @brief Affiche la liste des chaînes disponibles pour les clients.
 *
 * @param dsc Le descripteur de socket pour la connexion établie avec le client.
 * @return NULL.
 */
void show_chanels(int dsc) {
    // Variable pour stocker la chaîne concaténée des chaînes disponibles
    char concatenatedChanel[1000] = {0};
    strcat(concatenatedChanel, "Chaines disponibles :\n");

    // Ouvre le répertoire contenant les fichiers de chaînes
    DIR *d = opendir("chaines_files");
    if (d == NULL) {
        perror("Erreur d'ouverture du repertoire");
        exit(EXIT_FAILURE);
    }

    // Lit chaque entrée du répertoire
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        // Ignore les entrées spéciales
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || strcmp(e->d_name, ".DS_Store") == 0) {
            continue;
        }

        // Vérifie si l'entrée est un fichier régulier
        if (e->d_type == DT_REG) {
            FILE *f;
            char file_path[30] = {0};
            strcat(file_path, "chaines_files/");
            strcat(file_path, e->d_name);
            f = fopen(file_path, "r");
            if (f == NULL) {
                perror("[-]Error in reading file.");
                exit(1);
            }
            char ligne[200];

            // Lit chaque ligne du fichier de la chaîne
            while (fgets(ligne, 200, f) != NULL) {
                strcat(concatenatedChanel, ligne); // Concatène la ligne à la chaîne finale
                strcat(concatenatedChanel, "\n"); // Ajoute un saut de ligne après chaque ligne
            }
        }
    }

    // Création du message à envoyer
    Message message;
    strcpy(message.commande, "@show_chanels");
    message.idclient = NMAXCLI; // code de serveur
    message.taille_pseudo = strlen("Server");
    strcpy(message.pseudo, "Server");
    message.taille_msg = 2000; // taille de la chaîne de commandes
    strcpy(message.msg, concatenatedChanel); // copie de la chaîne de commandes

    // Envoi du message au client
    int send_ret = send(dsc, &message, sizeof(Message), 0);
    if (send_ret == -1) {
        printf("Erreur lors de l'envoi des commandes\n");
        return;
    }

    // Affiche un message de confirmation côté serveur
    int receiver_idx = getDscIndex(dsc);
    printf(YELLOW"Liste des chaines envoyée au client %d\n"RESET, receiver_idx);
}

/**
 * @brief Supprime une chaîne de discussion.
 *
 * @param dsc Le descripteur de socket pour la connexion établie avec le client.
 * @param msg Le message contenant le numéro de la chaîne à supprimer.
 * @return NULL.
 */
void supp_chaine(int dsc, char* msg) {
  int supprime = 0;
    strtok(msg, " ");
    char* numeroc = strtok(NULL, "\n");
    int num_c = atoi(numeroc);

    //ouvre le dossier dans le quels se trouvent les fichiers des chaines
    DIR* dossier = opendir("chaines_files");
    if (dossier == NULL) {
        printf("Erreur lors de l'ouverture du dossier chaines_files.\n");
        return;
    }

   // permet de créer un nom de fichier avec le numéro entré et vérifie s'il existe
    char nom_fichierc[50];
    sprintf(nom_fichierc, "chaine%s.txt", numeroc);
              printf("le nom %s ce qui  \n",nom_fichierc);
    struct dirent* entree;
    while ((entree = readdir(dossier)) != NULL) {
        if (entree->d_type == DT_REG) {
            char* nom_fichier = entree->d_name;
            if (strcmp(nom_fichierc, nom_fichier) == 0) {
              //dans le cas ou le fichier est trouvé il est alors supprimé
                    char chemin_fichier[100];
                    snprintf(chemin_fichier, sizeof(chemin_fichier), "chaines_files/%s", nom_fichier);
                    remove(chemin_fichier);
                    char mess[100];
                    sprintf(mess, "Vous avez suprimé la chaine numéro : %d \n ", num_c);
                    Message message;
                    message.taille_msg = strlen(mess);
                    strncpy(message.msg, mess, sizeof(message.msg));
                    message.taille_pseudo = strlen("Server");
                    strncpy(message.pseudo, "Server", sizeof(message.pseudo));
                    message.idclient = NMAXCLI;
                    send(dsc, &message, sizeof(Message), 0);
                    supprime = 1;
                    break;  // Sortir de la boucle après avoir supprimé le fichier correspondant
              }
        }
    }
    //si aucun fichier n'est supprimé
    if (!supprime) {
                    char* mess=  "La chaine que vous essayez de supprimer n'existe pas";
                    Message message;
                    message.taille_msg = strlen(mess);
                    strncpy(message.msg, mess, sizeof(message.msg));
                    message.taille_pseudo = strlen("Server");
                    strncpy(message.pseudo, "Server", sizeof(message.pseudo));
                    message.idclient = NMAXCLI;
                    send(dsc, &message, sizeof(Message), 0);
              }
    closedir(dossier);
}

/**
 * @brief Vérifie et exécute la commande du client.
 *
 * @param dsc Le descripteur de socket pour la connexion établie avec le client.
 * @param cmd La commande spécifiée par le client.
 * @param msg Le message supplémentaire associé à la commande.
 * @return NULL.
 */
void verif_commande(int dsc, char* cmd, char* msg) {
    if (strcmp(cmd, "@mp") == 0) {

      int sender_idx = getDscIndex(dsc);
      char* temp = strtok(msg," ");
      char* receiver_str = strtok(NULL, " ");
      int receiver_idx = getPseudoIndex(receiver_str);
      if(receiver_idx!= NMAXCLI){
        char* message = strtok(NULL, " ");
        mp(receiver_idx, sender_idx, message);
      }
    }
    else if (strcmp(cmd, "@leave_chanel\n") == 0) {
      printf("Le client revient au général\n");
      int ind_client = getDscIndex(dsc);
      int chaine_actuelle = tab_num_chaine[ind_client];
      tab_num_chaine[ind_client] = 0;

      char mess[100];
      sprintf(mess, "Vous quittez la chaine %d et rejoignez le général\n", chaine_actuelle);

      Message message;
      message.taille_msg = strlen(mess) + 1;
      strncpy(message.msg, mess, sizeof(message.msg) - 1);
      message.msg[sizeof(message.msg) - 1] = '\0';
      message.taille_pseudo = strlen("Server") + 1;
      strncpy(message.pseudo, "Server", sizeof(message.pseudo) - 1);
      message.pseudo[sizeof(message.pseudo) - 1] = '\0';
      message.idclient = NMAXCLI;
      send(dsc, &message, sizeof(Message), 0);
    }
     else if (strcmp(cmd, "@allCmd") == 0) {
        allCmd(dsc);
        //envoi_message_fin(dsc);
    } else if (strcmp(cmd, "@printUsers") == 0) {
        printUsers(dsc);
        //envoi_message_fin(dsc);
    } else if (strcmp(cmd, "@fin") == 0) {
        fin(dsc);
    } else if (strcmp(cmd, "@get_file") == 0) {
        char * numFi=strtok(msg," ");
        int params[1];
        params[0] = atoi(msg);
        pthread_t client_file_thread;
        if (pthread_create(&client_file_thread, NULL, send_file, (void *)params)!= 0 ){
          perror("Erreur lors de la création du thread du client : ");
          exit(1);
        }
    } else if (strcmp(cmd, "@server_files") == 0) {
        func_server_files(dsc);
        //envoi_message_fin(dsc);
    }
    else if (strcmp(cmd, "@send_file") == 0) {
        char * numFi=strtok(msg," ");
        pthread_t client_file_thread;
        if (pthread_create(&client_file_thread, NULL, recv_file, (void*)(long)dsc)!= 0 ){
          perror("Erreur lors de la création du thread du client : ");
          exit(1);
        }
    }
    else if (strcmp(cmd, "@show_chanels") == 0) {
        show_chanels(dsc);
    }    
    else if (strcmp(cmd, "@join_chanel") == 0) {
        int numChaine=atoi(strtok(msg," "));
        joindre_chaine(dsc,numChaine);
    }
  
    else if (strcmp(cmd, "@create_chanel") == 0) {
      printf("je rentre iciii\n");
      creer_chaine (dsc, msg);
    }
    else if (strcmp(cmd, "@delete_chanel") == 0) {
      printf("ok je vais supprimer la chaine\n");
      supp_chaine (dsc, msg);
    }
    else {
        char* mess =RED"Commande non reconnue ('@allCmd' pour lister toutes les commandes) \n"RESET;
        Message message;
        message.taille_msg = strlen(mess);
        strcpy(message.msg, mess);
        message.taille_pseudo = strlen("Server");
        strcpy(message.pseudo, "Server");
        message.idclient = NMAXCLI;

        printf(RED"Mauvaise commande du client\n"RESET);
        send(dsc, &message, sizeof(Message), 0); // envoi du message pour indiquer que la commande n'existe pas
        //envoi_message_fin(dsc);
    }
}

/**
 * @brief Vérifie les informations d'identification du client.
 *
 * @param dsc Le descripteur de socket pour la connexion établie avec le client.
 * @return 0 si l'authentification est réussie, -1 sinon.
 */
int verifClient(int dsc) {
    int taille_msg;
    int reception_taille = recv(dsc, &taille_msg, 4, 0);
    if (reception_taille == -1) {
        perror("Erreur de réception de la taille du message du client");
        return -1;
    }

    // On reçoit après le contenu du message
    char infos[(taille_msg)];
    int reception = recv(dsc, infos, taille_msg, 0);
    if (reception == -1) {
        perror("Erreur de réception du pseudo du client");
        return -1;
    }
    char* ic = strtok(infos, " ");
    char* login = strtok(NULL, " ");
    char* mdp = strtok(NULL, " ");

    FILE* file = fopen("clients.txt", "r+");
    if (file == NULL) {
        printf("Impossible d'ouvrir le fichier.\n");
        return 1;
    }

    char line[100];

    // Vérification pour l'inscription
    if (strcmp(ic, "i") == 0) {
        // Vérification si le login existe déjà dans le fichier
        while (fgets(line, sizeof(line), file)) {
            if (strstr(line, login) != NULL) {
                int insciption_code = 500;
                send(dsc, &insciption_code, sizeof(int), 0);
                return -1;
            }
        }

        // Construction de la chaîne login_mdp
        char login_mdp[50] = {0};
        strcat(login_mdp, login);
        strcat(login_mdp, " ");
        strcat(login_mdp, mdp);

        // Écriture de la chaîne login_mdp dans le fichier
        fprintf(file, "\n%s\n", login_mdp);
        fclose(file);

        // Envoi du code de réussite de l'inscription au client
        int insciption_code = 200;
        printf(GREEN"Inscription réussie\n"RESET);
        send(dsc, &insciption_code, sizeof(int), 0);
        return 0;
    } else { // Vérification pour la connexion
        // Vérification si le login existe dans le fichier et si le mot de passe correspond
        while (fgets(line, sizeof(line), file)) {
            if (strstr(line, login) != NULL) {
                strtok(line, " ");
                char* mdp_bd = strtok(NULL, "\n");
                fclose(file);
                if (strcmp(mdp, mdp_bd) == 0) {
                    printf(GREEN"Connexion réussie\n"RESET);
                    int insciption_code = 200;
                    send(dsc, &insciption_code, sizeof(int), 0);
                    return 0;
                } else {
                    printf(RED"Mdp incorrect\n"RESET);
                    int insciption_code = 500;
                    send(dsc, &insciption_code, sizeof(int), 0);
                    return -1;
                }
            }
        }
        printf(RED"Login incorrect\n"RESET);
        int insciption_code = 500;
        send(dsc, &insciption_code, sizeof(int), 0);
    }
    return -1;
}


/**
 * @brief Vérifie et exécute la commande du client.
 *
 * @param dsc Le descripteur de socket pour la connexion établie avec le client.
 * @param cmd La commande spécifiée par le client.
 * @param msg Le message supplémentaire associé à la commande.
 * @return NULL.
 */
void* client(void* var) {
    sem_wait(&server_sem);
    int dsc = (long)var;

    // Envoi du port d'envoi de fichiers au client
    send (dsc, &p_send, sizeof(int), 0);
    // Envoi du port de réception de fichiers au client
    send (dsc, &p_recv, sizeof(int), 0);
    printf("Port de réception et d'envoi de fichiers envoyé au client\n");

    int connexion = 0;
    do {
        // Vérification de l'authentification du client
        connexion = verifClient(dsc);
        printf("Code de connexion : %d\n", connexion);
    } while (connexion != 0);

    int save = 0;
    do {
        // Sauvegarde du pseudo du client
        save = savePseudo(dsc);
    } while (save != 0);

    while(1) {
        // Réception du message du client
        Message message; // Réception du message de type Message
        int reception = recv(dsc, &message, sizeof(Message), 0);
        if (reception == -1) {
            perror("Erreur de réception du message du client");
            break;
        }
        
        // Vérification des arguments du message
        message.idclient = getDscIndex(dsc);
        size_t len = strcspn(tab_pseudo[message.idclient], "\n"); // Longueur de la chaîne sans le saut de ligne
        strncpy(message.pseudo, tab_pseudo[message.idclient], len); // Copie de la chaîne sans le saut de ligne
        message.pseudo[len] = '\0'; // Ajout du caractère de fin de chaîne
        message.taille_pseudo = strlen(message.pseudo);
   
        // Gestion de la déconnexion du client sans commande "@fin"
        if (reception == 0) {
            fin(dsc); 
            sem_post(&server_sem);
            pthread_exit(NULL);
        } else {
            // Si le message commence par "@", il est traité comme une commande
            if (message.commande[0] == '@') {
                // Appeler la fonction de traitement des commandes
                verif_commande(dsc, message.commande, message.msg);
            } else {
                // S'il n'y a pas d'erreur lors de la réception du message, on le diffuse aux autres clients
                int indexClient = getDscIndex(dsc);
                char* pseudo = tab_pseudo[indexClient];
                printf("Message reçu depuis %s : %s", message.pseudo , message.msg);
                int ind_client = getDscIndex(dsc);
                for (int i = 0; i < NMAXCLI; i++) {
                    if (dsc != tab_dSc[i] && tab_dSc[i] != -1 && tab_num_chaine[i] == tab_num_chaine[ind_client]) {
                        send(tab_dSc[i], &message, sizeof(Message), 0); // Envoi du message
                        printf("Message envoyé au client %d : %s", i, message.msg);
                    }
                }
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    printf(BRIGHT_GREEN"Début programme\n"RESET);


    //stocker file server in tab
    DIR *dir = opendir(chemin_Abs);
    if (dir == NULL) {
        perror("Erreur d'ouverture du repertoire");
        exit(EXIT_FAILURE);
    }
    // Lit chaque entrée du répertoire
    struct dirent *entry;
    int i=0;

    while ((entry = readdir(dir)) != NULL) {
        // Ignore les entrées spéciales
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".DS_Store") == 0) {
            continue;
        }
        // Vérifie si l'entrée est un fichier régulier
        if (entry->d_type == DT_REG) {
            strcpy(server_files[nb_file],entry->d_name);
            nb_file++;
        }
    }

    //initialisation chaine
    for (int i=0;i<maxChaine;i++){
      tab_chaine_libre[i]=-1;
      tab_cap_chaine[i]=0;
    }
    for (int i=0;i<NMAXCLI; i++){
      tab_num_chaine[i]=0;
    }

DIR *d = opendir("chaines_files");

    if (d == NULL) {
        perror("Erreur d'ouverture du repertoire");
        exit(EXIT_FAILURE);
    }
    // Lit chaque entrée du répertoire
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        // Ignore les entrées spéciales
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || strcmp(e->d_name, ".DS_Store") == 0) {
            continue;
        }
        // Vérifie si l'entrée est un fichier régulier
        if (e->d_type == DT_REG) {
            FILE *f;
            
            char file_path[30]={0};

            strcat(file_path,"chaines_files/");
            strcat(file_path,e->d_name);
            f = fopen(file_path, "r");
            if (f == NULL) {
              perror("[-]Error in reading file.");
              exit(1);
            }
            char data[1000] = {0};
            char ligne[1000];
        // pemet de lire correctement la première ligne de chaine pour tirer les informations utiles
            while (fgets(ligne, 1000, f) != NULL) {
              strtok(ligne, " :");
              strtok(NULL, " ");
              char* numChaine = strtok(NULL, ",");
              tab_chaine_libre[atoi(numChaine)]=0;
              strtok(NULL, ": ");
              strtok(NULL, " ");
              char* nomChaine = strtok(NULL, ",");
              strtok(NULL, ": ");
              strtok(NULL, " ");
              char* cap = strtok(NULL, ", ");
              tab_cap_chaine[atoi(numChaine)]=atoi(cap);
              strtok(NULL, ": ");
              strtok(NULL, " ");
              int capacite = atoi(cap);
              char* theme = strtok(NULL, ", ");

              printf("Numéro de chaîne: %s\n", numChaine);
              printf("Nom de chaîne: %s\n", nomChaine);
              printf("Capacité: %d\n", capacite);
              printf("Thème: %s\n", theme);
          }

        }
    }

  // Création d'une socket pour le server celle de la réception et envoi de messages
    dS = socket(PF_INET, SOCK_STREAM, 0);
    printf(BRIGHT_GREEN"Socket Créée\n"RESET);

  // Configuration de la socket
    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = INADDR_ANY ;
    ad.sin_port = htons(atoi(argv[1])) ; // la relie à un port
    int bind_message_res = bind(dS, (struct sockaddr*)&ad, sizeof(ad)); // le bind sert à renommer la socket dans le port que l'on veut pour pouvoir le reutiliser par le client
    if (bind_message_res == -1) {
        perror("l'erreur est : ");
        exit(1);
    };
    printf(BRIGHT_GREEN"Socket Nommée\n"RESET);


  // Server est en écoute, pour enregistrer les clients, on crée un tableau pour stocker leurs descripteurs
    listen(dS, 3) ;
    printf(BRIGHT_GREEN"Mode écoute\n"RESET);
    
    // Création d'une socket pour le server celle de la réception des fichiers
    dS_send = socket(PF_INET, SOCK_STREAM, 0);
    printf(BRIGHT_GREEN"Socket send file Créée\n"RESET);
     
     // Configuration de la socket
    struct sockaddr_in ad1;
    ad1.sin_family = AF_INET;
    ad1.sin_addr.s_addr = INADDR_ANY ;
    p_send=  3456;
    ad1.sin_port = htons(p_send) ;
    int bind_send_res = bind(dS_send, (struct sockaddr*)&ad1, sizeof(ad1));
    if (bind_send_res == -1) {
       perror("l'erreur est : ");
       exit(1);
    };
    listen(dS_send, 3);
    printf(BRIGHT_GREEN"send file écoute\n"RESET);
     
    // création soket receive
    dS_recv = socket(PF_INET, SOCK_STREAM, 0);
    printf(BRIGHT_GREEN"Socket recv file Créée\n"RESET);

    struct sockaddr_in ad2;
    ad2.sin_family = AF_INET;
    ad2.sin_addr.s_addr = INADDR_ANY ;
    p_recv= 3457;
    ad2.sin_port = htons(p_recv) ;
    int bind_receive_res = bind(dS_recv, (struct sockaddr*)&ad2, sizeof(ad2)); 
    if (bind_receive_res == -1) {
        perror("l'erreur est : ");
        exit(1);
    };

  
    listen(dS_recv, 3) ;
    printf(BRIGHT_GREEN"recv file écoute\n"RESET);

    sem_init(&server_sem, 0, NMAXCLI);

   //allocation mémoire pour le tableau
    tab_dSc = malloc(NMAXCLI* sizeof(int));

    for(int i = 0 ; i<NMAXCLI ; i++){
        tab_dSc[i]=-1;
    }

  //Création d'un thread pour chaque client qui se connecte 
  //dans la limite des places disponibles dans le server
    pthread_t client_thread;

    while(1){
        int place_restante;
        sem_getvalue(&server_sem, &place_restante);
    
        if (place_restante != 0) {
            int dscLibre=getDscLibre();
            if (dscLibre!=-1){
        //attente de l'autorisation d'accès au sémaphore
        // création d'une socket pour le client 
                struct sockaddr_in aC ;
                socklen_t lg = sizeof(struct sockaddr_in) ;
                int dSC = accept(dS, (struct sockaddr*) &aC,&lg);
        
                if (dSC == -1 ) {
                    perror ("l'erreur est : ");
                    exit(1);
                };

                //ajouter le client au tableau tab_dSc
                pthread_mutex_lock(&mutex);
                tab_dSc[dscLibre] = dSC;
                printf(BRIGHT_GREEN"Client %d Connecté\n"RESET,dscLibre);
        
                if (pthread_create(&client_thread, NULL, client, (void*)(long)dSC) != 0) {
                    perror("Erreur lors de la création du thread du client : ");
                    exit(1);
                }
                pthread_mutex_unlock(&mutex);
            }
        }

        sleep(1);
    }

    //libération de la mémoire
    sem_destroy(&server_sem);
    free(tab_dSc);
    return 0;
}
