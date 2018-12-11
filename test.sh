#!/bin/bash

# Script de test pour les programmes client et server 
# Attention à bien modifié la variable TERM si vous n'avez pas xterm

if [ $# -ne 2 ]; then
   echo "Usage : $0 <NOM_HOTE> <NUMERO_PORT>"
   exit 1
fi

SERV_ADDR=$1
SERV_PORT=$2
TERM=xterm

# Serie de test N°1 :

$TERM -e "./bin/server $SERV_PORT; $SHELL" &
sleep 1
$TERM -e "./bin/server $SERV_PORT; $SHELL" &
sleep 1
$TERM -e "./bin/server $SERV_PORT; $SHELL" &
sleep 1
$TERM -e "echo message | ./bin/client $SERV_ADDR $SERV_PORT; $SHELL" &

# Serie de test N°2

$TERM -e "./bin/server $SERV_PORT; $SHELL" &
sleep 1
$TERM -e "./bin/client  $SERV_ADDR $SERV_PORT; $SHELL" &
sleep 1
$TERM -e "./bin/server $SERV_PORT; $SHELL" &
sleep 1
$TERM -e "echo message | ./bin/client $SERV_ADDR $SERV_PORT; $SHELL" &

# Serie de test N°3

$TERM -e "./bin/server $SERV_PORT; $SHELL" &
sleep 1

for i in {1..11}; do $TERM -e "./bin/client $SERV_ADDR $SERV_PORT; $SHELL" & done
for pid in $(ps -ef | grep ./bin/client | awk '{print $2}'); do
	kill $pid
	break
done
		       
# Serie de test N°4

$TERM -e "./bin/server $SERV_PORT; $SHELL" &
sleep 1

for i in {1..10}; do $TERM -e "./bin/client $SERV_ADDR $SERV_PORT; $SHELL" & done
sleep 1
for pid in $(ps -ef | grep ./bin/server | awk '{print $2}'); do
    kill $pid
    break
done

