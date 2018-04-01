#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFLEN 256

typedef struct {
    char nume[12];
    char prenume[12];
    char nrCard[6];
    char pin[4];
    char secretPass[16];
    float sold;
    int isLoggedIn;
    int tries;
} User;

void error(char *msg) {
    perror(msg);
    exit(1);
}

void readUsers(User users[], int n, FILE *f) {
    int i;
    for (i = 0; i < n; i++) {
        fscanf(f, "%s %s %s %s %s %f", users[i].nume, users[i].prenume, users[i].nrCard, users[i].pin,
               users[i].secretPass, &users[i].sold);
        users[i].isLoggedIn = 0;
        users[i].tries = 0;
    }
}

int main(int argc, char *argv[]) {

    // ---- intro ----
    FILE *f = fopen(argv[2], "r");
    int n;
    fscanf(f, "%d", &n);

    User users[n];
    readUsers(users, n, f);
    fclose(f);

    // ---- content ----

    struct sockaddr_in serv_addr_tcp, serv_addr_udp, cli_addr;
    char buffer[BUFLEN];
    int sockfd_tcp, sockfd_udp, newsockfd, portno, clilen;
    int i, j, k = 0;
    unsigned int addrlen;
    int logged[100];

    fd_set read_fds;    //multimea de citire folosita in select()
    fd_set tmp_fds;    //multime folosita temporar
    int fdmax;        //valoare maxima file descriptor din multimea read_fds

    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);

    sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    sockfd_udp = socket(PF_INET, SOCK_DGRAM, 0);
    portno = atoi(argv[1]);

    memset((char *) &serv_addr_tcp, 0, sizeof(serv_addr_tcp));
    memset((char *) &serv_addr_udp, 0, sizeof(serv_addr_udp));

    serv_addr_tcp.sin_family = AF_INET;
    serv_addr_tcp.sin_addr.s_addr = INADDR_ANY;    // foloseste adresa IP a masinii
    serv_addr_tcp.sin_port = htons(portno);

    serv_addr_udp.sin_family = AF_INET;
    serv_addr_udp.sin_addr.s_addr = INADDR_ANY;
    serv_addr_udp.sin_port = htons(portno);

    if (bind(sockfd_tcp, (struct sockaddr *) &serv_addr_tcp, sizeof(struct sockaddr)) < 0)
        error("ERROR on binding TCP");

    if (bind(sockfd_udp, (struct sockaddr *) &serv_addr_udp, sizeof(struct sockaddr)) < 0)
        error("ERROR on binding UDP");

    listen(sockfd_tcp, 20);

    FD_SET(sockfd_tcp, &read_fds);
    FD_SET(sockfd_udp, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    fdmax = sockfd_udp + 1;

    while (1) {
        tmp_fds = read_fds;
        if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
            error("ERROR in select");

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {

                // transmit de la stdin -> quit
                if (i == STDIN_FILENO) {
                    memset(buffer, 0, BUFLEN);
                    scanf("%s", buffer);
                    if (strncmp(buffer, "quit", 5) == 0) {
                        strcpy(buffer, "quit\n");
						
						// trimit tuturor clientilor logati quit
                        for (j = 0; j < k; j++) {
                            int s = send(logged[j], buffer, strlen(buffer), 0);
                            if (s < 0)
                                error("S: ERROR writing to socket");
                        }

                        close(sockfd_tcp);
                        close(sockfd_udp);
                        exit(1);
                    }
                    continue;
                }

                if (i == sockfd_udp) {
                    recvfrom(i, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, &addrlen);

                    // am prmit unlock pe UDP
                    char *input = strtok(buffer, " ");
                    if (strncmp(input, "unlock", 6) == 0) {
                        char *nrCard = strtok(NULL, " ");

                        for (j = 0; j < n; j++) {
                            if (strncmp(users[j].nrCard, nrCard, strlen(nrCard) - 1) == 0) {
                                break;
                            }
                        }

                        if (j == n) {
                            strcpy(buffer, "UNLOCK> -4 : Numar card inexistent\n");
                            int s = sendto(i, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, addrlen);
                            if (s < 0)
                                error("S: ERROR writing to socket");
                            continue;
                        }

                        if (users[j].tries < 3) {
                            strcpy(buffer, "UNLOCK> -6 : Operatie esuata\n");
                            int s = sendto(i, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, addrlen);
                            if (s < 0)
                                error("S: ERROR writing to socket");

                        } else {
                            strcpy(buffer, "UNLOCK> Trimite parola secreta\n");
                            sendto(i, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, addrlen);
                            recvfrom(i, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, &addrlen);

                            if (strncmp(users[j].secretPass, buffer, strlen(users[j].secretPass)) != 0) {
                                strcpy(buffer, "UNLOCK> -7 : Deblocare esuata\n");
                                int s = sendto(i, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, addrlen);
                                if (s < 0)
                                    error("S: ERROR writing to socket");
                            } else {
                                strcpy(buffer, "UNLOCK> Client deblocat\n");
                                users[j].tries = 0;
                                int s = sendto(i, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, addrlen);
                                if (s < 0)
                                    error("S: ERROR writing to socket");
                            }
                        }

                    }
                    continue;
                }

                if (i == sockfd_tcp) {
                    // a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
                    // actiunea serverului: accept()
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(sockfd_tcp, (struct sockaddr *) &cli_addr, &clilen);
                    logged[k] = newsockfd;
                    k++;

                    //adaug noul socket intors de accept() la multimea descriptorilor de citire
                    FD_SET(newsockfd, &read_fds);
                    if (newsockfd > fdmax) {
                        fdmax = newsockfd;
                    }
					
                } else {
                    // am primit date pe unul din socketii cu care vorbesc cu clientii
                    //actiunea serverului: recv()
                    memset(buffer, 0, BUFLEN);
                    int v;
                    if ((v = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
                        if (v == 0) {
                            //conexiunea s-a inchis
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            error("ERROR in recv");
                        }
                        close(i);
                        FD_CLR(i, &read_fds); // scoatem din multimea de citire socketul pe care
                    } else { //recv intoarce >0
                        // am primit un mesaj pe TCP
                        char *input = strtok(buffer, " ");

                        if (strncmp(input, "login", 5) == 0) {
                            input = strtok(NULL, " ");
                            int found = 0;

                            for (j = 0; j < n; j++) {
                                if (strncmp(users[j].nrCard, input, 6) == 0) {
                                    found = 1;
                                    break;
                                }
                            }

                            if (users[j].isLoggedIn == 1) {
                                strcpy(buffer, "ATM> -2 : Sesiune deja deschisa\n");
                                int s = send(i, buffer, strlen(buffer), 0);
                                if (s < 0)
                                    error("S: ERROR writing to socket");
                            } else {

                                if (found == 0) {
                                    strcpy(buffer, "ATM> -4 : Numar card inexistent\n");
                                    int s = send(i, buffer, strlen(buffer), 0);
                                    if (s < 0)
                                        error("S: ERROR writing to socket");
                                } else {

                                    if (users[j].tries >= 3) {
                                        strcpy(buffer, "ATM> -5 : Card blocat\n");
                                        int s = send(i, buffer, strlen(buffer), 0);
                                        if (s < 0)
                                            error("S: ERROR writing to socket");
                                        continue;
                                    }

                                    input = strtok(NULL, " ");
                                    if (strncmp(users[j].pin, input, 4) != 0) {
                                        strcpy(buffer, "ATM> -3 : Pin gresit\n");
                                        users[j].tries++;
                                        int s = send(i, buffer, strlen(buffer), 0);
                                        if (s < 0)
                                            error("S: ERROR writing to socket");
                                    } else {
                                        char aux[BUFLEN] = "";
                                        strncpy(aux, users[j].nrCard, 6);
                                        strcat(aux, "ATM> Welcome ");
                                        strcat(aux, users[j].nume);
                                        strcat(aux, " ");
                                        strcat(aux, users[j].prenume);
                                        strcat(aux, "\n\0");
                                        strncpy(buffer, aux, strlen(aux));
                                        users[j].isLoggedIn = 1;
                                        users[j].tries = 0;
                                        int s = send(i, buffer, strlen(buffer), 0);
                                        if (s < 0)
                                            error("S: ERROR writing to socket");
                                    }
                                }
                            }
                        }

                        if (strncmp(input, "logout", 5) == 0) {
                            char *nrCard = strtok(NULL, " ");

                            for (j = 0; j < n; j++) {
                                if (strncmp(users[j].nrCard, nrCard, strlen(nrCard) - 1) == 0) {
                                    break;
                                }
                            }

                            users[j].isLoggedIn = 0;

                            strcpy(buffer, "ATM> Deconectare de la bancomat\n");
                            int s = send(i, buffer, strlen(buffer), 0);
                            if (s < 0)
                                error("S: ERROR writing to socket");
                        }

                        if (strncmp(input, "listsold", 8) == 0) {
                            char *nrCard = strtok(NULL, " ");

                            for (j = 0; j < n; j++) {
                                if (strncmp(users[j].nrCard, nrCard, strlen(nrCard) - 1) == 0) {
                                    break;
                                }
                            }
                            char aux[100];
                            sprintf(aux, "%.2f", users[j].sold);
                            strcpy(buffer, "ATM> ");
                            strcat(buffer, aux);
                            strcat(buffer, "\n");

                            int s = send(i, buffer, strlen(buffer), 0);
                            if (s < 0)
                                error("S: ERROR writing to socket");
                        }

                        if (strncmp(input, "getmoney", 8) == 0) {
                            char *amount = strtok(NULL, " ");
                            char *nrCard = strtok(NULL, " ");

                            for (j = 0; j < n; j++) {
                                if (strncmp(users[j].nrCard, nrCard, strlen(nrCard) - 1) == 0) {
                                    break;
                                }
                            }

                            if (strcmp(amount, "0") == 0 || amount[strlen(amount) - 1] != '0') {
                                strcpy(buffer, "ATM> -9 : Suma nu este multiplu de 10\n");
                                int s = send(i, buffer, strlen(buffer), 0);
                                if (s < 0)
                                    error("S: ERROR writing to socket");
                            } else {
                                if (users[j].sold < atoi(amount)) {
                                    strcpy(buffer, "ATM> -8 : Fonduri insuficiente\n");
                                    int s = send(i, buffer, strlen(buffer), 0);
                                    if (s < 0)
                                        error("S: ERROR writing to socket");
                                } else {
                                    users[j].sold -= atoi(amount);
                                    strcpy(buffer, "Suma ");
                                    strcat(buffer, amount);
                                    strcat(buffer, " ");
                                    strcat(buffer, "retrasa cu succes\n");
                                    int s = send(i, buffer, strlen(buffer), 0);
                                    if (s < 0)
                                        error("S: ERROR writing to socket");
                                }

                            }

                        }

                        if (strncmp(input, "putmoney", 8) == 0) {
                            char *amount = strtok(NULL, " ");
                            char *nrCard = strtok(NULL, " ");

                            for (j = 0; j < n; j++) {
                                if (strncmp(users[j].nrCard, nrCard, strlen(nrCard) - 1) == 0) {
                                    break;
                                }
                            }

                            printf("amount = %s\n", amount);
                            float aux;
                            sscanf(amount, "%f\n", &aux);
                            printf("%.2f\n", aux);
                            users[j].sold += aux;
                            strcpy(buffer, "Suma ");
                            strcat(buffer, amount);
                            strcat(buffer, " ");
                            strcat(buffer, "depusa cu succes\n");
                            int s = send(i, buffer, strlen(buffer), 0);
                            if (s < 0)
                                error("S: ERROR writing to socket");
                        }

                        if (strncmp(input, "quit", 4) == 0) {
                            char *nrCard = strtok(NULL, " ");

                            for (j = 0; j < n; j++) {
                                if (strncmp(users[j].nrCard, nrCard, strlen(nrCard) - 1) == 0) {
                                    break;
                                }
                            }

                            users[j].isLoggedIn = 0;
                            close(i);

                            // scoatem din multimea de citire socketul pe care am comunicat cu clientul
                            FD_CLR(i, &read_fds);
                        }
                    }
                }
            }
        }
    }
}