# Tema 2 protocoale de comunicatii - Aplicatie client-server TCP si UDP

324CA Tudorache Bogdan Mihai

Tema contine implementarea unei aplicatii **client-server** ce foloseste protocoalele **TCP si UDP**. Descrierea
pe scurt a aplicatiei: 

* Clientii ce folosesc TCP se pot abona la niste topic feed-uri ce folosesc protocolul 
UPD prin intermediul unui server. 

* Serverul se ocupa de multiplexarea si trimiterea mesajelor de la clientii UDP
catre clientii TCP abonati. 

* Comunicarea facila intre server si clientii TCP se realizeaza printr-un protocol 
simplist: stabilirea unei structuri si dimensiuni anume prin care fac comunicarea cele doua entitati.

# Implementare

Implementarea contine 4 etape: definirea structurilor folosite intern de catre server, implementarea serverului, 
implementarea clientului si definirea protocolului de comunicatie intre server si client;

## Descrierea Structurilor folosite

Structurile folosite (in cea mai mare parte in server) sunt urmatoarele:

* struct udp_message - structura unui mesaj primit de la clientii udp, in conformitate cu enuntul temei.

* struct server_message - structura folosita pentru comunicarea intre clientii TCP si server. Contine toate
datele necesare pentru clienti (mesajul de la clientii udp si adresa clientului udp care a trimis mesajul).

* struct tcp_map_entry - structura folosita pentru maparea dintre sockfd-ul folosit intern de program si
datele clientului TCP cu care face legatura (addresa clientului si id-ul lui)

* struct subscriber_entry - reprezentarea interna a unui subscriber; contine toate datele necesare despre un
subscriber (id, daca e conectat, lista de topicuri cu care a interactionat, o coada pentru functionalitatea
sf, adresa clientului)

* struct topic_entru - reprezentarea interna a unui topic; contine toate datele necesare despre un topic
(nume, daca sf e activ, daca clientul care detine topic-ul este abonat actualmente la acest topic)

## Implementarea serverului

Serverul se ocupa de multiplexarea dintre stdin, mesajele primite pe portul UDP si clienti TCP. 

Conexiunea dintre un client TCP si server se face in modul urmator: clientul trimite o cerere de conectare
si un mesaj prin care trimitel id-ul propriu (identificat prin mesajul "connect *id*". Serverul accepta cererea 
de conectare, si apoi primeste mesajul cu id-ul clientului.

In aceasta etapa se foloseste un array de structuri tcp_map_entry pentru a pastra maparea dintre socket-ul 
intern si datele de identificare ale clientului. Daca un client cu acelasi id este deja conectat, se 
respinge cererea de conectare.

Serverul foloseste o lista de structuri de tip **struct subscriber_entry** pentru a stoca informatiile despre
subscriberi in mod dinamic.

La o cerere de conectare, daca clientul a fost connectat anterior si are mesaje stocate gratie functionalitatii de 
store and forward, in momentul conectarii clientul primeste toate mesajele pastrate intr-o coada.

Cand serverul primeste un mesaj de la clientii UDP, inceaca sa trimita mesajul tuturor clientilor abonati, iar 
pentru cei abonati dar deconectati, stocheaza mesajele intr-o coada asociata fiecarui client in parte.

Singurul mesaj pe care clientul il poate primi de la tastatura este "exit", prin care anunta toti clientii sa
termine conexiunea.

Cand primeste mesaje de la clientii TCP, interpreteaza cererile de tip "subscribe *topic* *sf*", 
"unsubscribe *topic*" si "exit" corespunzator (modifica reprezentarea interna a unui client, sau inchide conexiunea)

## Implementarea clientului

Clientul se poate connecta la server (procedeul este discutat mai sus) si ii poate trimite comenzi de tipul 
"subscribe *topic* *sf*", "unsubscribe *topic*" si "exit". 

De asemenea poate interpreta cele 4 tipuri de topicuri: INT, SHORT_REAL, FLOAT, STRING, si afiseaza mesajele 
primite in mod corect.

## Protocolul client-server

Metoda care faciliteaza comunicarea corecta dintre clientii TCP si server este urmatoarea: comunicarea se face doar
prin mesaje de tip **struct server_message**. Astfel, dimensiunea mesajului trimis de server si cea a mesajului
receptat de client este de aceeasi lungime, si se evita problemele aduse de trunchierile/concatenarile interne
create de catre protocolul TCP.


