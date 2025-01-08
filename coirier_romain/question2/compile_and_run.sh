#!/bin/bash

# Sources
CLIENT_SRC="client.c" 
SERVER_SRC="server.c"

# Executables
CLIENT_OUT="client"
SERVER_OUT="server"

# Compilation
GCC_FLAGS= "" #"-Wall -Werror"
echo "Compilation du client..."
gcc $GCC_FLAGS -o $CLIENT_OUT $CLIENT_SRC
if [$? -ne 0]; then
    echo "Echec de la compilation du client."
    exit 1
fi

echo "Compilation serveur..."
gcc $GCC_FLAGS -o $SERVER_OUT $SERVER_SRC
if [$? -ne 0]; then
    echo "Echec de la compilation du serveur."
    exit 1
fi

echo "Succes de la compilation."

# Lancement du server
echo "Lancement du serveur..."
./$SERVER_OUT
sleep 1 # Delai pour l'init des serveurs

echo "Lancement du client..."
./$CLIENT_OUT 
