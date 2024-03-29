/********************************************************************
 * ** Program Filename: ftserver.c
 * ** Author: Kendra Ellis
 * ** Date: 11/26/2017
 * ** Description: Implementation of a TCP connected text file
 *      transfer server.
 * ** Input: None, but meant to interact with ftclient.py.
 * ** Output: When prompted, sends either a current listing of
 *      ftserver.c's directory or transfers the .txt file specified
 *      by ftclient.py.
 * ** Sources Cited: This program relies heavily on the old style of
 *      socket programming outlined here: http://www.linuxhowtos.org/C_C++/socket.htm
 *
 *      I also consulted other sources as noted below in the function
 *      header blocks.
 * *********************************************************************/

#include <stdio.h> //input/output
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> //defn's of data types used in system calls
#include <sys/socket.h> //defn's of structures needed for sockets
#include <netinet/in.h> //constants and structs needed for internet domain addrs
#include <netdb.h>
#include <dirent.h> //for getting current directory contents
const int BUFFER_SIZE = 500;



/*********************************************************************
 * ** Function:error
 * ** Description:Uses the c function perror() to print a descriptive
 *      error message to stderr.
 * ** Source: This function comes from:
 *      http://www.linuxhowtos.org/C_C++/socket.htm
 * ** Parameters:Takes a constant char message
 * ** Pre-Conditions:None
 * ** Post-Conditions:Will exit the program.
 * *********************************************************************/
void error(const char *msg){
    perror(msg);
    exit(1);
}



/*********************************************************************
 * ** Function: createSocket()
 * ** Description: Creates a new socket and checks that the socket
 *      creation was successful.
 * ** Parameters: Takes the address of a socket file descriptor.
 * ** Pre-Conditions: There's a file descriptor created to receive
 *      the socket.
 * ** Post-Conditions: The socket is open.
 * *********************************************************************/
void createSocket(int *socketFD){
    //Open socket
    *socketFD = socket(AF_INET, SOCK_STREAM,0);

    //Check socket creation success
    if(*socketFD < 0) error("ERROR opening socket");
}



/*********************************************************************
 * ** Function:startUp()
 * ** Description:Starts the server up listening on the port specified
 *      on the command line.
 * ** Parameters: The serverPort number defined on command line, the
 *      file descriptor for the server socket, a pointer to the server's
 *      sockaddr_in struct, and the file descriptor for the socket.
 * ** Pre-Conditions: The server port number must be valid and
 *      defined, the server's socket must be instantiated.
 * ** Post-Conditions: A message will display in the console that the
 *      server is open and listening on the provided port number
 * *********************************************************************/
void startUp(int portNum, struct sockaddr_in *servAddr, int sockFD){
    //Initialize servAddr to zeros
    bzero((char*) servAddr, sizeof(*servAddr));

    //Set address family, port number(network byte order), and
    //host IP address
    servAddr->sin_family = AF_INET;
    servAddr->sin_addr.s_addr = INADDR_ANY;
    servAddr->sin_port = htons(portNum);

    //Bind the socket to the host address and port number
    if(bind(sockFD,(struct sockaddr *) servAddr, sizeof(*servAddr)) < 0)
        error("ERROR on binding");

    //Start listening for connections
    listen(sockFD,5);
    printf("Server open and listening on port %i.\n", portNum);
}



/*********************************************************************
 * ** Function: sendMsg()
 * ** Description: Sends messages to the client over the specified
 *      socket. Terminates if there's an error writing to the socket.
 * ** Parameters: A socket file descriptor (for the socket
 *      over which the message will be sent), a pointer to a
 *      const char message to be sent.
 * ** Pre-Conditions: The connection file descriptor must be associated
 *      with an open socket. The char array must be malloc'd.
 * ** Post-Conditions: The message will be sent to a connected client.
 * *********************************************************************/
void sendMsg(int socketFD, char *buffer, const char *msg){
    //Write to the socket
    bzero(buffer, BUFFER_SIZE);
    strcpy(buffer, msg);
    int success = write(socketFD, buffer, BUFFER_SIZE);
    if(success < 0)
        error("ERROR writing message to socket.");
}



/*********************************************************************
 * ** Function: recMsg()
 * ** Description: Reads messages from the socket for handling.
 *      Function does its own error checking and terminates the
 *      program if there's an error.
 * ** Parameters: A pointer to a char array, a file descriptor
 *      for the communication socket.
 * ** Pre-Conditions: There is an active socket connection and a
 *      char array has been malloc'd in main.
 * ** Post-Conditions: The message will be available for use by other
 *      functions like handleRequest. If an error occurs, the program
 *      will terminate.
 * *********************************************************************/
void recMsg(char *buffer, int socketFD){
    //Zero out the buffer
    bzero(buffer, BUFFER_SIZE);

    //Read from the socket
    int n = read(socketFD, buffer, BUFFER_SIZE);

    //Check for success
    if(n < 0) error ("ERROR reading from socket");
}



/*********************************************************************
 * ** Function: acceptClient()
 * ** Description: Starts accepting clients on the socket created.
 * ** Parameters: A pointer to a sockaddr_in struct for the client's
 *      information, the address of a file descriptor for the control
 *      connection, the server socket's file descriptor.
 * ** Pre-Conditions: The server must be open and listening.
 * ** Post-Conditions: The server will accept client connections.
 **********************************************************************/
void acceptClient(struct sockaddr_in *cliAddr, int *controlFD, int servFD){
    socklen_t clilen = sizeof(*cliAddr);

    //Start accepting client connections
    *controlFD = accept(servFD,(struct sockaddr*) cliAddr, &clilen);

    //Check for success
    if(*controlFD < 0) error("ERROR on accept");
    else printf("Connection from client.\n");
}



/*********************************************************************
 * ** Function: sendDir()
 * ** Description: Opens the server's current directory and sends
 *      each item's name across to the client.
 * ** Parameters: file descriptor for socket connection
 * ** Pre-Conditions: There must be an open connection between server
 *      and client, the server must have already received the client's
 *      command, the server must have already sent its intent to the
 *      client.
 * ** Post-Conditions: A listing of the directory's contents will be
 *      sent across to the client.
 * ** Sources Cited:
 *      https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
 *      https://en.wikibooks.org/wiki/C_Programming/dirent.h
 * *********************************************************************/
void sendDir(int socketFD, int portNum){
    //Create DIR stream pointer and dirent struct pointer
    DIR *d;
    struct dirent *dir;
    char *buffer = malloc(BUFFER_SIZE);

    //Open the directory
    d = opendir(".");

    //Check open success
    if(d){
        printf("Sending directory requested on port %i\n", portNum);
        //While there are items in the directory
        while((dir = readdir(d)) != NULL){
            //Send that item's name across to the client
            sendMsg(socketFD, buffer, strcat(dir->d_name, "\n"));
        }

        //Signal to client that sending is finished
        sendMsg(socketFD, buffer, "~done\n");
    }

    //Close the directory
    closedir(d);
    free(buffer);
}



/*********************************************************************
 * ** Function: inDir()
 * ** Description: Iterates over the server's current directory
 *      contents and returns true if the given file name is in the
 *      directory. Otherwise it returns false.
 * ** Parameters: Pointer to string that contains the file name,
 *      the port number for the connection.
 * ** Pre-Conditions: The file name must be defined.
 * ** Post-Conditions: Returns true or false depending on whether
 *      the exact file name is found.
 * ** Sources Cited:
 *      https://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
 *      https://en.wikibooks.org/wiki/C_Programming/dirent.h
 * *********************************************************************/
int inDir(char* fileName){
    //Create DIR stream pointer and dirent struct pointer
    DIR *d;
    struct dirent *dir;
    //Create variable that signals whether fileName is in directory
    int found = 0;

    //Open the directory
    d = opendir(".");

    //Check open success
    if(d){
        //While there are items in the directory
        while((dir = readdir(d)) != NULL){
            //Compare current item name to fileName
            if(strcmp(fileName, dir->d_name) == 0)
                found = 1;
        }
    }
    closedir(d);
    return found;
}


/*********************************************************************
 * ** Function: sendFile()
 * ** Description: Gets the file specified by the client and sends
 *      the file across in pieces until finished.
 * ** Parameters: A pointer to the file name, the socket file
 *      descriptor.
 * ** Pre-Conditions: There must be an open between client
 *      and server, the file name must be specified, the file
 *      must exist in the directory.
 * ** Post-Conditions: The file transfer will occur, otherwise
 *      the program will terminate with an error message.
 * *********************************************************************/
void sendFile(char *fileName, int socketFD, int portNum){
    //Create buffer for file transfer
    char *buffer = malloc(BUFFER_SIZE);
    //Create file pointer and open file to read
    FILE *file = fopen(fileName, "r");
    if(file == NULL) error("Can't open file.\n");
    printf("Sending \"%s\" requested on port %i.\n", fileName, portNum);

    //While there are characters in the file
    while(!feof(file)){
        //Read from the file
        int numBytesRead = fread(buffer, sizeof(char), 500, file);
        //Send data chunks to client
        int success = write(socketFD, buffer, numBytesRead);
        if(success < 0)
            error("ERROR writing message to socket.");
    }
    //Close file
    fclose(file);
    //Free buffer
    free(buffer);
}



/*********************************************************************
 * ** Function: handleRequest()
 * ** Description:
 * ** Parameters: pointer to buffer that's storing the command or
 *      file name, file descriptor for socket over which data will be
 *      sent.
 * ** Pre-Conditions: A connection must be established for sending
 *      messages or data between server and client, a command or
 *      file name must be loaded in the buffer for parsing.
 * ** Post-Conditions: The server will send its directory listing,
 *      the file specified, or an error msg (file not found -OR-
 *      command unknown).
 * *********************************************************************/
void handleRequest(char *buffer, int socketFD, int portNum){
    //If command is -l
    if(strncmp(buffer, "-l", 2) == 0){
        //Send current directory listing across
        printf("List directory requested on port %i.\n", portNum);
        sendMsg(socketFD, buffer, "dir\n");
        sendDir(socketFD, portNum);
        return;
    }
    //If command is !'%none', indicating that a filename
    //was entered by the client on the command-line
    if(strncmp(buffer, "\%none", 5) != 0){
        printf("File \"%s\" requested on port %i.\n", buffer, portNum);
        //Validate file name
        if(inDir(buffer) != 0){
            //Save file name
            char *fileName = malloc(BUFFER_SIZE);
            strcpy(fileName, buffer);
            //If valid, send file transfer intent
            sendMsg(socketFD, buffer, "fil\n");
            //Send file across
            sendFile(fileName, socketFD, portNum);
            free(fileName);
        }
        //Else send error message: file not found
        else {
            printf("File not found. Sending error message to client: %i.\n", portNum);
            sendMsg(socketFD, buffer, "nof\n");
        }
        return;
    }
    //Else send error message: command unknown
    else
        sendMsg(socketFD, buffer, "unk\n");
    return;
}


/*MAIN*/
int main(int argc, char *argv[]){
    //Variable, file descriptors, and Struct definitions
    int listenSockFD, connectSockFD, dataSockFD;
    int portNum;
    struct sockaddr_in *servAddr = malloc(sizeof(struct sockaddr_in));
    struct sockaddr_in *cliAddr = malloc(sizeof(struct sockaddr_in)); //From <netinet/in.h>
    char *buffer = malloc(BUFFER_SIZE); //For storing characters exchanged in socket connection

    //command-line parameter validation
    if(argc < 2){
        fprintf(stderr, "ERROR, no port provided.\n");
        printf("usage: ./executableName portNum.\n\tportNum: must be in range 4,000-65,000.\n");
        exit(1);
    }
    //Check valid port number
    if(atoi(argv[1]) < 4000 || atoi(argv[1]) > 65000){
        fprintf(stderr, "ERROR, invalid port number.\n");
        printf("usage: ./executableName portNum.\n\tportNum: must be in range 4,000-65,000.\n");
        exit(1);
    }

    //Convert specified portNum to int
    portNum = atoi(argv[1]);

    //Create a new socket
    createSocket(&listenSockFD);

    //Start up server to listen
    startUp(portNum, servAddr, listenSockFD);

    //Until SIGINT is received, accept connections
    while(1){
        //Accept client connection
        acceptClient(cliAddr, &connectSockFD, listenSockFD);

        //Get command and other info from the client
        //on the control connection
        recMsg(buffer, connectSockFD);

        //Get data port for data connection
        char *dataPortStr = malloc(BUFFER_SIZE);
        recMsg(dataPortStr, connectSockFD);
        int dataPort = atoi(dataPortStr);

        //Establish data connection
        sleep(1);
        createSocket(&dataSockFD);

        cliAddr->sin_port = htons(dataPort);
        if(connect(dataSockFD, (struct sockaddr*) cliAddr, sizeof(*cliAddr)) < 0)
            error("ERROR establishing data connection.\n");

        //Handle request on data connection
        handleRequest(buffer, dataSockFD, dataPort);

        //Close data connection socket
        printf("Closing data connection.\n");
        printf("\n\n");
        close(dataSockFD);
        //Free dataPort Str
        free(dataPortStr);
    }

    //Close control socket and free memory
    close(listenSockFD);
    free(servAddr);
    free(buffer);
    return 0;
}
