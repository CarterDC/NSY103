/*******************************************************************************
 * @file client.c
 * @brief Implémentation du programme client de la question 2.
 * @author Romain COIRIER
 * @date 05/01/2025
 * @version 1.0
 * 
 * cf common.h
 * Envoie les requêtes de l'utilisateur (consultation ou réservation) au server,
 *  via une file de message.
 * Reçoit les réponses identifiée avec le PID du client sur la meme queue 
 * 
 * @note Ce client permet de faire de multiples requêtes à la suite 
 * 
 * @bug ?
 ******************************************************************************/

#include "common.h"

//Variables globales
int msg_queue_id; // l'identifiant de la file de messages System V

//prototypes de fonctions
void sigint_handler(int sig);

void setupSignalHandlers();
void setupMsgQueue(key_t key);
void initClient();

int getUserRequest(Message *msg);
void requestShowId(Message *msg);
int getRequestType(Message *msg);
void requestNbSeatsToBook(Message *msg);

void displayResponse(Response msg_resp, Request msg_req);

/**
 * main()
 *
 * Crée ou récupère une message queue pour envoyer et recevoir des messages
 * avec un server sur la meme machine locale.
 * 
 */
int main(void){

    printf("PROJET NSY103 - QUESTION 2.\n");
    printf("Client.\n");
    printf("===========================\n");

    // variables
    int return_value;
    pid_t pid = getpid();
    Request msg_req;
    Response msg_resp;

    initClient();

    while(1) {

        //préparation de le requete en fonction des choix de l'utilisateur
        msg_req.msg_type = getUserRequest(&msg_req.msg);

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

/**
 * @brief Gère le signal d'interruption (SIGINT).
 *
 * Affiche un message d'adieu et termine proprement le programme.
 *
 * @param sig Le numéro du signal (non utilisé dans cette fonction).
 */
void sigint_handler(int sig) {

    printf("\n");

    // coté client, on ne ferme pas la messageQueue
    // pour ne pas générer un SIPIPE coté le server 

    // On met fin au programme
    printf("Au revoir.\n");
    exit(EXIT_SUCCESS);
}

/**
 * @brief Mets en place les handlers de signaux pour le process.
 * ici uniquement SIGINT
 */
void setupSignalHandlers() {

    // Mise en place du handler d'interruption de l'exécution
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Erreur sigaction.\n");
        exit(EXIT_FAILURE);
    }
    // Affiche la méthode de déclenchement du signal 
    printf("'Ctrl + c' pour mettre fin au programme.\n");
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
            msg_queue_id = msgget(key, 0666);
        } else {
            perror("Echec creation de la file de messages.\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}


/**
 * @brief Initialise le client.
 *
 * Configure les handlers de signaux
 * et initialise la file de messages.
 */
void initClient() {
    // Mise en place du handler d'interruption de l'exécution
    setupSignalHandlers();

    // Génération de la clé pour la message queue
    key_t key = ftok(KEY_FILENAME, KEY_ID);
    if (key == -1) {
        perror("Echec creation de la clef.\n");
        exit(EXIT_FAILURE);
    }

    // Initialisation de la file de messages avec la clef générée
    setupMsgQueue(key);
}


/**
 * @brief Affiche la réponse reçue du serveur en fonction du type de requête.
 *
 * Si l'identifiant du spectacle n'existe pas, un message d'erreur est affiché.
 * Un nombre de places strictement positif indique une réservation acceptée.
 * Si la réservation est refusée, un nombre de places négatif indique le nb de places restantes.
 * 
 * @param msg_resp La réponse du serveur
 * @param msg_req La requête initiale de l'utilisateur
 */
void displayResponse(Response msg_resp, Request msg_req) {
    
    //si la structure est remplie de 0, le server indique que le show n'existe pas
    if (msg_resp.msg.show_id[0] == '\0') {        
        printf("Le serveur indique que le spectacle %s n existe pas.\n\n",
            msg_req.msg.show_id);
        return;
    }

    if (msg_req.msg_type == REQUEST_CONSULT) {
        // Requête de consultation
        printf("Il reste %d places libres pour le spectacle %s.\n\n", 
            msg_resp.msg.nb_seats, msg_resp.msg.show_id);
    } else {
        // Requête de réservation
        if (msg_resp.msg.nb_seats > 0) {
            printf("Reservation confirmee de %d places pour le spectacle %s.\n\n",
                msg_resp.msg.nb_seats, msg_resp.msg.show_id);
        } else {
            printf("Reservation impossible de %d places ; %d disponibles pour le spectacle %s.\n\n", 
                msg_req.msg.nb_seats, -1 * msg_resp.msg.nb_seats, msg_resp.msg.show_id);
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
int getUserRequest(Message *msg)
{
    int request_type = REQUEST_CONSULT;
    bool is_valid_input = false;
    // demande à l'utilisateur de saisir un spectacle parmi la liste
    requestShowId(msg);

    while (!is_valid_input)
    {
        request_type = getRequestType(msg);
        if (request_type == REQUEST_RESA)
        {
            // requete de réservation => on demande le nb de places
            is_valid_input = true;
            requestNbSeatsToBook(msg);
        }
        else if (request_type == REQUEST_CONSULT)
        {
            is_valid_input = true;
        }
        else
        {
            // la requête saisie n'est ni REQUEST_RESA ni REQUEST_CONSULT
            fprintf(stderr, "Saisie non valide.\n");
        }
    }

    return request_type;
}

/**
 * @brief Demande à l'utilisateur de saisir un spectacle et
 *  remplit le champ correspondant dans la structure de message.
 * 
 * Affiche la liste des spectacles disponibles 
 * et demande à l'utilisateur de saisir un identifiant de spectacle valide.
 * 
 * @param msg Une structure de message à remplir (par référence).
 */
void requestShowId(Message *msg)
{
    bool is_valid_input = false;

    // demande un spectacle tant que la saisie n'est pas valide
    while (!is_valid_input)
    {
        printf("Saisissez un spectacle parmi \n");
        //affichage de la liste des spectacles
        int i = 0;
        while (SHOW_IDS[i] != NULL)
        {
            if (i > 0)
            {
                printf(", ");
            }
            printf("%s", SHOW_IDS[i]);
            i++;
        }
        printf(" :\n");

        //attend une saisie de 6 caractères
        // (les char en plus ne sont pas pris en compte)
        if (scanf("%6s", msg->show_id) == 1) // Attention, magic number '6' !
        { 
            // on rajoute une terminaison de chaine comme dernière caractere
            msg->show_id[SHOW_ID_LEN - 1] = '\0';
            is_valid_input = true;
        }
        else
        {
            fprintf(stderr, "Saisie non valide.\n");
            // Vide le buffer d'entrée pour éviter une boucle infinie
            while (getchar() != '\n');
        }
    }
    return;
}

/**
 * @brief Demande à l'utilisateur de choisir un type de requête.
 *
 * Affiche les options de types de requêtes disponibles et demande à l'utilisateur
 * de saisir un type de requête valide.
 *
 * @param msg Une structure de message contenant l'identifiant du spectacle.
 * @return Le type de requête : REQUEST_CONSULT ou REQUEST_RESA.
 */
int getRequestType(Message *msg)
{
    bool is_valid_input = false;
    int request_type;

    // Demande un type de requête tant que la saisie n'est pas valide
    while (!is_valid_input)
    {
        printf("Choisissez votre requete pour %s\n \
(%d)-> Consultation, (%d)-> Reservation :\n",
               msg->show_id, REQUEST_CONSULT, REQUEST_RESA);
        if (scanf("%d", &request_type) == 1)
        {
            is_valid_input = true;
        }
        else
        {
            fprintf(stderr, "Saisie non valide.\n"); // ex: caractères non numériques
            // Vide le buffer d'entrée pour éviter une boucle infinie
            while (getchar() != '\n');
        }
    }
    return request_type;
}


/**
 * @brief Demande à l'utilisateur de saisir le nombre de places à réserver.
 *
 * Affiche un message demandant à l'utilisateur de saisir un nombre de places
 * valide pour la réservation. La saisie doit être comprise entre 0 et 127.
 *
 * @param msg Une structure de message à remplir avec le nombre de places (par référence).
 */
void requestNbSeatsToBook(Message *msg)
{
    bool is_valid_input = false;
    int nb_seats;

    // Demande un nombre de places tant que la saisie n'est pas valide
    while (!is_valid_input)
    {
        printf("Combien de places souhaitez-vous reserver pour %s (1-127) :\n", msg->show_id);

        // Vérifie si l'utilisateur a saisi un entier et s'il est dans la plage valide (1-127)
        if ((scanf("%d", &nb_seats) == 1) && ((nb_seats > 0) && (nb_seats <= 127)))
        {
            is_valid_input = true;
        }
        else
        {
            fprintf(stderr, "Saisie non valide.\n");
            // Vide le buffer d'entrée pour éviter une boucle infinie
            while (getchar() != '\n');
        }
    }

    msg->nb_seats = (signed char)nb_seats;
}