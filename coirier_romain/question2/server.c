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
 * L'initialisation du server se fait après le fork
 * Les requêtes sont extraites d'une file de messages
 *
 *
 * @note Chaque process fils attache individuellement le segment de mémoire partagée (table des spectacles)
 * La ressource critique est alors accédée en lecture ou écriture, selon les règles de synchronisation de l'exclusiion mutuelle.
 *
 * @bug : En cas d'erreurs (exit(EXIT_FAILURE)), les ressources ne sont pas toujours libérées correctement,
 * aussi il arrive de devoir relancer le server et de le fermer avant de récupérer un fonctionnement normal.
 ******************************************************************************/

#include "common.h"

#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h> // uniquement pour la génération aléatoire de nb de places

// variables globales
char process_name[30]; // pour identifier les 2 serveurs dans le terminal
int msg_queue_id; // l'identifiant de la file de messages System V
int sharedmem_id; // l'identifiant du segment de mémoire partagé
int semset_id;    // l'identifiant du tableau de sémaphore System V
Message *shows;   // pointeur vers le futur tableau partagé

// Prototypes
void sigint_handler(int sig);

void setupSignalHandlers();
void setupSemaphoreSet(key_t key);
void setupSharedMem(key_t key);
void populateResource();
int getNbShows();
void setupMsgQueue(key_t key);
void initServer(key_t key);

void getNbSeats(Message *msg); // consultation
void bookSeats(Message *msg);  // réservation

/**
 * @brief Crée deux process séparés, un serveur de consultation itératif et un 
 * serveur de réservation parallèle
 *
 * Chaque serveur est initialisé séparément 
 * et chaque process est attaché individuellement au segment de mémoire partagé 
 *
 */
int main(void)
{
    printf("PROJET NSY103 - QUESTION 2.\n");
    printf("Serveur.\n");
    printf("===========================\n");

    pid_t pid; // pour différencier les processes après un fork()
    int return_value;
    Request msg_req;
    Response msg_resp;

    // Génération de la clé pour la mémoire partagée et le sémaphore
    key_t key = ftok(KEY_FILENAME, KEY_ID);

    // séparation du serveur en 2 processus lourds
    pid = fork();
    if (pid == 0)
    {
        // processus fils en charge des consultations (mode séquentiel)
        strcpy(process_name,"Serveur de consultation");

        // mise en place des gestionnaires de signaux,
        // sémaphore bianire, mémoire partagée et file de messages
        initServer(key);
        
        while (1)
        {
            // on se met en attente d'un message de type REQUEST_CONSULT
            printf("%s : en attente de requetes...\n", process_name);
            if ((return_value = msgrcv(msg_queue_id, &msg_req,
             sizeof(Request) - sizeof(long), REQUEST_CONSULT, 0)) == -1)
            {
                perror("Echec msgrcv.\n");
                exit(EXIT_FAILURE);
            }
            printf("Requete de Consultation pour le spectacle %s.\n", msg_req.msg.show_id);

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
        }
    }
    else
    {
        // process père en charge des réservations
        strcpy(process_name,"Serveur de reservation");
        // mise en place des gestionnaires de signaux,
        // sémaphore bianire, mémoire partagée et file de messages
        initServer(key);
        
        while (1)
        {
            // on se met en attente d'un message de type REQUEST_RESA
            printf("%s : en attente de requetes...\n", process_name);
            if ((return_value = msgrcv(msg_queue_id, &msg_req, sizeof(Request) - sizeof(long), REQUEST_RESA, 0)) == -1)
            {
                perror("Echec msgrcv.\n");
                exit(EXIT_FAILURE);
            }
            pid = fork();
            if (pid == 0)
            {
                // process fils
                sprintf(process_name, "Serveur de reservation N%d", getpid());
                printf("%s : Requete de Reservation de %d places pour le spectacle %s.\n", process_name, msg_req.msg.nb_seats, msg_req.msg.show_id);

                //Récup et attachement du segment paratagé
                setupSharedMem(key);

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

                exit(EXIT_SUCCESS);
            }
            // le process père continue d'attendre des requetes dans la queue.
        }
    }
}

/**
 * @brief Gère le signal d'interruption (SIGINT) pour terminer proprement le programme.
 *
 * Libère les ressources : file de message, semaphore, segment de mémoire partagé.
 *
 * @param sig Le numéro du signal (non utilisé dans cette fonction).
 */
void sigint_handler(int sig)
{
    printf("\n");
    printf("%s : Suppression de la queue.\n", process_name);
    msgctl(msg_queue_id, IPC_RMID, NULL);
    printf("%s : Suppression du semaphore.\n", process_name);
    semctl(semset_id, 0, IPC_RMID, 0);
    printf("%s : Détachement du segment de mémoire partagé.\n", process_name);
    shmdt(shows);
    printf("%s : Suppression du segment partagé.\n", process_name);
    shmctl(sharedmem_id, IPC_RMID, NULL);

    printf("%s : Au revoir.\n", process_name);
    exit(EXIT_SUCCESS);
}

/**
 * @brief Mets en place les handlers de signaux
 * 
 * Déclare un handler pour le signal d'interruption
 *  et ignore le signal de mort des enfants
 * 
 * @note les enfants zombies seront gérés par le systeme d'exploitation
 * (récupération des ressources etc..)
 */
void setupSignalHandlers()
{
    // mise en place du handler d'interruption de l'exécution
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("Erreur sigaction.\n");
        exit(EXIT_FAILURE);
    }

    // pas de handler pour sigchld :
    // les enfants zombis seront gérés par l'OS
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("Erreur sigaction.\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Initialise un server (appelé par le server de consult ET le server de résa).
 * 
 * Configure les handlers de signaux, le sémaphore,
 * le segment de mémoire partagée et la file de messages
 * 
 * @note : le premier process à créer le segement partagé 
 * est aussi en charge de créer le tableau des données.
 */
void initServer(key_t key)
{
    srand(time(NULL)); // reset de la seed pour le nb de places aléatoire

    // mise en place du handler d'interruption de l'exécution
    setupSignalHandlers();

    // mise en place du tableau des sémaphores
    setupSemaphoreSet(key);

    // mise en place / récupération du segment de mémoire partagé
    setupSharedMem(key);
    
    // Création / récupération de la message queue
    setupMsgQueue(key);

    setbuf(stdout, NULL);
    printf("%s : 'Ctrl + c' pour mettre fin au programme.\n", process_name);
}

/**
 * @brief Crée/récupère un tableau de 1 sémaphore et l'initie à 1 (sémaphore binaire)
 * 
 * Utilise la clef en paragmètre pour identifier le sémaphore.
 * Si la tentative de création échoue, alors on tente une récupération
 * 
 *  
 * @param key_t la clef identifiant l'outil IPC
 */
void setupSemaphoreSet(key_t key) {
    // Création d'un tableau de 1 semaphore
    if ((semset_id = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666)) == -1)
    {
        // la création a échoué
        if (errno == EEXIST)
        {
            //le tableau de semaphore existe déjà, on le récupère
            printf("%s : Recuperation du tableau des semaphores.\n", process_name);
            semset_id = semget(key, 1, 0666);            
        }
        else
        {
            // tout à échoué, on abandonne
            perror("Creation du tableau des semaphores : Echec.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else {
        printf("%s : Creation du tableau des semaphores : Succes.\n", process_name);
    }    
    // Initialisation du semaphore (index 0, valeur d'init : 1)
    semctl(semset_id, 0, SETVAL, 1);
}

/**
 * @brief Récupère / ou crée le segment de mémoire partagé
 * 
 * Utilise la clef en paragmètre pour identifier un segment paratgé.
 * Si la tentative de récupération échoue, alors on tente une création
 * 
 * @note Le processus qui crée (en premier) le segment paratgé est aussi celui 
 * qui est responsable du remplissage de la ressource (populateResource()).
 * 
 * @param key_t la clef identifiant l'outil IPC
 */
void setupSharedMem(key_t key)
{
    // mise en place du segment de mémoire partagée
    size_t shm_size;
    shm_size = (getNbShows() + 1) * sizeof(Message);

    // récupération du segment de mémoire partagée
    if ((sharedmem_id = shmget(key, shm_size, 0666)) == -1)
    {
        //echec de la récupération,
        // peut être que le segment n'est pas encore créé        
        if (errno == ENOENT)
        {
            // le segment n'existe pas encore, => on le crée
            printf("%s : Creation du segment de memoire partage.\n", process_name);
            sharedmem_id = shmget(key, shm_size, 0666 | IPC_CREAT);
            // attachement du segment créé à l'espace d'adressage du process
            if ((shows = (Message *)shmat(sharedmem_id, NULL, 0)) == (Message *)-1)
            {
                perror("Erreur lors de l attachement a la memoire partagee");
                exit(EXIT_FAILURE);
            }
            // instanciation du tableau des spectacles
            populateResource();
        }
        else
        {
            //autre erreur de récupération
            perror("Erreur recuperation du segment de memoire partage.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("%s : Segment de memoire partage recupere.\n", process_name);
        // attachement du segment créé à l'espace d'adressage du process
        if ((shows = (Message *)shmat(sharedmem_id, NULL, 0)) == (Message *)-1)
        {
            perror("Erreur lors de l attachement a la memoire partagee");
            exit(EXIT_FAILURE);
        } else {
            printf("%s : Segment de memoire partage attache.\n", process_name);
        }
    }
}

/**
 * @brief Remplit la resource partagée shows[] avec les données des spectacle
 * 
 * l'accès en écriture à la ressource est protégé par un sémaphore en exclusion mutuelle
 * 
 * @note le nombre de places est décidé au hasard entre 16 et 30
 * un indicateur de fin de tableau est signifié par tous les bits de la structure à 0
 */
void populateResource()
{
    printf("%s : Remplissage de la ressource.\n", process_name);
    //accès en écriture sur la ressource partagée => on protège par sémaphores
    struct sembuf operations[1];

    // prélude
    operations[RESOURCE_SEM].sem_num = 0;
    operations[RESOURCE_SEM].sem_op = -1; // Ressource.P()
    semop(semset_id, operations, 1);

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

/**
 * @brief retourne le nb de place d'un spectacle passé en paramètre
 * 
 * l'accès en lecture à la ressource est protégé par un sémaphore binaire (mutex)
 * 
 * note : la partie recherche d'index est hors de la section critique
 * 
 * @param Message* un pointeur qui va recevoir le nb de places
 */
void getNbSeats(Message *msg)
{
    struct sembuf operations[1];
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

    // accès en lecture au segment partagé

    // prélude
    operations[0].sem_num = RESOURCE_SEM;
    operations[0].sem_op = -1; // ressource.P()
    semop(semset_id, operations, 1);
    // section critique
    msg->nb_seats = shows[i].nb_seats;
    // postlude
    operations[0].sem_num = RESOURCE_SEM;
    operations[0].sem_op = 1; // ressource.V()
    semop(semset_id, operations, 1);
    return;
}

/**
 * @brief Tente de Réserver le nb de place demandé pour le spectacle passé en paramètre
 * 
 * vérifie si la requete est possible (nb places restantes >= nb de places demandées)
 * 
 * l'accès en écriture à la ressource est protégé est protégé par un sémaphore binaire (mutex)
 * 
 * note : la partie recherche d'index est hors de la section critique
 * 
 * @param Message* nb de places > 0 : réservation acceptée pour le nb_places
 *                              <= 0 : réservation refusée nb de places restantes en négatif
 */
void bookSeats(Message *msg)
{
    struct sembuf operations[1];
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

    // accès en écriture au segment partagé

    // prélude
    operations[0].sem_num = RESOURCE_SEM;
    operations[0].sem_op = -1; // ressource.P()
    semop(semset_id, operations, 1);
    // section critique
    if (msg->nb_seats <= shows[i].nb_seats)
    {
        // il reste assez de places
        shows[i].nb_seats = shows[i].nb_seats - msg->nb_seats;
    }
    else
    {
        // il ne reste pas assez de places pour honorer la réservation entière
        msg->nb_seats = -1 * shows[i].nb_seats;
    }
    // postlude
    operations[0].sem_num = RESOURCE_SEM;
    operations[0].sem_op = 1; // ressource.V()
    semop(semset_id, operations, 1);
    return;
}
