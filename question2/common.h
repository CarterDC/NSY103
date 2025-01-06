/*******************************************************************************
 * @file common.h
 * @brief Includes et Constantes communs aux client et serveurs de la question 2.
 * @author Romain COIRIER
 * @date 05/01/2025
 * @version 1.0
 * 
 * Contient aussi la structure des messages partagés par l'ensemble des clients
 * et servers : 
 * -> spectacle_id : char[7] (6 caractères + terminaison \0)
 * -> nb_places : signed char (-127 à 127)
 * pour une taille totale de 64 bits (TODO : à vérifier).
 * 
 * @note le champ nb_places est utilisé pour la communication client server : 
 *     0 demande de consultation.
 *     > 0 en consultation : réponse ; en réservation : demande / accusé
 *     < 0 en réservation refus avec indication des places restantes
 * 
 * @bug oui ?.
 ******************************************************************************/

#ifndef COMMON_H
#define COMMON_H

// Standard Library
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h>
#include <string.h>
#include <sys/types.h>

//signaux
#include <signal.h>

// Sockets & réseaux
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

// erreurs
#include <errno.h>

// mémoire partagée

// semaphores system V

// CONSTANTES
#define SERVER_IP "localhost" //devrait fonctionner pareil en IPV4 ou V6
#define SERVER_PORT_CONSULT "49152" // premier port de la plage privée
#define SERVER_PORT_RESA "49153"
#define CLIENT_PORT "49154"

// Tableau des noms de spectacles (6 caractères exactement)
static const char *const LISTE_SPECTACLES[] = {
    "NSY103",
    "RCP105",
    "NFP121",
    NULL // terminaison du tableau
};

// Structure des messages
#define SPECTACLE_ID_LEN 7 // todo : pê pas nécessaire ? 
typedef struct {
    char spectacle_id[SPECTACLE_ID_LEN]; // 6 char + \0
    signed char nb_places; // -1,  0 ou nb places demandées / réservées
} message;


#endif