/*******************************************************************************
 * @file cclient.c
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

int main(void){

    printf("PROJET NSY103 - QUESTION 2.\n");
    printf("Client.\n");
    printf("===========================\n");

    //mise en place du handler d'interruption de l'exécution
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    if(sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Erreur sigaction.\n");
        exit(EXIT_FAILURE);
    }
    printf("'Ctrl + c' pour mettre fin au programme.\n");

    //déclarations
    int return_value, nb_bytes;
    bool is_consultation_request;
    struct addrinfo hints, *client_info, *serv_info;
    message c2s_buf, s2c_buf; //structs des messages clients vers server et retour

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
    bind(sock_fd, client_info->ai_addr, client_info->ai_addrlen);

    //préparation du message en fonction des choix de l'utilisateur
    is_consultation_request = getUserInput(&c2s_buf);

    //création de l'address info du server de consultation ou réservation
    memset(&hints, 0, sizeof(hints)); //remplissage avec des 0 avant de réutiliser hints
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (is_consultation_request) {
        //requete de consultation
        //utilisation du port du serveur de consult
        return_value = getaddrinfo(SERVER_IP, SERVER_PORT_CONSULT, &hints, &serv_info);
    } else {
        //requete de réservation
        //utilisation du port du serveur de résa
        return_value = getaddrinfo(SERVER_IP, SERVER_PORT_RESA, &hints, &serv_info);
    }
    if(return_value != 0) {
        perror("Erreur creation adresse serveur\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        printf("Fermeture du socket.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    //blocage en attendant la connexion
    if((return_value = connect(sock_fd, serv_info->ai_addr, serv_info->ai_addrlen)) != 0) {
        perror("Echec de connexion !\n");
        fprintf(stderr, "Erreur %d : %s\n", errno, strerror(errno));
        printf("Fermeture du socket.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    exit(0);
 
}

bool getUserInput(message *msg) {
    bool is_consultation_request;
    bool is_valid_input = false;

    // demande un spectacle tant que la saisie n'est pas valide
    while(!is_valid_input) {
        printf("Saisissez un spectacle parmis \n");
        int i = 0;
        while(LISTE_SPECTACLES[i] != NULL) {
            if(i > 0) {
                printf(", ");
            }
            printf("%s", LISTE_SPECTACLES[i]);
        }
        printf(" :\n");
        if(scanf("%6s", msg->spectacle_id) == 1) { //Attention, magic number !
            //on rajoute une terminaison de chaine comme 7éme caractere
            msg->spectacle_id[SPECTACLE_ID_LEN - 1] = "\0";
            is_valid_input = true;
        } else {
            fprintf(stderr, "Saisie non valide.\n");
        }
    }
    is_valid_input = false;

    
    return is_consultation_request;
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