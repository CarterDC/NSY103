/*******************************************************************************
 * @file client.c
 * @brief Implémentation du programme client de la question 1.
 * @author Romain COIRIER
 * @date 08/01/2025
 * @version 1.0
 * 
 * cf common.h
 * Envoie les requêtes de l'utilisateur (consultation ou réservation) au server,
 *  via socket TCP (avec connexion).
 * 
 * @note Ce client permet de faire de multiples requêtes à la suite 
 * 
 * @bug ?
 ******************************************************************************/

#include "common.h"

// descripteur du socket en global pour fermeture hors de main()
int sock_fd;

// prototypes de fonctions
void sigint_handler(int sig);

void setupSignalHandlers();
void setupSocket();
void getServerAddrInfo(struct addrinfo **serv_info);
void initClient(struct addrinfo *serv_info);

int getUserRequest(Message *msg);
void requestShowId(Message *msg);
int getRequestType(Message *msg);
void requestNbSeatsToBook(Message *msg);

void displayResponse(Message msg_resp, Message msg_req, int rq_type);

/**
 * main()
 * TODO !!
 * 
 */
int main(void)
{

    printf("PROJET NSY103 - QUESTION 1.\n");
    printf("Client.\n");
    printf("===========================\n");

    //
    int return_value, request_type, nb_bytes;
    struct addrinfo *serv_info;
    Message c2s_buf, s2c_buf; // structs des messages clients vers server et retour

    setupSignalHandlers();
    getServerAddrInfo(&serv_info);

    while (1)
    {
        // préparation du message en fonction des choix de l'utilisateur
        request_type = getUserRequest(&c2s_buf) == REQUEST_CONSULT;

        setupSocket();
        // blocage en attendant la connexion au server
        if ((return_value = connect(sock_fd, serv_info->ai_addr, serv_info->ai_addrlen)) != 0)
        {
            perror("Echec de connexion !\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            printf("Fermeture du socket.\n");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
        // envoi de la requête
        nb_bytes = send(sock_fd, &c2s_buf, sizeof(c2s_buf), 0);
        // TODO vérif du nb_bytes envoyés
        // attente de la réponse
        nb_bytes = recv(sock_fd, &s2c_buf, sizeof(s2c_buf), 0); // TODO check nb_bytes ?
        // TODO vérif du nb_bytes reçus
        
        displayResponse(s2c_buf, c2s_buf, request_type);
        close(sock_fd);
    }
}

/**
 * @brief Gère le signal d'interruption (SIGINT).
 *
 * Affiche un message d'adieu et termine proprement le programme.
 *
 * @param sig Le numéro du signal (non utilisé dans cette fonction).
 */
void sigint_handler(int sig)
{
    printf("\n");
    printf("Fermeture du socket.\n");
    close(sock_fd);

    // on met fin au programme
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
 * @brief TODO
 */
void setupSocket()
{
    int return_value;
    struct addrinfo hints, *client_info;

    // création de l'address info du client (nous): client_info
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // utilise IPV4 ou IPV6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;     // flag pour utiliser notre IP (client)
    if ((return_value = getaddrinfo(NULL, CLIENT_PORT, &hints, &client_info)) != 0)
    {
        perror("Erreur creation adresse client\n");
        exit(EXIT_FAILURE);
    }

    // création du socket et association à l'adresse (et port) du client.
    sock_fd = socket(client_info->ai_family, client_info->ai_socktype, client_info->ai_protocol);
    if (sock_fd == -1)
    {
        perror("Erreur creation du socket\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if ((return_value = bind(sock_fd, client_info->ai_addr, client_info->ai_addrlen)) != 0)
    {
        perror("Erreur association du socket\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        printf("Fermeture du socket.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief TODO
 */
void getServerAddrInfo(struct addrinfo **serv_info)
{
    int return_value;
    struct addrinfo hints;
    // création de l'address info du server de consultation ou réservation
    memset(&hints, 0, sizeof(hints)); // remplissage avec des 0 avant de réutiliser hints
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    return_value = getaddrinfo(SERVER_IP, SERVER_PORT, &hints, serv_info);

    if (return_value != 0)
    {
        perror("Erreur creation adresse serveur\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        printf("Fermeture du socket.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Initialise le client.
 *
 * Configure les handlers de signaux
 * Crée le socket TCP et l'address info du server
 */
void initClient(struct addrinfo *serv_info)
{
    // mise en place du handler d'interruption de l'exécution
    setupSignalHandlers();

    // mise en place du socket
    setupSocket();

    // création de l'address info du server adapté à la requête
    getServerAddrInfo(&serv_info);

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
void displayResponse(Message msg_resp, Message msg_req, int rq_type) {
    
    //si la structure est remplie de 0, le server indique que le show n'existe pas
    if (msg_resp.show_id[0] == '\0') {        
        printf("Le serveur indique que le spectacle %s n existe pas.\n\n",
            msg_req.show_id);
        return;
    }

    if (rq_type == REQUEST_CONSULT) {
        // Requête de consultation
        printf("Il reste %d places libres pour le spectacle %s.\n\n", 
            msg_resp.nb_seats, msg_resp.show_id);
    } else {
        // Requête de réservation
        if (msg_resp.nb_seats > 0) {
            printf("Reservation confirmee de %d places pour le spectacle %s.\n\n",
                msg_resp.nb_seats, msg_resp.show_id);
        } else {
            printf("Reservation impossible de %d places ; %d disponibles pour le spectacle %s.\n\n", 
                msg_req.nb_seats, -1 * msg_resp.nb_seats, msg_resp.show_id);
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