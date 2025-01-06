/*******************************************************************************
 * @file server_consult.c
 * @brief Implémentation du serveur de consultation de la question 2.
 * @author Romain COIRIER
 * @date 06/01/2025
 * @version 1.0
 * 
 * cf common.h
 * Ce serveur accepte des connections (TCP) 
 * et traite les requetes de façon séquentielle (itérative)
 * 
 * @note Ce serveur accède au segment de mémoire partagée (table des spectacles) 
 * en tant que lecteur, selon les règles de synchronisation 
 * lecteur / rédacteur avec préférence aux rédacteurs 
 * 
 * @bug .
 ******************************************************************************/

#include "common.h"

//descripteur du socket en global pour fermeture hors de main()
int listen_sock_fd;

//déclarations
void sigint_handler(int sig);

void setupSignalHandlers();
void setupListeningSocket();

int getNbSeats(const char *show_id);

int main(void){

    printf("PROJET NSY103 - QUESTION 2.\n");
    printf("Serveur de Consultation.\n");
    printf("===========================\n");

    // variables
    int service_sock_fd, nb_bytes;
    struct sockaddr client_addr; // adresse du client 
    int addr_len = sizeof(client_addr);
    Message s2c_buf, c2s_buf; //structs des messages server vers client et retour

    //mise en place du handler d'interruption de l'exécution
    setupSignalHandlers();
    //mise en place du socket
    setupSocket();

    listen(listen_sock_fd, 10); // Queue de 10 demandes (pertinent avec server itératif)

    while(1) {
        //créa d'une socket de service à l'acceptation de la connexion
        service_sock_fd = accept(listen_sock_fd, &client_addr, &addr_len);
        
        // traitement séquntiel de la requête avant d'accepter une nouvelle connexion
        // réception de la requête
        nb_bytes = recv(service_sock_fd, &c2s_buf, sizeof(c2s_buf),0); //TODO check nb_bytes ? 
        printf("SC : Requete de Consultation pour le spectacle %s.\n", c2s_buf.show_id);
        
        strncpy(s2c_buf.show_id, c2s_buf.show_id, SHOW_ID_LEN);
        s2c_buf.nb_seats = getNbSeats(s2c_buf.show_id);
        //renvoie nb de places pour le spectacle
        nb_bytes = send(service_sock_fd, &s2c_buf, sizeof(s2c_buf),0); //TODO check nb_bytes ? 

        //traitement terminé on retourne en attente d'acceptation de connexion
    }

    exit(0); 
}

// handler pour signal SIGINT
void sigint_handler(int sig) {
    printf("\n");
    printf("Fermeture du socket.\n");
    close(listen_sock_fd);
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

void setupListeningSocket() {
   int return_value;
    struct addrinfo hints, *server_info;

    //création de l'address info du server de consultation (nous): server_info
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // utilise IPV4 ou IPV6
    hints.ai_socktype = SOCK_STREAM; //TCP
    if((return_value = getaddrinfo(SERVER_IP, SERVER_PORT_CONSULT, &hints, &server_info)) != 0) {
        perror("Erreur creation adresse server consultation\n");
        exit(EXIT_FAILURE);
    }

    //création du socket et association à l'adresse (et port) du client.
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

int getNbSeats(const char *show_id) {
    int nb_seats = 0;
    if(strcmp(show_id, "NSY103") == 0) {
        nb_seats = 30;
    } else {
        nb_seats = 25;
    }
    return nb_seats;
}
