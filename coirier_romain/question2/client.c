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

//descripteur du socket en global pour fermeture hors de main()
int sock_fd;

//déclarations
void sigint_handler(int sig);

void setupSignalHandlers();
void setupSocket();
void getServerAddrInfo(struct addrinfo **serv_info, RequestType rq_type);

int getUserRequest(Message *msg);
void getShowId(Message *msg);
int getRequestType(Message *msg);
void getNbSeatsRequested(Message *msg);

int main(void){

    printf("PROJET NSY103 - QUESTION 2.\n");
    printf("Client.\n");
    printf("===========================\n");

    // variables
    int return_value, request_type, nb_bytes;
    struct addrinfo *serv_info;
    Message c2s_buf, s2c_buf; //structs des messages clients vers server et retour

    //mise en place du handler d'interruption de l'exécution
    setupSignalHandlers();

    while(1) {
        //mise en place du socket
        setupSocket();

        //préparation du message en fonction des choix de l'utilisateur
        if ((request_type = getUserRequest(&c2s_buf)) == REQUEST_CONSULT) {
            //requete de consultation
            printf("Requete de Consultation pour le spectacle %s.\n", c2s_buf.show_id);
        } else {
            //requete de réservation
            printf("Requete de Reservation de %d places pour le spectacle %s.\n", c2s_buf.nb_seats, c2s_buf.show_id);
        }

        //création de l'address info du server adapté à la requête
        getServerAddrInfo(&serv_info, request_type);

        //blocage en attendant la connexion
        if((return_value = connect(sock_fd, serv_info->ai_addr, serv_info->ai_addrlen)) != 0) {
            perror("Echec de connexion !\n");
            fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
            printf("Fermeture du socket.\n");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        //envoi de la requête
        nb_bytes = send(sock_fd, &c2s_buf, sizeof(c2s_buf),0);
        //TODO vérif du nb_bytes envoyés
        //attente de la réponse
        nb_bytes = recv(sock_fd, &s2c_buf, sizeof(s2c_buf),0); //TODO check nb_bytes ?
        if (request_type == REQUEST_CONSULT) {
            //requete de consultation
            printf("Il reste %d places libres pour le spectacle %s.\n\n", s2c_buf.nb_seats, s2c_buf.show_id);
        } else {
            //requete de réservation
            printf("Il reste %d places libres pour le spectacle %s.\n\n", s2c_buf.nb_seats, s2c_buf.show_id);
        }

        //Fermeture du socket (un shutdown n'est pas suffisant pour reconnecter)
        close(sock_fd);
    }

    exit(0); 
}

// handler pour signal SIGINT
void sigint_handler(int sig) {
    printf("\n");
    printf("Fermeture du socket.\n");
    close(sock_fd);
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

void setupSocket() {
   int return_value;
    struct addrinfo hints, *client_info;

    //création de l'address info du client (nous): client_info
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // utilise IPV4 ou IPV6
    hints.ai_socktype = SOCK_STREAM; //TCP
    hints.ai_flags = AI_PASSIVE; // flag pour utiliser notre IP (client)
    if((return_value = getaddrinfo(NULL, CLIENT_PORT, &hints, &client_info)) != 0) {
        perror("Erreur creation adresse client\n");
        exit(EXIT_FAILURE);
    }

    //création du socket et association à l'adresse (et port) du client.
    sock_fd = socket(client_info->ai_family, client_info->ai_socktype, client_info->ai_protocol);
    if(sock_fd == -1) {
        perror("Erreur creation du socket\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if((return_value = bind(sock_fd, client_info->ai_addr, client_info->ai_addrlen)) != 0) {
        perror("Erreur association du socket\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        printf("Fermeture du socket.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
}

void getServerAddrInfo(struct addrinfo **serv_info, RequestType rq_type) {
    int return_value;
    struct addrinfo hints;
   //création de l'address info du server de consultation ou réservation
    memset(&hints, 0, sizeof(hints)); //remplissage avec des 0 avant de réutiliser hints
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (rq_type == REQUEST_CONSULT) {
        //utilisation du port du serveur de consult
        return_value = getaddrinfo(SERVER_IP, SERVER_PORT_CONSULT, &hints, serv_info);
    } else {
        //utilisation du port du serveur de résa
        return_value = getaddrinfo(SERVER_IP, SERVER_PORT_RESA, &hints, serv_info);
    }
    if(return_value != 0) {
        perror("Erreur creation adresse serveur\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        printf("Fermeture du socket.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
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

