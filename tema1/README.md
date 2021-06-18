# Tema 1 protocoale de comunicatii - Router

324CA Tudorache Bogdan Mihai

Tema contine implementarea unor protocoale si algoritmi necesari pentru a simula functionalitatile unui 
router. Pe scurt, logica programului este urmatoarea: 

Se parseaza tabela de rutare, dupa care incepe procesarea continua a pachetelor. 

Daca pachetul este unul **IP/ICMP**, se transmite mai departe daca se poate, cu tot cu prelucrarile asociate, 
sau se descopera noi adrese mac prin requesturi de tip ARP. 

Daca pachetul este de **tip ARP**, routerul raspunde la arp requests sau memoreaza informatiile din arp replies, si 
trimite mai departe pachetele din coada.

# Descrierea cerintelor

## Parsarea tabelei de rutare

Tabela de rutare este parsata cu ajutorul functiei 
```struct route_table_entry* parse_route_table(char* rtable_name, int* rtable_size)```. Aceastea numara 
liniile din fisierul de la input, aloca un array de table_entry-uri, parseaza fiecare linie in parte si o salveaza 
in array-ul de entry-uri de tip ```struct route_table_entry```. 

La final, este folosit ```qsort``` pentru a sorta tabela de rutare in **O(nlogn)**, folosind comparatorul 
```int route_entry_comparator(const void* first, const void* second)```, pentru a facilita gasirea celui mai 
bun entry din tabela de rutare in **O(logn)**.

## Implementarea protocolului ARP

Prima data se verifica daca pachetul ethernet primit este de tip **ETHERTYPE_ARP**. In caz afirmativ, avem 
doua situatii: 

* Daca headerul arp este de tip **ARPOP_REQUEST**, routerul trimite inapoi pe interfata pe care s-a facut requestul 
adresa mac a interfetei, printr-un pachet de tip **ARPOP_REPLY**. 

* Daca headerul arp este de tip **ARPOP_REPLY**, se salveaza noua adresa mac descoperita intr-o structura 
de array-uri de tip ```struct arp_entry```, si se trimit toate pachetele pastrate in coada asociata interfetei 
pe interfata pe care s-a primit reply-ul.

## Procesul de dirijare

Cand primim un pachet ip, verificam intai daca s-a pastrat checksum-ul integru si daca ttl este mai mare 
ca 1. Verificam apoi daca ip-ul destinatie din header se gaseste in tabela de rutare a routerului. 

**Algoritmul de rutare** este implementat folosind cautare binara, cu o complexitate de **O(logn)**. 

La pasul urmator, vedem daca avem adresa mac a urmatorului hop in tabela arp a routerului. In caz afirmativ, se 
transmite pachetul mai departe fara probleme. 

Altfel, trimitem un pachet de tip **ARPOP_REQUEST** catre urmatorul hop cu adresa mac de broadcast ff:ff:ff:ff:ff:ff 
si punem pachetul ip curent intr-o coada asociata interfetei pe care se doreste sa se faca transmisia. Cand 
vom primi reply-ul asociat requestului, se vor trimite toate pachetele dincoada pe interfata pe care se 
face transmisia.

## Implementarea protocolului ICMP

Avem urmatoarele trei tipuri de pachete:

* Daca routerul primeste in mod direct un pachet **ICMP_ECHO**, raspunde cu un pachet icmp de tip 
**ICMP_ECHOREPLY**. 

* Daca ttl-ul unui pachet ip este mai mic sau egal cu 1, raspunde cu un pachet icmpde tip **ICMP_TIME_EXCEEDED**. 

* Daca routerul nu gaseste next-hop-ul unui asociat unui pachet ip primit, raspunde cu un pachet icmp de tip 
**ICMP_DEST_UNREACH**.