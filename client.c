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
#define LIGHT_BLUE "\033[94m"
#define BRIGHT_GREEN "\033[92m"
#define BRIGHT_BLUE "\033[94m"
#define YELLOW_GREEN "\033[93m"

#define NMAXCLI 3
#define chemin_Abs "./client_files/"

typedef struct Message Message;

// struct du message à recevoir et à envoyer
struct Message {
    char commande[20];
    int taille_msg;
    char msg[2000]; // peut envoyer une copie d'un fichier aussi
    int taille_pseudo;
    char pseudo[50];
    int idclient; // celui qui envoi le message
};

// communication générale 
int dS;
char *colors[]={BLUE,CYAN,LIGHT_BLUE};
char* ip;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//gestion de fichier
int p_send, p_recv; // prot réception et envoi fichier
char client_files[100][50]; //nom des fichier du clients
int nb_file=0; // nb de fichiers du clients


/**
 * @brief Envoie le pseudo saisi par le client au serveur.
 *
 * @return 0 en cas de succès, -1 en cas d'erreur.
 */
int send_pseudo() {
    char pseudo[100];
    printf(BOLD"Entrez votre pseudo : \n"RESET);
    fgets(pseudo, sizeof(pseudo), stdin);
    int tpseudo = strlen(pseudo) + 1;
    int send_tpseudo = send(dS, &tpseudo, 4, 0);
    if (send_tpseudo == -1) {
        return -1;
    }
    // On envoie le contenu du message
    int send_pseudo = send(dS, pseudo, tpseudo, 0);
    if (send_pseudo == -1) {
        return -1;
    }
    printf(YELLOW_GREEN"Pseudo Envoyé\n"RESET);
    return 0;
}


/**
 * @brief Affiche la liste des fichiers du client.
 */
void my_files() {
    printf(MAGENTA"Files client :\n"RESET);
    for (int i = 0; i < nb_file; i++) {
        printf(MAGENTA"%d : %s\n"RESET, i, client_files[i]);
    }
    printf(BOLD"Entrez un message ('@fin' pour arrêter): \n"RESET);
}

/**
 * @brief Vérifie si le nom de fichier existe déjà dans le répertoire client.
 *
 * @param filename Le nom de fichier à vérifier.
 * @return 0 si le fichier n'existe pas, -1 si le fichier existe déjà.
 */
int verif_nom_file(char* filename) {
    if (strcmp(filename, "error") == 0) {
        printf(RED"Le fichier que vous souhaitez récupérer n'existe pas\n"RESET);
        return -1;
    }
    pthread_mutex_lock(&mutex);
    // Vérifie si le fichier existe déjà dans le répertoire client
    for (int i = 0; i < nb_file; i++) {
        if (strcmp(client_files[i], filename) == 0) {
            printf(RED"Vous possédez déjà %s\n"RESET, filename);
            pthread_mutex_unlock(&mutex);
            return -1;
        }
    }
    printf(BRIGHT_GREEN"Nouveau fichier\n"RESET);
    strcpy(client_files[nb_file], filename);
    nb_file++;
    pthread_mutex_unlock(&mutex);
    printf(BOLD"Entrez un message ('@fin' pour arrêter): \n"RESET);
    return 0;
}


/**
 * @brief Fonction exécutée par le thread pour recevoir un fichier du serveur.
 *
 * @return NULL.
 */
void* recv_file() {
    // Création de la socket
    int dSFile = socket(PF_INET, SOCK_STREAM, 0);
    if (dSFile == -1) {
        perror("Erreur de création de la socket receive file : ");
        exit(1);
    }
    printf(BRIGHT_GREEN"Socket Receive File Créée\n"RESET);

    // Établissement de la connexion avec le serveur
    struct sockaddr_in aS;
    aS.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(aS.sin_addr));
    aS.sin_port = htons(p_recv);

    socklen_t lgA = sizeof(struct sockaddr_in);

    sleep(1);
    int co_res = connect(dSFile, (struct sockaddr*)&aS, lgA);
    if (co_res == -1) {
        printf(RED"Connexion socket receive file échouée, veuillez réessayer ultérieurement\n"RESET);
    } else {
        printf(BRIGHT_GREEN"Socket Receive File Connectée\n"RESET);

        // Réception du nom de fichier
        struct Message message;
        int res = recv(dSFile, &message, sizeof(struct Message), 0);
        if (res == -1) {
            printf("Erreur lors de la réception du nom de fichier.\n");
            close(dSFile);
            return NULL;
        }

        char* filename = malloc(100 * sizeof(char));
        strcpy(filename, message.commande);

        int verif = verif_nom_file(filename);

        if (verif == 0) {
            FILE* fp;
            char path[100];
            strcat(path, chemin_Abs);
            strcat(path, filename);

            fp = fopen(path, "w");
            if (fp == NULL) {
                printf("Erreur lors de l'ouverture du fichier pour écriture.\n");
                close(dSFile);
                return NULL;
            }

            // Réception et écriture des lignes du fichier une par une
            while (1) {
                res = recv(dSFile, &message, sizeof(struct Message), 0);

                if (res == -1) {
                    printf("Erreur lors de la réception de la ligne du fichier.\n");
                    fclose(fp);
                    return NULL;
                } else if (message.taille_msg == 0) {
                    break;
                }
                fprintf(fp, "%s", message.msg);
            }
            fclose(fp); // Fermeture du fichier
        }
        close(dSFile);
    }
    return NULL;
}

/**
 * @brief Fonction exécutée par le thread pour envoyer un fichier au serveur.
 *
 * @param arg Le nom du fichier à envoyer.
 * @return NULL.
 */
void* send_file(void* arg) {
    char* name_file = (char*)arg;
    int dSFile = socket(PF_INET, SOCK_STREAM, 0);
    if (dSFile == -1) {
        perror("Erreur de création de la socket send file : ");
        exit(1);
    }
    printf(BRIGHT_GREEN"Socket Send File Créée\n"RESET);

    // Établissement de la connexion avec le serveur
    struct sockaddr_in aS;
    aS.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(aS.sin_addr));
    aS.sin_port = htons(p_send);

    socklen_t lgA = sizeof(struct sockaddr_in);

    sleep(2);
    int co_res = connect(dSFile, (struct sockaddr*)&aS, lgA);
    if (co_res == -1) {
        printf(RED"Connexion socket send file échouée, veuillez réessayer ultérieurement\n"RESET);
    } else {
        printf(BRIGHT_GREEN"Socket Send File Connectée\n"RESET);

        FILE* fp;
        char path[100];

        strcat(path, chemin_Abs);
        strcat(path, name_file);
        fp = fopen(path, "rb");
        if (fp == NULL) {
            perror("[-]Error in reading file.");
            exit(1);
        }

        char ligne[500];
        Message message;
        strcpy(message.commande, name_file);

        while (fgets(ligne, sizeof(ligne), fp) != NULL) {
            message.taille_msg = strlen(ligne);
            strcpy(message.msg, ligne);

            int send_ret = send(dSFile, &message, sizeof(Message), 0);
            if (send_ret == -1) {
                printf("Erreur lors de l'envoi de fichier pour la ligne %s\n", ligne);
                fclose(fp);
                break;
            }
        }
        printf(YELLOW_GREEN"File envoyé\n"RESET);
        printf(BRIGHT_GREEN"close socket file client\n"RESET);
        close(dSFile);
    }
    printf(BOLD"Entrez un message ('@fin' pour arrêter) : \n"RESET);
    return NULL;
}

/**
 * @brief Vérifie et traite la commande envoyée par le client pour l'envoyer au serveur 
 * et l'enregistre dans message.commande.
 *
 * @param message Le message contenant la commande du client.
 */
void verif_commande_send(Message message) {
    char mess[strlen(message.msg)];
    strcpy(mess, message.msg);
    char* cmd = strtok(mess, " ");

    if (strcmp(cmd, "@leave_chanel\n") == 0) {
        strcpy(message.commande, "@leave_chanel");
        strcpy(message.msg, "@leave_chanel");
    }
    if (strcmp(cmd, "@my_files\n") == 0) {
        my_files();
        return;
    } else if (strcmp(cmd, "@server_files\n") == 0) {
        strcpy(message.commande, "@server_files");
    } else if (strcmp(cmd, "@allCmd\n") == 0) {
        strcpy(message.commande, "@allCmd");
    } else if (strcmp(cmd, "@show_chanels\n") == 0) {
        strcpy(message.commande, "@show_chanels");
    } else if (strcmp(cmd, "@join_chanel") == 0) {
        strcpy(message.commande, "@join_chanel");
        char* numChaine = strtok(NULL, "\n");
        printf("num chaine : %s\n", numChaine);
        strcpy(message.msg, numChaine);
    } else if (strcmp(cmd, "@get_file") == 0) {
        if (nb_file != 100) {
            char* numFile = strtok(NULL, "\n");
            strcpy(message.msg, numFile);
            strcpy(message.commande, "@get_file");

            int sendi = send(dS, &message, sizeof(message), 0);
            if (sendi == -1) {
                perror("Problème lors de l'envoi de la commande @get_file ");
                exit(1);
            }

            pthread_t receive_file_thread;
            if (pthread_create(&receive_file_thread, NULL, recv_file, NULL) != 0) {
                perror("Erreur lors de la création du thread de réception du fichier");
                exit(1);
            }
        } else {
            printf(RED"Votre dossier est limité à 100 fichiers, vous ne pouvez pas recevoir de nouveaux fichiers\n"BOLD);
        }
        return;
    } else if (strcmp(cmd, "@create_chanel") == 0) {
        strcpy(message.commande, "@create_chanel");
    } else if (strcmp(cmd, "@send_file") == 0) {
        char* num_file = strtok(NULL, "\n");
        int numFile = atoi(num_file);
        if (numFile >= 0 && numFile < nb_file) {
            strcpy(message.commande, "@send_file");
            int sendi = send(dS, &message, sizeof(message), 0);
            if (sendi == -1) {
                perror("Erreur lors de l'envoi de la commande @send_file");
                exit(1);
            }
            char* name_file = client_files[numFile];
            pthread_t send_file_thread;
            if (pthread_create(&send_file_thread, NULL, send_file, (void*)name_file) != 0) {
                perror("Erreur lors de la création du thread d'envoi du fichier");
                exit(1);
            }
        } else {
            printf(RED"Le fichier que vous souhaitez envoyer n'existe pas\n"RESET);
            printf(BOLD"Entrez un message ('@fin' pour arrêter) : \n"RESET);
        }
        return;
    } else if (strcmp(cmd, "@fin\n") == 0) {
        printf(RED"Client déconnecté\n"RESET);
        shutdown(dS, 2);
        exit(0);
    } else if (strcmp(cmd, "@printUsers\n") == 0) {
        strcpy(message.commande, "@printUsers");
    } else if (strcmp(cmd, "@mp") == 0) {
        strcpy(message.commande, "@mp");
    } else if (strcmp(cmd, "@delete_file") == 0) {
        strcpy(message.commande, "@delete_file");
    } else {
        strcpy(message.commande, cmd);
    }

    int sendi = send(dS, &message, sizeof(message), 0);
    if (sendi == -1) {
        perror("Erreur lors de l'envoi de la commande");
        exit(1);
    }
    printf(YELLOW_GREEN"Commande envoyée\n"RESET);
}

/**
 * @brief Fonction exécutée par le thread pour envoyer des messages au serveur.
 *
 * @return NULL.
 */
void *send_message() {
    printf(BOLD"Entrez un message ('@fin' pour arrêter) : \n"RESET);
    while (1) {
        // Envoi du message du client
        Message message;
        char msg[1000];
        
        fgets(message.msg, sizeof(message.msg), stdin);

        // On remplit les champs de la structure Message
        message.taille_msg = strlen(message.msg) + 1;
        message.taille_pseudo = strlen(message.pseudo) + 1;
        message.idclient = dS;
        
        if(message.msg[0]=='@'){
          verif_commande_send(message);
        }
        else{
          // On envoie le message au serveur
          int sendi = send(dS, &message, sizeof(message), 0);
          if (sendi == -1) {
              perror ("Erreur lors de l'envoi du message : ");
              exit(1);
          };
          printf(YELLOW_GREEN"Message Envoyé\n"RESET);
          printf(BOLD"Entrez un message ('@fin' pour arrêter) : \n"RESET);
        }
    }
    pthread_exit(NULL);
}

/**
 * @brief Fonction exécutée par le thread pour recevoir des messages du serveur.
 *
 * @return NULL.
 */
void *receive_message() {
    while (1) {
        // Réception du message avec une structure Message
        time_t currentTime;
        struct tm *localTime;
        char timeString[80];

        // Obtenir l'heure actuelle
        currentTime = time(NULL);

        // Convertir l'heure en heure locale
        localTime = localtime(&currentTime);

        // Formater l'heure dans une chaîne de caractères
        strftime(timeString, sizeof(timeString), "%H:%M:%S", localTime);
      
        struct Message message;
        int res = recv(dS, &message, sizeof(struct Message), 0);

        if (res == -1) {
            perror("Erreur lors de la réception du message : ");
            exit(1);
        }

        // Dans le cas où le serveur est fermé
        if (res == 0) {
            printf(RED"Le serveur est fermé, client déconnecté\n"RESET);
            shutdown(dS, 2);
            exit(0);
        }
      
        else if (message.idclient != NMAXCLI && strcmp(message.pseudo, "Server") != 0) {
            // Affiche le message reçu du client
            printf("%s%s  | %s : %s"RESET, colors[message.idclient], timeString, message.pseudo, message.msg);
        }
        else {
            printf(MAGENTA"%s"RESET, message.msg);
        }
        printf(BOLD"Entrez un message ('@fin' pour arrêter) : \n"RESET);
    }
    pthread_exit(NULL);
}

/**
 * @brief Envoie le choix de connexion (connexion ou création de compte) au serveur.
 *        Demande également le login et le mot de passe au client.
 *
 * @return 0 si l'envoi des informations s'est déroulé avec succès, -1 sinon.
 */
int send_choix_connection() {
    char input[10];
    while (strcmp(input, "i\n") != 0 && strcmp(input, "c\n") != 0) {
        printf(BOLD"Voulez vous vous connecter ou créer un nouveau compte ? Entrez c pour connexion, i pour inscription \n"RESET);
        fgets(input, sizeof(input), stdin);
    }
    char* choix = strtok(input, "\n");

    int nb_param;
    char params[50];
    char* param1;
    char* param2;
    while (nb_param != 2) {
        nb_param = 0;
        printf(BOLD"Saisissez votre login et mot de passe, séparés avec un espace\n"RESET);
        fgets(params, sizeof(params), stdin);
        param1 = strtok(params, " ");
        if (sizeof(param1) != 0) {
            nb_param++;
        }
        param2 = strtok(NULL, "\n");
        if (sizeof(param2) != 0) {
            nb_param++;
        }
    }
    char mes[50] = {0};
    strcat(mes, choix);
    strcat(mes, " ");
    strcat(mes, param1);
    strcat(mes, " ");
    strcat(mes, param2);
    int tmes = strlen(mes) + 1;
    int send_taille = send(dS, &tmes, 4, 0);
    if (send_taille == -1 ) {
        return -1;
    };
    int send_infos = send(dS, mes, tmes, 0);
    if (send_infos == -1 ) {
        return -1;
    };

    printf(YELLOW_GREEN"Login et mot de passe envoyés\n"RESET);
    return 0;
}

int main(int argc, char *argv[]) {

    //stocker file client in tab
    DIR *dir = opendir(chemin_Abs);
    if (dir == NULL) {
        perror("Erreur d'ouverture du repertoire");
        exit(EXIT_FAILURE);
    }
    // Lit chaque entrée du répertoire
    struct dirent *entry;
    
    while ((entry = readdir(dir)) != NULL) {
        // Ignore les entrées spéciales
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".DS_Store") == 0) {
            continue;
        }
        // Vérifie si l'entrée est un fichier régulier
        if (entry->d_type == DT_REG) {
            strcpy(client_files[nb_file], entry->d_name);
            nb_file++;
        }
    }

    ip = argv[1];

    // crée la socket
    printf(BRIGHT_GREEN "Début programme\n"RESET);
    dS = socket(PF_INET, SOCK_STREAM, 0);
    if (dS == -1) {
        perror("Erreur lors de la création de la socket : ");
        exit(1);
    };
    printf(BRIGHT_GREEN"Socket Créée\n"RESET);

    // fait le lien avec le serveur
    struct sockaddr_in aS;
    aS.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &(aS.sin_addr));
    aS.sin_port = htons(atoi(argv[2]));
    socklen_t lgA = sizeof(struct sockaddr_in);
    int co_res = connect(dS, (struct sockaddr *) &aS, lgA);
    if (co_res == -1) {
        perror("Erreur lors de la connexion : ");
        exit(1);
    };

    printf(BRIGHT_GREEN"Socket Connectée\n"RESET);

    // réception du port de réception des fichiers (qui est p_send du côté serveur)
    recv(dS, &p_recv, 4, 0);

    // réception du port d'envoi des fichiers (qui est p_recv du côté serveur)
    recv(dS, &p_send, 4, 0);

    int code_connection = 0;
    do {
        if (code_connection == 500) {
            printf("%s\n", RED"Le login ou le mot de passe n'est pas valide"RESET);
        }
        send_choix_connection();
        int res_t = recv(dS, &code_connection, 4, 0);
        printf("code recu %d\n", code_connection);
    } while (code_connection != 200);

    int code_pseudo = 0;
    do {
        send_pseudo();
        int res_t = recv(dS, &code_pseudo, 4, 0);
    } while (code_pseudo != 200);

    pthread_t send_thread;
    pthread_t receive_thread;

    if (pthread_create(&send_thread, NULL, send_message, NULL) != 0) {
        perror("Erreur lors de la création du thread d'envoi de message");
        exit(1);
    }

    if (pthread_create(&receive_thread, NULL, receive_message, NULL) != 0) {
        perror("Erreur lors de la création du thread de réception de message");
        exit(1);
    }

    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);
}