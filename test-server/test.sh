#!/bin/bash

# Script de test pour les programmes client et server

if [ $# -ne 2 ]; then
   echo "Usage : $0 <ADRESSE_SERVEUR> <NUMERO_PORT>"
   exit 1
fi

SERV_ADDR=$1
SERV_PORT=$2

# Serie de test N°1 :

#xterm -e "./bin/server $SERV_PORT; $SHELL" &
#sleep 1
#xterm -e "./bin/server $SERV_PORT; $SHELL" &
#sleep 1
#xterm -e "./bin/server $SERV_PORT; $SHELL" &
#sleep 1
#xterm -e "echo message | ./bin/client $SERV_ADDR $SERV_PORT; $SHELL" &

# Serie de test N°2

#xterm -e "./bin/server $SERV_PORT; $SHELL" &
#sleep 1
#xterm -e "./bin/client  $SERV_ADDR $SERV_PORT; $SHELL" &
#sleep 1
#xterm -e "./bin/server $SERV_PORT; $SHELL" &
#sleep 1
#xterm -e "echo message | ./bin/client $SERV_ADDR $SERV_PORT; $SHELL" &

# Serie de test N°3

#xterm -e "./bin/server $SERV_PORT; $SHELL" &
#sleep 1

#for i in {1..11}; do xterm -e "./bin/client $SERV_ADDR $SERV_PORT; $SHELL" & done

#for pid in $(ps -ef | grep ./bin/client | awk '{print $2}'); do
#    kill $pid
#    break
#done
		       
#Serie de test N°4

xterm -e "./bin/server $SERV_PORT; $SHELL" &
sleep 1

for i in {1..100}; do xterm -e "./bin/client $SERV_ADDR $SERV_PORT; $SHELL" & done
sleep 1
for pid in $(ps -ef | grep ./bin/server | awk '{print $2}'); do
    kill $pid
    break
done
