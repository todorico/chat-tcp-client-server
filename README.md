# Description

Notre projet ce compose de deux programmes, un client et un serveur qui vont
permettre la mise en place d'un chat concurrent ou chaque clients pourra 
communiquer en TCP avec les autres clients connectés au serveur via un terminal.

Le premier programme "server" va permettre de demarrer un serveur concurrent via
un numero de port, il traitera ensuite les requetes des clients.

Le deuxieme programme "client" va se connecter à un serveur distant via son nom 
d'hote et son numero port, se synchroniser avec cd serveur pour récupérer les
messages deja envoyés par les autres clients et enfin permettre la communication avec
les autres clients message par message.

# Compilation

Ouvrez un terminal dans le repertoire du projet et tapez :

```bash
make
#   ou
mkdir -p bin ; gcc -Wall -Wextra server.c -o bin/server -lpthread ; gcc -Wall -Wextra client.c -o bin/client -lpthread 
```

# Execution

Ouvrez un terminal dans le repertoire du projet, compilez puis mettez en place un serveur
serveur. Usage : ./bin/server <NUMERO_PORT> (Avec NUMERO_PORT > 1024).
Une fois mis en place le serveur indiquera sont nom d'hote et le numero de port sur lequel ce connecté.

```bash
./bin/server 1234
```

Ensuite connectez vous à ce serveur en executant le client : Usage : ./bin/client <NOM_HOTE> <NUMERO_PORT>
(Avec NOM_HOTE et NUMERO_PORT respectivement le nom d'hote et numero de port du serveur sur lequel vous voulez vous connecter)

```bash
./bin/client NOM_HOTE 1234 
```
Vous pouvez maintenant communiquer avec les autres clients connectés au même serveur.