/*******************************************************************************
 * @file server.c
 * @brief Implémentation du serveur de consultation/réservation de la question 2.
 * @author Romain COIRIER
 * @date 07/01/2025
 * @version 1.0
 * 
 * cf common.h
 * Ce serveur fork 1 process lourd fils pour gérer les consultations de façon séquentielle
 * Le père gère les réservation de façon parallèle en créant au autre fils pour chaque requête.
 * 
 * 
 * @note Chaque process fils accède au segment de mémoire partagée (table des spectacles) 
 * en lecture ou écriture, selon les règles de synchronisation de l'exclusiion mutuelle. 
 * 
 * @bug .
 ******************************************************************************/

#include "common.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

// variables globales
int msg_queue_id;
int sharedmem_id;
int semid;
Message *shows; // pointeur vers le futur tableau partagé

//Prototypes
void sigint_handler(int sig);
void setupSignalHandlers();
void setupSharedMem(key_t key);
void populateSharedMem();
int getNbShows();
void setupMsgQueue(key_t key);
void initServer();
void getNbSeats(Message *msg);

int main(void){

    printf("PROJET NSY103 - QUESTION 2.\n");
    printf("Serveur.\n");
    printf("===========================\n");

    // variables
    pid_t pid;
    int return_value;
    Request msg_req;
    Response msg_resp;

    //mise en place du segment partagé, du sémaphore binaire et de la msgQueue
    initServer();

    //séparation du serveur en 2 processus lourds
    pid = fork();
    if(pid == 0) {
        //processus fils en charge des consultations
        while(1){
            // on se met en attente d'un message de type REQUEST_CONSULT
            printf("Serveur de consultation en attente de requetes...\n");
            if((return_value = msgrcv(msg_queue_id, &msg_req, sizeof(Request) - sizeof(long), REQUEST_CONSULT, 0)) == -1) {
                perror("Echec msgrcv.\n");
                exit(EXIT_FAILURE);
            }
            
            printf("SC : Requete de Consultation pour le spectacle %s.\n", msg_req.msg.show_id);
            msg_resp.msg_type = msg_req.pid;
            strncpy(msg_resp.msg.show_id, msg_req.msg.show_id, SHOW_ID_LEN);
            getNbSeats(&msg_resp.msg);
            // envoi de la réponse
            if((return_value = msgsnd(msg_queue_id, &msg_resp, sizeof(Response) - sizeof(long), 0)) == -1) {
                perror("Echec msgsnd.\n");
                exit(EXIT_FAILURE);
            }            
        }
    } else {
        //process père en charge des réservations
        printf("Serveur de reservation en attente de requetes...\n");
        while(1){
            // on se met en attente d'un message de type REQUEST_RESA            
            if((return_value = msgrcv(msg_queue_id, &msg_req, sizeof(Request) - sizeof(long), REQUEST_RESA, 0)) == -1) {
                perror("Echec msgrcv.\n");
                exit(EXIT_FAILURE);
            }
            pid = fork();
            if(pid == 0) {
                //process fils
                printf("Requete de Reservation de %d places pour le spectacle %s.\n", msg_req.msg.nb_seats, msg_req.msg.show_id);
                msg_resp.msg_type = msg_req.pid;
                strncpy(msg_resp.msg.show_id, msg_req.msg.show_id, SHOW_ID_LEN);

                getNbSeats(&msg_resp.msg);
                // envoi de la réponse
                if((return_value = msgsnd(msg_queue_id, &msg_resp, sizeof(Response) - sizeof(long), 0)) == -1) {
                    perror("Echec msgsnd.\n");
                    exit(EXIT_FAILURE);
                }
                // le process fils a traité la requete, il se termine proprement
                printf("Détachement du segment de mémoire partagé.\n");
                shmdt(shows);
                exit(EXIT_SUCCESS);
            }
            // le process père continue d'attendre des requetes dans la queue.          
        }
    }
}

void initServer() {
    srand(time(NULL)); // reset de la seed pour le nb de places aléatoire

    //mise en place du handler d'interruption de l'exécution
    setupSignalHandlers();

    // Génération de la clé pour la mémoire partagée et le sémaphore
    key_t key = ftok(KEY_FILENAME, KEY_ID);
    //mise en place / récupération du segment de mémoire partagé
    setupSharedMem(key);
    //Création ou récupération (si déjà créé) d'un tableau de 1 semaphore
    if((semid = semget(key, 1, IPC_CREAT | IPC_EXCL| 0666)) == -1){
        if( errno == EEXIST) {
            semid = semget(key, 1, 0666 );
        } else {
            perror("Echec creation du semaphore.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);            
        }
    }
    //Initialisation du semaphore (index 0, valeur d'init : 1)
    semctl(semid, 0, SETVAL, 1);

    //Création / récupération de la message queue
    setupMsgQueue(key);

    setbuf(stdout, NULL);
    printf("'Ctrl + c' pour mettre fin aux programmes.\n");
}

// handler pour signal SIGINT
void sigint_handler(int sig) {

    printf("\n");
    printf("Fermeture de la queue.\n");
    msgctl(msg_queue_id, IPC_RMID, NULL);
    printf("Suppression du semaphore.\n");
    semctl(semid, 0, IPC_RMID, 0);
    printf("Détachement du segment de mémoire partagé.\n");
    shmdt(shows);
    printf("Suppression du segment partagé.\n");
    shmctl(sharedmem_id, IPC_RMID, NULL);
    
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

    //pas de handler pour sigchld : on ignore
    signal(SIGCHLD, SIG_IGN);    
}

void setupSharedMem(key_t key){
    //mise en place du segment de mémoire partagée
    size_t shm_size;
    shm_size = (getNbShows() + 1) * sizeof(Message);

    // récupération du segment de mémoire partagée
    if((sharedmem_id = shmget(key, shm_size, 0666)) != 0) {
        if(errno == ENOENT) {
            //le segment n'existe pas encore, => on le crée
            printf("Creation du segment de memoire partage.\n");
            sharedmem_id = shmget(key, shm_size, 0666 | IPC_CREAT);
            //attachement du segment créé à l'espace d'adressage du process
            if ((shows = (Message *)shmat(sharedmem_id, NULL, 0)) == (Message *) -1) {
                perror("Erreur lors de l attachement a la memoire partagee");
                exit(EXIT_FAILURE);
            }
            //instanciation du tableau des spectacles
            populateSharedMem();

        } else {
            perror("Erreur creation du segment de memoire partage.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Recuperation du segemnt de memoire partage.\n");
    }
}

void populateSharedMem(){
    //TODO : protection de la resource par le mutex ? 

    //instanciation du tableau des spectacles
    int i = 0;
    while(SHOW_IDS[i] != NULL) {
        strncpy(shows[i].show_id, SHOW_IDS[i], SHOW_ID_LEN);
        shows[i].nb_seats = 16 + rand() % 15;
        i++;
    }
    memset(&shows[++i], 0, sizeof(Message));

    //vérification du tableau
    i = 0;
    while(shows[i].show_id[0] != '\0') {
        printf("%d) %s : %d places.\n", i, shows[i].show_id, shows[i].nb_seats);
        i++;
    }
}

int getNbShows() {
    int i = 0;
    // on compte la taille du tableau des noms de spectacles
    while(SHOW_IDS[i] != NULL) {
        i++;
    }    // et on multiple par la taille d'un message
    return i;
}

void setupMsgQueue(key_t key) {
    msg_queue_id = msgget(key, 0666 | IPC_CREAT | IPC_EXCL);
    if(msg_queue_id == -1) {
        if( errno == EEXIST) {
            msg_queue_id = msgget(key, 0666 );
        } else {
            perror("Echec creation de la message queue.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);            
        }
    }
}

void getNbSeats(Message *msg) {
    struct sembuf operations[1];
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
    
    //accès en lecture au segment partagé

    //prélude
    operations[0].sem_num = 0;
    operations[0].sem_op = -1; //P()
    semop(semid, operations, 1);
    //section critique
    msg->nb_seats = shows[i].nb_seats;
    //postlude
    operations[0].sem_num = 0;
    operations[0].sem_op = 1; //V()
    semop(semid, operations, 1);    
    return;
}
