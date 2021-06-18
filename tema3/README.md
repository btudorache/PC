# Tema 3 protocoale de comunicatii - Client web. Comunicatie cu un REST API

324CA Tudorache Bogdan Mihai

Tema contine implementarea unui client care poate sa trimita **requesturi HTTP** catre un **REST API**.

Clientul functioneaza ca o interfata care comunica cu un server care expune API-ul unei biblioteci online.
Exista cateva comenzi pe care clientul stie sa le interpreteze, si pe care le "imbraca" intr-un request 
catre server.

# Implementare

Tema contine doua etape de implementare: parsarea si construirea de obiecte JSON folosind biblioteca 
[parson](https://github.com/kgabis/parson), si construirea corecta a headerelor HTTP. 

## Biblioteca Parson

Am folosit intens API-ul expus de biblioteca parson pentru a construi si interpreta obiecte JSON. 

Pentru cererile de tip POST, am folosit functii precum ```json_value_init_object()```, 
```json_object_set_string(JSON_object, key, value)```, ```json_object_set_number(JSON_object, key, value)``` 
pentru construirea si adaugarea de campuri in obiectele JSON definite in headerul bibliotecii. 

In cazul in care primeam raspunsuri sub forma unor date JSON, am folosit ```json_parse_string(strstr(response, "{"))```
pentru gasirea stringului JSON si parsarea lui intr-un obiect corespunzator. Diferitele campuri din obiectele JSON
au fost extrase folosind ```json_object_get_string(JSON_object, key)```. De fiecare data cand a fost nevoie sa transform
obiectul JSON intr-un string, am folosit ```json_serialize_to_string_pretty(JSON_object)```.

## Implementarea comenzilor

Serverul functioneaza intr-o bucla while infinita, care primeste pe rand comenzi si le interpreteaza. La fiecare
comanda noua deschid o noua conexiune catre server, iar la finalul comenzii o inchid.

* register - Se asteapta de la tastatura numele si parola, dupa care se construieste o cerere POST si 
se trimite la server. Daca nu primim nici un obiect JSON cu un camp de eroare, inseamna ca inregistrarea
a avut loc cu succes.

* login - Asemanator ca si la register, doar ca aici extragem cookie-ul de login din request si il pastram
pentru requesturile ulterioare pentru a demonstra ca clientul s-a autentificat.

* logout - Pentru logout este necesar cookie-ul de login. Daca exista cookie-ul in memoria interna, il folosim
pentru a construi o cerere GET. In cazul in care primim o eroare, o extragem din response si o afisam

* enter_library - Asemanator ca la logout, avem nevoie de cookie-ul de login pentru a intra in biblioteca. Daca
requestul are loc cu succes, extragem tokenul JWT pentru a avea acces la functionalitatile CRUD. Altfel, afisam
mesajul de eroare din request

* get_books - Facem o cerere GET folosind tokeul JWT. Daca cererea are loc cu succes, afisam lista de carti 
intoarsa, altfel afisam mesajul de eroare din response.

* get_book - Asemanator cu get_books, numai ca aici trebuie sa construim url-ul corect in functie de inputul
primit de la tastatura

* add_book - Construim obiectul JSON pentru cererea POST in functie de inputul primit de la tastatura, si 
trimitem requestul cu tot cu tokenul JWT. In caz ca nu primim nici o eroare, cartea s-a adaugat cu succes, 
altfel afisam eroarea din response.

* delete_book - Comanda aproape identica cu get_book, doar ca trimitem un request de tipul DELETE in loc de GET

* exit - Opreste conexiunea si inchide clientul
