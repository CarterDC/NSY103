     _______      __        __  __  ___           __           
    /_  __(_)____/ /_____  / /_/  |/  /___ ______/ /____  _____
     / / / / ___/ //_/ _ \/ __/ /|_/ / __ `/ ___/ __/ _ \/ ___/
    / / / / /__/ ,< /  __/ /_/ /  / / /_/ (__  ) /_/  __/ /    
   /_/ /_/\___/_/|_|\___/\__/_/  /_/\__,_/____/\__/\___/_/     
                                                                                                                                       
=============================================================================

Objectifs :
----------------------
Consultation et réservation de tickets pour des spectacles (TicketMaster) :
Demonstration de concepts d'IPC sur linux
par le biais d'applications clients/server en C

Compilation :
----------------------
 Un script bash 'compile_and_run.sh' est présent dans chaque répertoire afin
 de compiler (avec option Werror) les fichiers sources
 et de lancer les executables résultants de cette conpilation.

Utilisation :
-------------

Après compilation lancer les programmes dans des fenetre de terminal séparées
$ ./server 
$ ./client 
l'ordre d'exécution n'a pas d'importance.

Contenu :
---------

coirier_romain\ : Racine du Projet
|
|-question1\ : Résolution du projet avec utilisation de processus légers
|  |-common.h : source du header commun au client et au serveur
|  |-client.c : source du client
|  |-server.c : source du serveur
|  |-compile_and_run.sh : script bash pour compiler et lancer les 2 executables
|
|-question2\ : Résolution du projet avec utilisation de processus lourds
|  |-common.h : source du header commun au client et au serveur
|  |-client.c : source du client
|  |-server.c : source du serveur
|  |-compile_and_run.sh : script bash pour compiler et lancer les 2 executables
|
|-rapport.pdf : Rapport explicatif du projet
|
|-README.MD : Le présent fichier

Crédits :
---------

Romain COIRIER
email : romain.coirier.auditeur.lecnam.net
