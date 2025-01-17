/*******************************************************************************
 * @file server_consult.c
 * @brief Implémentation du serveur de consultation de la question 1.
 * @author Romain COIRIER
 * @date 09/01/2025
 * @version 1.0
 * 
 * cf common.h
 * Ce serveur accepte des connections (TCP) 
 * 
 * 
 * @note .
 * 
 * @bug .
 ******************************************************************************/

#include "common.h"

#include <pthread.h>
#include <sys/sem.h>
#include <time.h>

// variables globales
int listen_sock_fd; //descripteur du socket d'écoute TCP
int semset_id; // l'identifiant du tableau des sméphores System V
int nb_readers; // nb de lecteurs qui accèdent notre tableau à un instant t

Message *shows; // pointeur vers le futur tableau partagé

//Prototypes
void sigint_handler(int sig);

void setupSignalHandlers();
void setupSemaphoreSet(key_t key);
void populateResource();
int getNbShows();
void setupListeningSocket();
void initServer();

void bookSeats(Message *msg);
void getNbSeats(Message *msg);

void* workerThread(void* arg) {
    int service_sock_fd = *(int*)arg;
    int nb_bytes;
    Message s2c_buf, c2s_buf; //structs des messages server vers client et retour

    printf("Demarrage thread.\n");
    // attente de la requete de la requête
    nb_bytes = recv(service_sock_fd, &c2s_buf, sizeof(c2s_buf),0); //TODO check nb_bytes ? 
    
    //préparationde la réponse
    strncpy(s2c_buf.show_id, c2s_buf.show_id, SHOW_ID_LEN);
    if (c2s_buf.nb_seats == 0) {
        //requete de consult
        printf("Requete de Consultation pour le spectacle %s.\n", c2s_buf.show_id);
        getNbSeats(&s2c_buf);
    } else {
        //requete de résa
        printf("Requete de Reservation de %d places pour le spectacle %s.\n", c2s_buf.nb_seats, c2s_buf.show_id);
        s2c_buf.nb_seats = c2s_buf.nb_seats;
        bookSeats(&s2c_buf);
    }
    //renvoi de la réponse 
    nb_bytes = send(service_sock_fd, &s2c_buf, sizeof(s2c_buf),0); //TODO check nb_bytes ? 

    printf("Fermeture socket de service.\n");
    close(service_sock_fd);

    pthread_exit(NULL);
}

int main(void){

    printf("PROJET NSY103 - QUESTION 1.\n");
    printf("Serveur.\n");
    printf("===========================\n");

    int return_value;
    struct sockaddr client_addr; // adresse du client 
    int addr_len = sizeof(client_addr);
    
    //mise en place des sémaphores, de la ressource paratgée et du socket d'écoute
    //mise en palce de la message queue et des threads
    initServer();

    listen(listen_sock_fd, 10); // Queue de 10 demandes
    printf("Serveur en attente de requetes reservation ou consultation...\n");

    while(1) {
        //créa d'un socket de service à l'acceptation de la connexion
        int service_sock_fd = accept(listen_sock_fd, &client_addr, &addr_len);
   
        pthread_t thread;
        pthread_create(&thread, NULL, workerThread,(void *)&service_sock_fd);
        pthread_detach(thread); 
    }
}

// handler pour signal SIGINT
void sigint_handler(int sig) {

    printf("\n");
    printf("Fermeture du socket.\n");
    close(listen_sock_fd);

    printf("Suppression du semaphore.\n");
    semctl(semset_id, 0, IPC_RMID, 0);

    printf("Liberation de la memoire.\n");
    free(shows);
    
    printf("Au revoir.\n");
    exit(EXIT_SUCCESS);
}

void setupSignalHandlers() {
    //mise en place du handler d'interruption de l'exécution
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    if(sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Erreur sigaction.\n");
        exit(EXIT_FAILURE);
    }
    printf("'Ctrl + c' pour mettre fin au programme.\n");
}

void initServer()
{
    srand(time(NULL)); // reset de la seed pour le nb de places aléatoire

    // mise en place du handler d'interruption de l'exécution
    setupSignalHandlers();

    // Génération de la clé pour le sémaphore
    key_t key = ftok(KEY_FILENAME, KEY_ID);
    /*if (key == -1) {
        perror("Echec creation de la clef.\n");
        exit(EXIT_FAILURE);
    }*/

    // mise en place du tableau des sémaphores
    setupSemaphoreSet(key);
    //mise en place d'une message queue pour déposer les requetes (les ids de service socket)

    //allocation et remplissage du tableau des spectacles
    shows = (Message *) malloc((getNbShows() + 1) * sizeof(Message));
    populateResource();

    //mise en place du socket
    setupListeningSocket();

}

void setupSemaphoreSet(key_t key) {
    // Création ou récupération (si déjà créé) d'un tableau de 3 semaphores
    if ((semset_id = semget(key, 3, IPC_CREAT | IPC_EXCL | 0666)) == -1)
    {
        if (errno == EEXIST)
        {
            //le tableau de semaphore existe déjà, on le récupère
            semset_id = semget(key, 3, 0666);
        }
        else
        {
            perror("Echec creation du semaphore.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }    
    // Initialisation des semaphores (tous init à 1)
    semctl(semset_id, NB_READERS_MUTEX, SETVAL, 1); //protection de nb_readers en exclusion mutuelle
    semctl(semset_id, QUEUE_SEM, SETVAL, 1); //queue de service pour l'équité d'accès
    semctl(semset_id, RESOURCE_SEM, SETVAL, 1); //protection de la resource partagée (shows[])
}

/**
 * @brief Remplit la resource partagée shows[] avec les données des spectacle
 * 
 * l'accès en écriture à la ressource est protégé par un jeu de sémaphores
 * en appliquant une synchronisation de type lecteurs/rédacteur avec file d'attente
 * 
 * @note le nombre de places est décidé au hasard entre 16 et 30
 * un indicateur de fin de tableau est signifié par tous les bits de la structure à 0
 */
void populateResource()
{
    //accès en écriture sur la ressource partagée => on protège par sémaphores
    struct sembuf operations[3];

    // prélude
    operations[QUEUE_SEM].sem_num = 0;
    operations[QUEUE_SEM].sem_op = -1; // ServiceQueue.P()
    operations[RESOURCE_SEM].sem_num = 0;
    operations[RESOURCE_SEM].sem_op = -1; // Ressource.P()
    operations[QUEUE_SEM].sem_num = 0;
    operations[QUEUE_SEM].sem_op = 1; // ServiceQueue.V()
    semop(semset_id, operations, 3);

    // Entrée en section critique
        // instanciation du tableau des spectacles
        int i = 0;
        while (SHOW_IDS[i] != NULL)
        {
            strncpy(shows[i].show_id, SHOW_IDS[i], SHOW_ID_LEN);
            shows[i].nb_seats = 16 + rand() % 15;
            i++;
        }
        // terminaison du tableau
        memset(&shows[++i], 0, sizeof(Message));
    // Sortie de section critique

    // postlude
    operations[RESOURCE_SEM].sem_num = 0;
    operations[RESOURCE_SEM].sem_op = 1; // Ressource.V()
    semop(semset_id, operations, 1);
}


int getNbShows() {
    int i = 0;
    // on compte la taille du tableau des noms de spectacles
    while(SHOW_IDS[i] != NULL) {
        i++;
    }    // et on multiple par la taille d'un message
    return i;
}

void setupListeningSocket() {
   int return_value;
    struct addrinfo hints, *server_info;

    //création de l'address info du server (nous): server_info
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // utilise IPV4 ou IPV6
    hints.ai_socktype = SOCK_STREAM; //TCP
    if((return_value = getaddrinfo(SERVER_IP, SERVER_PORT, &hints, &server_info)) != 0) {
        perror("Erreur creation adresse server \n");
        exit(EXIT_FAILURE);
    }

    //création du socket et association à notre adresse .
    listen_sock_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if(listen_sock_fd == -1) {
        perror("Erreur creation du socket d ecoute\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if((return_value = bind(listen_sock_fd, server_info->ai_addr, server_info->ai_addrlen)) != 0) {
        perror("Erreur association du socket d ecoute\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        printf("Fermeture du socket.\n");
        close(listen_sock_fd);
        exit(EXIT_FAILURE);
    }
}

void getNbSeats(Message *msg) {
    struct sembuf operations[2];
    // recherche de l'index du spectacle
    bool found = false;
    int i = -1;
    while(!found && (shows[++i].show_id[0] != '\0')) {
        found = strcmp(msg->show_id, shows[i].show_id) == 0;
    }
    if(!found) {
        //le spectacle demandé n'a pas été trouvé dans la liste
        //on met tous les bits du message à 0 pour le signifier
        memset(msg, 0, sizeof(Message));
        return;
    }
    
    //accès en écriture sur la ressource partagée => on protège par sémaphores

    // prélude
    operations[0].sem_num = QUEUE_SEM;
    operations[0].sem_op = -1; // ServiceQueue.P()
    operations[1].sem_num = NB_READERS_MUTEX;
    operations[1].sem_op = -1; // nb_readers.P()
    semop(semset_id, operations, 2);
        //mini section critique
        nb_readers++;
        if(nb_readers == 1) {
            //on est le premier lecteur sur la ressource
            operations[0].sem_num = RESOURCE_SEM;
            operations[0].sem_op = -1; // Ressource.P()
            semop(semset_id, operations, 1);        
        }
    operations[0].sem_num = QUEUE_SEM;
    operations[0].sem_op = 1; // ServiceQueue.V()
    operations[1].sem_num = NB_READERS_MUTEX;
    operations[1].sem_op = 1; // nb_readers.V()
    semop(semset_id, operations, 2);

    // Entrée en section critique

        msg->nb_seats = shows[i].nb_seats;

    // Sortie de section critique

    // postlude
    operations[0].sem_num = NB_READERS_MUTEX;
    operations[0].sem_op = -1; // nb_readers.P()
    semop(semset_id, operations, 1);
        //mini section critique
        nb_readers--;
        if(nb_readers == 0) {
            //on était le dernier lecteur sur la ressource
            operations[0].sem_num = RESOURCE_SEM;
            operations[0].sem_op = 1; // Ressource.V()
            semop(semset_id, operations, 1);       
        }
    operations[0].sem_num = NB_READERS_MUTEX;
    operations[0].sem_op = 1; // nb_readers.V()
    semop(semset_id, operations, 1);
}

void bookSeats(Message *msg)
{
    struct sembuf operations[3];
    // recherche de l'index du spectacle
    bool found = false;
    int i = -1;
    while (!found && (shows[++i].show_id[0] != '\0'))
    {
        found = strcmp(msg->show_id, shows[i].show_id) == 0;
    }
    if (!found)
    {
        // le spectacle demandé n'a pas été trouvé dans la liste
        // on met tous les bits du message à 0 pour le signifier
        memset(msg, 0, sizeof(Message));
        return;
    }

    //accès en écriture sur la ressource partagée => on protège par sémaphores

    // prélude
    operations[0].sem_num = QUEUE_SEM;
    operations[0].sem_op = -1; // ServiceQueue.P()
    operations[1].sem_num = RESOURCE_SEM;
    operations[1].sem_op = -1; // Ressource.P()
    operations[2].sem_num = QUEUE_SEM;
    operations[2].sem_op = 1; // ServiceQueue.V()
    semop(semset_id, operations, 3);

    // Entrée en section critique
    if (msg->nb_seats <= shows[i].nb_seats)
    {
        // il reste assez de places
        printf("Il reste %d et on demande %d", shows[i].nb_seats, msg->nb_seats);
        shows[i].nb_seats = shows[i].nb_seats - msg->nb_seats;
    }
    else
    {
        // il ne reste pas assez de places pour honorer la réservation entière
        msg->nb_seats = -1 * shows[i].nb_seats;
    }
    // Sortie de section critique

    // postlude
    operations[0].sem_num = RESOURCE_SEM;
    operations[0].sem_op = 1; // Ressource.V()
    semop(semset_id, operations, 1);
}

