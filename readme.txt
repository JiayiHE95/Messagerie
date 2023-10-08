FAR - Projet Messagerie

Ce projet vise à créer une application de messagerie en langage C permettant aux utilisateurs de se connecter à un serveur, d'échanger des messages avec d'autres clients connectés et de se déconnecter de manière contrôlée.


#######Auteurs#######

Amel ADDOU & Jiayi HE & Ines AMZERT (IG3-TD2)


#######Lancer le serveur#######

Ouvrez un terminal, puis exécuter les commandes suivantes : 
➙ Compiler : gcc server.c -o server -lpthread
➙ Exécuter : ./server le-port-du-serveur (>1024)


#######Lancer le(s) Client(s)#######

On peut lancer le programme client sur les différents terminaux pour imiter la connexion de plusieurs clients, en suivant les commandes ci-dessus : 
➙ Compiler : gcc client.c  -o client -lpthread
➙ Exécuter : ./client votre-adresse-ip le-port-du-serveur

Pour le rappel, on peut récupérer l’adresse ip avec la commande hostname -I


#######Organisation des fichiers et dossiers#######

L'application se compose de deux fichiers principaux qui contiennent du code permettant de lancer la messagerie : 

➙ server.c et client.c. 

D’autres fichiers et dossiers suivants sont nécessaires pour le bon fonctionnement de l'application :

➙ Dossier server_files : contient les fichiers du serveur et les fichiers que les clients lui ont envoyés.
➙ Dossier client_files : contient les fichiers du client.
➙ Dossier chaines_files : contient les informations des salon (le numéro, le nom, la capacité d’accueil et le thème), un salon par fichier
➙ Fichier allCmd.txt : le manuel utilisateur de l’application que le serveur va potentiellement l’envoyer au client
➙ Fichier clients.txt : contient les logins et les mot de passe des clients inscrits


#######Liste des commandes disponibles#######
 
Veuillez consulter le Manuel Utilisateur
