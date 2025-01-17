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
char *process_name; // pour identifier les 2 serveurs dans le terminal
int msg_queue_id; // l'identifiant de la file de messages System V
int semset_id; // l'identifiant du tableau des sméphores System V
int nb_readers; // nb de lecteurs qui accèdent notre tableau à un instant t

Message *shows; // pointeur vers le futur tableau partagé

//Prototypes
void sigint_handler(int sig);

void setupSignalHandlers();
void setupSemaphoreSet(key_t key);
void populateResource();
int getNbShows();
void setupMsgQueue(key_t key);
void initServer();

void bookSeats(Message *msg);
void getNbSeats(Message *msg);

void* consultation(void* arg) {
    Request msg_req = *(Request*)arg; //on recaste l'argument dans une struct Requete
    Response msg_resp;
    int return_value;

    printf("Demarrage thread consultation.\n");    
    //préparation de la réponse
    msg_resp.msg_type = msg_req.pid; //pid du client pour récupération par le process adéquat
    strncpy(msg_resp.msg.show_id, msg_req.msg.show_id, SHOW_ID_LEN);
    getNbSeats(&msg_resp.msg);
    // envoi de la réponse
    if ((return_value = msgsnd(msg_queue_id, &msg_resp,
        sizeof(Response) - sizeof(long), 0)) == -1)
    {
        perror("Echec msgsnd.\n");
        exit(EXIT_FAILURE);
    }

    pthread_exit(NULL);
}

void* reservation(void* arg) {
    Request msg_req = *(Request*)arg; //on recaste l'argument dans une struct Requete
    Response msg_resp;
    int return_value;

    printf("Demarrage thread reservation.\n");    
    //préparation de la réponse
    msg_resp.msg_type = msg_req.pid;
    msg_resp.msg = msg_req.msg;
    bookSeats(&msg_resp.msg);
    // envoi de la réponse
    if ((return_value = msgsnd(msg_queue_id, &msg_resp, sizeof(Response) - sizeof(long), 0)) == -1)
    {
        perror("Echec msgsnd.\n");
        exit(EXIT_FAILURE);
    }

    pthread_exit(NULL);
}

int main(void){

    printf("PROJET NSY103 - QUESTION 1.\n");
    printf("Serveur.\n");
    printf("===========================\n");

    int return_value;
    Request msg_req;
    
    //mise en place des sémaphores, de la ressource paratgée et de la queue
    initServer();

    while(1) {
        printf("Serveur en attente de requetes reservation ou consultation...\n");
        // attente de la réception d'une requete
        if ((return_value = msgrcv(msg_queue_id, &msg_req,
            sizeof(Request) - sizeof(long), MESSAGE_TYPE, 0)) == -1)
        {
            perror("Echec msgrcv.\n");
            exit(EXIT_FAILURE);
        }

        // création d'un thread spécifique pour traiter la requete client
        pthread_t thread;
        if(msg_req.msg.nb_seats == 0) {
            printf("Requete de Consultation pour le spectacle %s.\n",
             msg_req.msg.show_id);
            pthread_create(&thread, NULL, consultation,(void *)&msg_req);
        } else {
            printf("Requete de Reservation de %d places pour le spectacle %s.\n",
             msg_req.msg.nb_seats, msg_req.msg.show_id);
            pthread_create(&thread, NULL, reservation,(void *)&msg_req);
        }
        // on ne pourra pas resynchro avec le thread a l'aide d'un join
        // donc on détache et on laisse le Système d'Exploitation gérer ça
        pthread_detach(thread); 
    }
}

// handler pour signal SIGINT
void sigint_handler(int sig) {

    printf("\n");
    printf("Suppression de la file de messages.\n");
    msgctl(msg_queue_id, IPC_RMID, NULL);

    printf("Suppression des semaphores.\n");
    semctl(semset_id, NB_READERS_MUTEX, IPC_RMID, 0); //suprimer 1 les supprime tous

    printf("Liberation de la memoire.\n");
    free(shows);
    
    printf("Au revoir.\n");
    exit(EXIT_SUCCESS);
}

/**
 * @brief Mets en place les handlers de signaux
 * 
 * Déclare un handler pour le signal d'interruption
 * 
 */
void setupSignalHandlers() {
    //mise en place du handler d'interruption de l'exécution
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
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
    // mise en place du tableau des sémaphores
    setupSemaphoreSet(key);
    // Création / récupération de la message queue
    setupMsgQueue(key);

    //allocation et remplissage du tableau des spectacles
    shows = (Message *) malloc((getNbShows() + 1) * sizeof(Message));
    populateResource();

}

void setupSemaphoreSet(key_t key) {
    // Création ou récupération (si déjà créé) d'un tableau de 3 semaphores
    if ((semset_id = semget(key, 3, IPC_CREAT | IPC_EXCL | 0666)) == -1)
    {
        if (errno == EEXIST)
        {
            //le tableau de semaphore existe déjà, on le récupère
            printf("Recuperation du tableau des semaphores.\n");
            semset_id = semget(key, 3, 0666);
        }
        else
        {
            perror("Creation du tableau des semaphores : Echec.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Creation du tableau des semaphores : Succes.\n");
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
    printf("Remplissage de la ressource.\n");
    //accès en écriture sur la ressource partagée => on protège par sémaphores
    struct sembuf operations[3];

    // prélude
    operations[0].sem_num = QUEUE_SEM;
    operations[0].sem_op = -1; // ServiceQueue.P()
    operations[1].sem_num = RESOURCE_SEM;
    operations[1].sem_op = -1; // Ressource.P()
    operations[2].sem_num = QUEUE_SEM;
    operations[2].sem_op = 1; // ServiceQueue.V()
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
    operations[0].sem_num = RESOURCE_SEM;
    operations[0].sem_op = 1; // Ressource.V()
    semop(semset_id, operations, 1);
}


/**
 * @brief Renvoie le nombre d'entrée du tableau des identifiants de spectacles
 * défini dans le ficheir de header
 * 
 * note : On utilise NULL pour signifier la fin des entrées
 *
 * @return int : la longeur du tableau
 */
int getNbShows()
{
    int i = 0;
    // on compte la taille du tableau des noms de spectacles
    while (SHOW_IDS[i] != NULL)
    {
        i++;
    }
    return i;
}

/**
 * @brief Crée ou récupère une message Queue avec la clé passée en param.
 *
 * @param key La clé IPC utilisée pour identifier la file de messages.
 */
void setupMsgQueue(key_t key) {
    // Tente de créer une file de message avec la clef donnée
    msg_queue_id = msgget(key, 0666 | IPC_CREAT | IPC_EXCL);
    if (msg_queue_id == -1) {
        // Si la file de messages existe déjà (errno == EEXIST), tente de l'ouvrir
        if (errno == EEXIST) {
            printf("Recuperation de la file de messages.\n");
            msg_queue_id = msgget(key, 0666);            
        } else {
            perror("Creation de la file de messages : Echec.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Creation de la file de messages : Succes.\n");
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

