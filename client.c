#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define BUFLEN 256

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[]) {
    int sockfd_tcp, sockfd_udp, n;
    int session = 0;
    struct sockaddr_in serv_addr_tcp, serv_addr_udp;
    char loggedUser[100] = "";
    unsigned int addrlen = sizeof(struct sockaddr);
    char buffer[BUFLEN];

    // pregatesc fisierul de log
    char pid[10];
    sprintf(pid, "%d", getpid());
    char toWrite[100] = "client";
    strcat(toWrite, pid);
    strcat(toWrite, ".log");
    FILE *f = fopen(toWrite, "w");

    sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_tcp < 0)
        error("C: ERROR opening socket");

    sockfd_udp = socket(PF_INET, SOCK_DGRAM, 0);

    serv_addr_tcp.sin_family = AF_INET;
    serv_addr_tcp.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr_tcp.sin_addr);

    serv_addr_udp.sin_family = AF_INET;
    serv_addr_udp.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr_udp.sin_addr);

    if (connect(sockfd_tcp, (struct sockaddr *) &serv_addr_tcp, sizeof(serv_addr_tcp)) < 0)
        error("C: ERROR connecting");

    while (1) {

        //citesc de la tastatura
        memset(buffer, 0, BUFLEN);
        fgets(buffer, BUFLEN - 1, stdin);

        fprintf(f, "%s", buffer);

        if(strncmp(buffer, "login", 5) != 0 && session == 0){
            printf("-1 : Clientul nu este autentificat\n");
            continue;
        }

        if (strncmp(buffer, "unlock", 6) == 0) {
            strcpy(buffer, "unlock ");
            strcat(buffer, loggedUser);
            strcat(buffer, "\n");
            sendto(sockfd_udp, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, addrlen);
            recvfrom(sockfd_udp, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, &addrlen);
            printf("%s", buffer);
            if (strncmp(buffer, "UNLOCK> T", 9) == 0) {
                fgets(buffer, BUFLEN - 1, stdin);
                sendto(sockfd_udp, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, addrlen);
                recvfrom(sockfd_udp, buffer, BUFLEN, 0, (struct sockaddr *) &serv_addr_udp, &addrlen);
                printf("%s", buffer);
            }
            continue;
        }

        if (strncmp(buffer, "login", 5) == 0 && session == 1) {
            printf("-2 : Sesiune deja deschisa\n");
            continue;
        }

        if (strncmp(buffer, "logout", 6) == 0) {
            if (session == 0) {
                printf("-1 : Clientul nu este autentificat\n");
                continue;
            } else {
                strcpy(buffer, "logout ");
                strcat(buffer, loggedUser);
                strcat(buffer, "\n");
                session = 0;
            }
        }

        if (strncmp(buffer, "listsold", 8) == 0) {
            strcpy(buffer, "listsold ");
            strcat(buffer, loggedUser);
            strcat(buffer, "\n");
        }

        if (strncmp(buffer, "getmoney", 8) == 0) {
            strcpy(buffer + strlen(buffer) - 1, " ");
            strcat(buffer, loggedUser);
            strcat(buffer, "\n");
        }

        if (strncmp(buffer, "putmoney", 8) == 0) {
            strcpy(buffer + strlen(buffer) - 1, " ");
            strcat(buffer, loggedUser);
            strcat(buffer, "\n");
        }

        if (strncmp(buffer, "quit", 4) == 0) {
            strcpy(buffer, "quit ");
            strcat(buffer, loggedUser);
            strcat(buffer, "\n");
            n = send(sockfd_tcp, buffer, strlen(buffer), 0);
            if (n < 0)
                error("C: ERROR writing to socket");
            break;
        }

        // trimit mesaj la server
        n = send(sockfd_tcp, buffer, strlen(buffer), 0);
        if (n < 0)
            error("C: ERROR writing to socket");

        // primesc mesaj de la server
        memset(buffer, 0, BUFLEN);
        n = recv(sockfd_tcp, buffer, sizeof(buffer), 0);
        if (n < 0)
            error("C: ERROR receiving from socket");

        // verifi daca login a fost finalizat cu succes
        if (strncmp(buffer + 6, "ATM> Welcome", 12) == 0) {
            session = 1;
            strncpy(loggedUser, buffer, 6);
            strcpy(buffer, buffer + 6);
        }

        if (strncmp(buffer, "quit", 4) == 0)
            break;

        fprintf(f, "%s", buffer);
        printf("%s", buffer);
    }

    fclose(f);
    // inchid file descriptorii
    close(sockfd_tcp);
    close(sockfd_udp);

    return 0;
}