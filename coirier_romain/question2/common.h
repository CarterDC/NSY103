/*******************************************************************************
 * @file common.h
 * @brief Includes et Constantes communs au client et serveur de la question 2.
 * @author Romain COIRIER
 * @date 05/01/2025
 * @version 1.0
 * 
 * Contient aussi la structure des messages partagés par l'ensemble des clients
 * et servers : 
 * -> spectacle_id : char[7] (6 caractères + terminaison \0)
 * -> nb_places : signed char (-128 à 127)
 * pour une taille totale de 64 bits (TODO : à vérifier).
 * 
 * @note le champ nb_places est utilisé pour la communication client server : 
 *     0 demande de consultation.
 *     > 0 en consultation : réponse ; en réservation : demande / accusé
 *     < 0 en réservation refus avec indication des places restantes
 * 
 * @bug ?.
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

//message queue
#include <sys/msg.h>

// erreurs
#include <errno.h>

// CONSTANTES
#define KEY_FILENAME "NSY"
#define KEY_ID 103

// Tableau des noms de spectacles (6 caractères exactement)
static const char *const SHOW_IDS[] = {
    "NSY103",
    "RCP105",
    "NFP121",
    NULL // terminaison du tableau
};

// Types de requête
typedef enum {
    REQUEST_CONSULT = 1, // requête en consultation
    REQUEST_RESA = 2 // requête en réservation
} RequestType;


// Structure des messages
#define SHOW_ID_LEN 7 //  
typedef struct {
    char show_id[SHOW_ID_LEN]; // 6 char + \0
    signed char nb_seats; // -1,  0 ou nb places demandées / réservées
} Message;

typedef struct { 
    long msg_type;
    Message msg;
    pid_t pid;
} Request;

typedef struct {
    long msg_type;
    Message msg;
} Response;

#endif