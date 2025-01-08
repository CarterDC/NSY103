/*******************************************************************************
 * @file server_consult.c
 * @brief Implémentation du serveur de consultation de la question 1.
 * @author Romain COIRIER
 * @date 08/01/2025
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

#include <sys/sem.h>
#include <time.h>

// variables globales
int listen_sock_fd; //descripteur du socket d'écoute TCP
int semset_id; // l'identifiant du tableau des sméphores System V
Message *shows; // pointeur vers le futur tableau partagé

//Prototypes
void sigint_handler(int sig);

void setupSignalHandlers();
void setupSemaphoreSet(key_t key);
void populateSharedMem();
int getNbShows();
void setupListeningSocket();
void initServer();

void getNbSeats(Message *msg);

int main(void){

    printf("PROJET NSY103 - QUESTION 1.\n");
    printf("Serveur.\n");
    printf("===========================\n");

    // variables
    int service_sock_fd, nb_bytes;
    struct sockaddr client_addr; // adresse du client 
    int addr_len = sizeof(client_addr);
    Message s2c_buf, c2s_buf; //structs des messages server vers client et retour

    //mise en place du segment partagé, du sémaphore binaire et du socket d'écoute
    initServer();

    listen(listen_sock_fd, 10); // Queue de 10 demandes (pertinent avec server itératif)

    while(1) {
        //créa d'une socket de service à l'acceptation de la connexion
        service_sock_fd = accept(listen_sock_fd, &client_addr, &addr_len);
        
        // traitement séquentiel de la requête avant d'accepter une nouvelle connexion
        // réception de la requête
        nb_bytes = recv(service_sock_fd, &c2s_buf, sizeof(c2s_buf),0); //TODO check nb_bytes ? 
        printf("SC : Requete de Consultation pour le spectacle %s.\n", c2s_buf.show_id);
        
        strncpy(s2c_buf.show_id, c2s_buf.show_id, SHOW_ID_LEN);
        getNbSeats(&s2c_buf);
        //renvoie nb de places pour le spectacle
        nb_bytes = send(service_sock_fd, &s2c_buf, sizeof(s2c_buf),0); //TODO check nb_bytes ? 

        //traitement terminé on retourne en attente d'acceptation de connexion
        close(service_sock_fd);
    }

}

// handler pour signal SIGINT
void sigint_handler(int sig) {

    printf("\n");
    printf("Fermeture du socket.\n");
    close(listen_sock_fd);
    printf("Suppression du semaphore.\n");
    semctl(semset_id, 0, IPC_RMID, 0);
    
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

    // Génération de la clé pour la mémoire partagée et le sémaphore
    key_t key = ftok(KEY_FILENAME, KEY_ID);

    // mise en place du tableau des sémaphores
    setupSemaphoreSet(key);

    //remplissage du tableau des spectacles
    populateSharedMem();

    //mise en place du socket
    setupListeningSocket();
}

void setupSemaphoreSet(key_t key) {
    // Création ou récupération (si déjà créé) d'un tableau de 1 semaphore
    if ((semset_id = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666)) == -1)
    {
        if (errno == EEXIST)
        {
            //le tableau de semaphore existe déjà, on le récupère
            semset_id = semget(key, 1, 0666);
        }
        else
        {
            perror("Echec creation du semaphore.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }    
    // Initialisation du semaphore (index 0, valeur d'init : 1)
    semctl(semset_id, 0, SETVAL, 1);
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
    semop(semset_id, operations, 1);
    //section critique
    msg->nb_seats = shows[i].nb_seats;
    //postlude
    operations[0].sem_num = 0;
    operations[0].sem_op = 1; //V()
    semop(semset_id, operations, 1);    
    return;
}
