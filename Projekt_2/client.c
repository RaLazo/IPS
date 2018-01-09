/************************
 Name: Client - Toilet
 Autor: Rafael Lazenhofer
 Version: 1.0
 Date: 06.01.2018
 ***********************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <time.h>
//Unterprogramm zum Error handling
void error(const char *msg)
{
    perror(msg);
    exit(0);
}
// Messagetyp auswahl
int value(char *str) {
    char *words[]={"HELO","ENTR","BLOK","LEFT"}, str2[10];
    int i,j=0;
    bzero(str2,10);
    /*********************************
     Aussortieren für der unwichtigen
     Zeichen
    **********************************/
    for(i=6;i<10;i++)
    {
        if((strcmp(str,";"))==0)
            break;
        str2[j]=str[i];
        j++;
    }
    /*************************************
     Wert in Messagetyp in Integer 
     umwandeln für switch im Hauptprogramm
    **************************************/
    for (i = 0; i < sizeof words/sizeof words[0]; i++) {
        if ((strcmp(str2, words[i]) == 0)) {
            return i;
        }
    }
}
int main(int argc, char *argv[])
{
    /********************* 
    Varibalen definierten
    **********************/
    int sockfd, portno, n, random_variable, id=1, m;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];
    char mesg[256]="$0001:HELO;gender=M#\r\n";
    
    /********************************** 
    Abfrage ob alle Argumente beim Start
    mit geben wurden
     wenn diese nicht mit gelfiefert 
    werden wird das Programm abgebrochen
    ***********************************/
    if (argc < 4) {
       fprintf(stderr,"usage %s hostname port gender\n", argv[0]);
       exit(0);
    }
    /****************************************************
     atoi() macht aus einem String eine Int Zahl
    
    (Hier wird der String in dem die Portnr. enthalten ist 
    umgewandelt in eine Int Zahl)
     ***************************************************/
    portno = atoi(argv[2]);
    /********************************************************************
     socket()
     AF_INET => IPv4 
     SOCK_STEAM => Unterstützt eine zuverlässig Byte-Stream-Kommunikation
     0 => Für das nötige Protokoll
     *******************************************************************/
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    /*********************************
     Überprüfung ob Socket zugänglich
    **********************************/
    if (sockfd < 0) 
        error("ERROR opening socket");
    
    /*********************************
     Überprüfen ob Host zugänglich ist 
     z.B.: 127.0.0.1 //localhost
    *********************************/
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    /***************************************
     bzero()
     Setzt alle Elemente auf 0 von serv_addr
     ***************************************/
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET; //IPv4
    /**************************************
     bcopy()
     eine Anzahl von Zeichen in einer 
     andere Zeichenfolge kopieren
    ***************************************/
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);    
    
    serv_addr.sin_port = htons(portno);//Port setzen
    /*Zum Server verbinden*/
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    /*******************************
     Beginn der Kommunikation 
     zwischen Client & Server
     *******************************/
    printf("Person: %s\n",argv[3]);
    do
    {
    
     bzero(buffer,256);
    /***********************************************
     Message auswahl
     Aufbau:
     [Befehl-NR][START][NACHRICHT][END]
     [START] . . . . ID wird jedes mal inkrementiert

     1.1 HALO . . . . senden vom Geschlecht
     1.2 EXIT . . . . rausgehen nach dem man fertig ist
	 1.3 BLOK . . . . Man muss warten
	 1.4 ENTR . . . . Man darf das Klo betreten
     **********************************************/
     m=value(mesg); 
     switch(m)
     {
         case 0:
                sprintf(buffer, "$%04d:HELO;gender=%s#\r\n",id++,argv[3]);
                printf("%s: %s",argv[3],buffer);
         break;
         case 1:
                /*******************************
                Als Zufallsgenertator wird eine 
				Zahl von 1 bis 10 generiert
                ******************************/
                srand(time(NULL));
                random_variable = rand()%10;
				sleep(random_variable);
                sprintf(buffer, "$%04d:EXIT;gender=%s#\r\n",id++,argv[3]);
                printf("%s: %s",argv[3],buffer);
         break;
         case 2:
         printf("You have to wait until it is space!\n");	 
		 sprintf(buffer, "$%04d:HELO;gender=%s#\r\n",id++,argv[3]);
		 sleep(3);
         break;
		 
		 case 3:
		 //printf("Toilet: %s\n",mesg);
		 exit(0);
		 break;
         
		 default: 
         puts("Unkown Message!"); 
         exit(0);
         break;
    
     }
    
	// Schreiben
    n = write(sockfd,buffer,strlen(buffer));
	
    /*********************************
     ERROR Handling falls beim Befehl 
     write ()
     etwas fehlschlägt
    **********************************/
    if (n < 0) 
         error("ERROR writing to socket");
    // Lesen
    bzero(mesg,256);
    n = read(sockfd,mesg,255);
    /*********************************
     ERROR Handling falls beim Befehl 
     read()
     etwas fehlschlägt
    **********************************/
    if (n < 0) 
         error("ERROR reading from socket");
	printf("Toilet: %s\n",mesg);
    }while(1==1);
    
	close(sockfd);
    return 0;
}
