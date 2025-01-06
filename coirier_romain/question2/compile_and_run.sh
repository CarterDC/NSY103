#!/bin/bash

# Sources
CLIENT_SRC="client.c" 
RESA_SERVER_SRC="server_resa.c"
CONSULT_SERVER_SRC="server_consult.c"

# Executables
CLIENT_OUT="client"
RESA_SERVER_OUT="server_resa"
CONSULT_SERVER_OUT="server_consult"

# Compilation
GCC_FLAGS= "" #"-Wall -Werror"
echo "Compiling client..."
gcc $GCC_FLAGS -o $CLIENT_OUT $CLIENT_SRC
if [$? -ne 0]; then
    echo "Echec de la compilation du client."
    exit 1
fi

echo "Compilation serveur de reservation..."
gcc $GCC_FLAGS -o $RESA_SERVER_OUT $RESA_SERVER_SRC
if [$? -ne 0]; then
    echo "Echec de la compilation du serveur de reservation."
    exit 1
fi

echo "Compilation serveur de consultation..."
gcc $GCC_FLAGS -o $CONSULT_SERVER_OUT $CONSULT_SERVER_SRC
if [$? -ne 0]; then
    echo "Echec de la compilation du serveur de consultation."
    exit 1
fi

echo "Compilation successful."

# Lancement des servers
echo "Lancement des serveurs..."
./$CONSULT_SERVER_OUT
./$RESA_SERVER_OUT 

sleep 1 # Delai pour l'init des serveurs

echo "Lancement du client..."
./$CLIENT_OUT 
