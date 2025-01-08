/*******************************************************************************
 * @file client.c
 * @brief Implémentation du programme client de la question 2.
 * @author Romain COIRIER
 * @date 05/01/2025
 * @version 1.0
 * 
 * cf common.h
 * 
 * @note il y a 1 server différent pour chaque opération : consultation ou réservation
 * 
 * 
 * @bug .
 ******************************************************************************/

#include "common.h"

//Variables globales
int msg_queue_id; //

//déclarations
void sigint_handler(int sig);

void setupSignalHandlers();
void setupMsgQueue(key_t key);

int getUserRequest(Message *msg);
void getShowId(Message *msg);
int getRequestType(Message *msg);
void getNbSeatsRequested(Message *msg);
void displayResponse(Response msg_resp, Request msg_req);

int main(void){

    printf("PROJET NSY103 - QUESTION 2.\n");
    printf("Client.\n");
    printf("===========================\n");

    // variables
    int return_value;
    pid_t pid = getpid();
    Request msg_req;
    Response msg_resp;

    //mise en place du handler d'interruption de l'exécution
    setupSignalHandlers();
    // Génération de la clé pour la mémoire partagée et le sémaphore
    key_t key = ftok(KEY_FILENAME, KEY_ID);
    setupMsgQueue(key);

    while(1) {

        //préparation du message en fonction des choix de l'utilisateur
        if ((msg_req.msg_type = getUserRequest(&msg_req.msg)) == REQUEST_CONSULT) {
            //requete de consultation
            printf("Requete de Consultation pour le spectacle %s.\n", msg_req.msg.show_id);
        } else {
            //requete de réservation
            printf("Requete de Reservation de %d places pour le spectacle %s.\n", msg_req.msg.nb_seats, msg_req.msg.show_id);
        }

        //envoi de la requête
        msg_req.pid = pid; //utilisé pour le type de la réponse
        if((return_value = msgsnd(msg_queue_id, &msg_req, sizeof(Request) - sizeof(long), 0)) == -1) {
            perror("Echec msgsnd.\n");
            exit(EXIT_FAILURE);
        }

        //attente de la réponse
        if((return_value = msgrcv(msg_queue_id, &msg_resp, sizeof(Response) - sizeof(long), (long) pid, 0)) == -1) {
            perror("Echec msgrcv.\n");
            exit(EXIT_FAILURE);
        }

        displayResponse(msg_resp, msg_req);
    }
}

// handler pour signal SIGINT
void sigint_handler(int sig) {
    printf("\n");

    //on met fin au programme
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

void displayResponse(Response msg_resp, Request msg_req){
        //show_id inexistant (toute la structure msg est remplie de 0)
        if(msg_resp.msg.show_id[0] == '\0') {
            printf("Le serveur indique que le spectacle %s n existe pas.\n\n", msg_req.msg.show_id);
            return;
        }

        if (msg_req.msg_type == REQUEST_CONSULT) {
            //requete de consultation
            printf("Il reste %d places libres pour le spectacle %s.\n\n", msg_resp.msg.nb_seats, msg_resp.msg.show_id);
        } else {
            //requete de réservation
            if(msg_resp.msg.nb_seats > 0) { 
                printf("Reservation confirmee de %d places\
 pour le spectacle %s.\n\n", msg_resp.msg.nb_seats, msg_resp.msg.show_id); 
            } else {
                printf("Reservation impossible de %d places ;\
 %d disponibles pour le spectacle %s.\n\n", msg_req.msg.nb_seats, msg_resp.msg.nb_seats, msg_resp.msg.show_id);
            }            
        }
}

/**
 * @brief Rempli la structure de message à envoyer.
 * 
 * Demande à l'utilisateur de fournir les infos nécessaire
 * pour remplir le message de sa requête.
 * 
 * @param msg une structure de message à remplir (par reference).
 * @return type de requete : REQUETE_CONSULT ou REQUETE_RESA. 
 */
int getUserRequest(Message *msg) {
    int request_type = REQUEST_CONSULT;
    bool is_valid_input = false;

    getShowId(msg);
    while (!is_valid_input) {
        request_type = getRequestType(msg);
        if (request_type == REQUEST_RESA) {
            //requete de réservaztion => on demande le nb de places
            is_valid_input = true;
            getNbSeatsRequested(msg);
        } else if (request_type == REQUEST_CONSULT) {
            is_valid_input = true;
        } else {
            fprintf(stderr, "Saisie non valide.\n");
        }
    } 
    
    return request_type;
}

void getShowId(Message *msg) {
    bool is_valid_input = false;

    // demande un spectacle tant que la saisie n'est pas valide
    while(!is_valid_input) {
        printf("Saisissez un spectacle parmis \n");
        int i = 0;
        while(SHOW_IDS[i] != NULL) {
            if(i > 0) {
                printf(", ");
            }
            printf("%s", SHOW_IDS[i]);
            i++;
        }
        printf(" :\n");
        if(scanf("%6s", msg->show_id) == 1) { //Attention, magic number !
            //on rajoute une terminaison de chaine comme 7éme caractere
            msg->show_id[SHOW_ID_LEN - 1] = '\0';
            is_valid_input = true;
        } else {
            fprintf(stderr, "Saisie non valide.\n");
        }
    }
    return;
}

int getRequestType(Message *msg) {
    bool is_valid_input = false;
    int request_type;
    // demande un type de requete tant que la saisie n'est pas valide
    while(!is_valid_input) {
        printf("Choissiez votre requete pour %s\n \
(%d)-> Consultation, (%d)->Reservation :\n", msg->show_id, REQUEST_CONSULT, REQUEST_RESA);
        if(scanf("%d", &request_type) == 1) {
            is_valid_input = true;
        } else {
            fprintf(stderr, "Saisie non valide.\n");
        }
    }
    return request_type;
}

void getNbSeatsRequested(Message *msg) {
    bool is_valid_input = false;
    int nb_seats;
    int request_type;
    // demande un nombre de places tant que la saisie n'est pas valide
    while(!is_valid_input) {
        printf("Combien de places souhaitez-vous reserver pour %s (0-127):\n", msg->show_id);
        if(scanf("%d", &nb_seats) == 1) {
            //TODO add vérif du range 0 à 127
            is_valid_input = true;
        } else {
            fprintf(stderr, "Saisie non valide.\n");
        }
    }
    msg->nb_seats = (signed char) nb_seats;
    return;
}

