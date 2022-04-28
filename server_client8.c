// STUDENT: Matthew Teets
// CLASS: CSCI 3800
// ASSIGNMENT: Lab8

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <math.h>
#define STDIN 0
#define VERSION 4

// Structure that holds info of config.txt file
struct _configure {
    int port_num;
    char ip[14];
    int rows;
    int columns;
  // DMO - you need something here to keep track of things sent/received
  int  seqNumberSent; // For lab 7
  int  ackNumberSent;
  int  seqNumberReceived; // For lab 7
  int  ackNumberReceived;
};

// Structure that holds info pertaining to the message being sent
struct _msg{
    int version;
    int location;
    int originPort;
    int destPort;
    int hopCount;
    char command[20];
    char msg[100];
    int msg_id;
    int route[3];
    char sendPath[50];
};

/* Function Declarations */
int findCoordinates(int choice, int *row, int *column, int ROWS, int COLUMNS); // Generates coordinates for a given location within a given grid
int distance(int c1, int c2, int r1, int r2); // Uses the euclidean distance formula to calculate the distance between two points
int sendData(char *buffer, int sd, struct sockaddr_in server_address); // This function allows the client to send data to the server
void parseMe(char *line, struct _msg *message); // Parses through the sent message and tokenizes the colon delimited values.

// ==================================================================== //
// =============================== MAIN =============================== //
// ==================================================================== //

int main(int argc, char * argv[]) {
    
    struct _configure s1[100];  // Structure variable for the config file servers
    struct _msg message;        // Structure variable for the client message
    
    // Opens config file
    FILE *f = fopen("config.txt", "r");
    char line[250];
    
    // Error checking if file exists
    if (f == NULL)
    {
        printf("Error: No file \n");
    }
    
    // Reads config file into struct members for each server
    int MAXPARTNERS = 0;                    // Variable for the number of servers being connected
    int configGrid = 0;                     // Variable to check if the first line of config file has been read
    while (fgets(line, sizeof(line), f))    // returns NULL when there is no more data
    {
        if (configGrid < 1) {
            sscanf(line, "%d %d", &s1[MAXPARTNERS].rows, &s1[MAXPARTNERS].columns);
            configGrid++;
        }
        sscanf(line, "%s %d", s1[MAXPARTNERS].ip, &s1[MAXPARTNERS].port_num);
        MAXPARTNERS++;
    }
    
    /* Print config struct variables */
//    printf("ROWS and COLS : %d %d\n", s1[0].rows, s1[0].columns);
//    printf("1 : %s %d\n", s1[0].ip, s1[0].port_num); // Potential problem - index 0 prints the rows and columns (10 3)
//    printf("2 : %s %d\n", s1[1].ip, s1[1].port_num);
//    printf("3 : %s %d\n", s1[2].ip, s1[2].port_num);
//    printf("4 : %s %d\n", s1[3].ip, s1[3].port_num);
    
    fclose(f); // Closes file when we have what we need
    
    /* Variables used throughout the program */
    char buffer[100];       // Variable for the message entered by the user
    char bufferSend[100];   // Variable for the message being sent to the server(s)
    char bufferRecv[100];   // Variable for the message being received from the client(s)
    char bufferACK[100];    // Variable for the ACK message that is sent to the original sender
    char serverIP[29];      // Variable for the IP address of server to be stored
    char *ptr;              // Variable for pointer ptr
    char *ptr1;
    int rc;                 // Varaible for the return code
    int flags = 0;          // Variable recv function
    int messagVersion;      // Variable for version comparison
    char messageCommand [10];
    char sendPort[5];
    
    /* Server/Client information variables */
    struct sockaddr_in from_address;                // Structure variable for the from address of the message
    socklen_t fromLength = sizeof(struct sockaddr); // Sets variable to take the length of socket related parameters
    fromLength = sizeof(struct sockaddr_in);        // Size variable for the length of the from address
    fd_set socketFDS;                               // Set of socket discriptors
    int maxSD;                                      // Tells the OS how many sockets are set
    
    /* Checks if number of cmd line arguments is correct */
    if (argc < 3) {
        printf("client4 <portnumber> <location>\n");
        exit(1);
    }
    
    int sd;                             // Socket descriptor variable
    struct sockaddr_in server_address;  // Provides address info for current server
    struct sockaddr_in partner_address; // Provides address into for other servers
    
    /* Bind requirements */
    int portNumber = atoi(argv[1]);
    fromLength = sizeof(struct sockaddr_in);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portNumber);
    server_address.sin_addr.s_addr = INADDR_ANY;
    
    /* Bind actions */
    sd = socket(AF_INET, SOCK_DGRAM, 0);                                        // Creates the socket
    rc = bind(sd, (struct sockaddr *)&server_address, sizeof(server_address));  // Binds the socket to the local address info
    
    /* Checks if the bind was successful */
    if (rc < 0) {
        perror("bind");
        exit(1);
    }
    
    printf("-------------------------------\n");
    
    /* Message Variables */
    int location = atoi(argv[2]);               // Variable stores the users location
    printf("My location: %d\n", location);
    int ROWS = s1[0].rows;                          // Variable that holds the number of rows
    int COLUMNS = s1[0].columns;                    // Variable that holds the number of columns
//    printf("ROWS = %d\nCOLUMNS = %d\n", ROWS, COLUMNS);
    int originPort = atoi(argv[1]);                 // Variable for the origin port number
    int destPort;                                   // Variable for the destination port number
    int hopCount = 4;                               // Variable for the hop counter
    int msg_id = 1;                                 // Variable for the message ID
    char sendPath[50];                              // Variable for the message route
    int myPortNumber;
    
    
    // Variables used to obtain/store coordinates of location in the grid
    int choice_r;   // = x coord
    int choice_c;   // = y coord

    /* ================================= INFINITE LOOP ================================= */
    for(;;)
    {
        fflush(stdin); // Used to clear the output buffer
        printf("Enter port number and message plz: \n"); // Prompts user for port and message
        
        // Sets all buffer characters to '\0'
        memset(buffer, '\0', 100);
        memset(bufferSend, '\0', 100);
        memset(bufferRecv, '\0', 100);
        memset(bufferACK, '\0', 100);
        
        FD_ZERO(&socketFDS);        // Initializes set to contain no file descriptors
        FD_SET(sd, &socketFDS);     // Sets the value for sd in the set
        FD_SET(STDIN, &socketFDS);  // Sets the value for STDIN in the set
        
        // Checks descriptor sizes
        if (STDIN > sd)
        {
            maxSD = STDIN;
        }else{
            maxSD = sd;
        }
        
        rc = select (maxSD + 1, &socketFDS, NULL, NULL, NULL); // Sets which file descriptor is ready to be read
        memset (message.sendPath, 0, 50); // DMO // Zeros out the sendPath variable
        
        /* Received information from the user */
        if (FD_ISSET(STDIN, &socketFDS))
        {
            myPortNumber = originPort;
            /* Set the sendPath to portNumber */
            sprintf (message.sendPath, "%d", portNumber); //DMO ?
            
            memset(buffer, '\0', 100);
            scanf("%d", &destPort);                     /* DMO get the port number */
            ptr = fgets(buffer, sizeof(buffer), stdin); /* DMO get the msg */
            ptr = ptr;
            // Creates protocol message in the discussed format
            buffer[strlen(buffer)-1] = '\0';
            sprintf(bufferSend, "%d:INFO:%d:%d:%d:%d:%d:%s:%s",
                    VERSION, location, originPort, destPort, hopCount, msg_id, message.sendPath, buffer);

            printf("Sending: '%s'\n\n", buffer);
            
            /* Sends out the users message to all ports */
            int temp;
            for (int i = 0; i < MAXPARTNERS; i++)
            {
                /* Skips the 0th element */
                if (i == 0){
                    continue;
                }
                
                temp = s1[i].port_num;
                if (temp == portNumber) {
                    continue;
                }

                strcpy(serverIP, s1[i].ip);
                partner_address.sin_family = AF_INET;                   // Sets the address family for the transport address
                partner_address.sin_port = htons(s1[i].port_num);       // Indexes to the 'port_num's of the struct array
                partner_address.sin_addr.s_addr = inet_addr(serverIP);  // Indexes to the 'ip's of the struct array
            
                sendData(bufferSend, sd, partner_address);              // Calls the 'sendData' funtion
            }
        }
    
        /* Received information from the server */
        if (FD_ISSET(sd, &socketFDS))
        {
            rc = recvfrom(sd, bufferRecv, sizeof(bufferRecv), flags, (struct sockaddr *)&from_address, &fromLength);
            parseMe(bufferRecv, &message); // Calls the 'parseMe' function
            
            /* ================================= LOCATION STUFF ================================= */
            
            /* Find the coordinates of the user location */
            int x1, y1;
            int ret_1 = findCoordinates(location, &choice_r, &choice_c, ROWS, COLUMNS);
            if (-1 == ret_1) // If true then user location is NOT IN GRID
            {
                goto end_msg; // Jumps to end of message
            } else {
                // Stores user coords inside (x1, y1)
                x1 = choice_r;
                y1 = choice_c;
            }
            
            /* Find the coordinates of the message location */
            int x2, y2;
            int ret_2 = findCoordinates(message.location, &choice_r, &choice_c, ROWS, COLUMNS);
            if (-1 == ret_2) // If true then message location is NOT IN GRID
            {
                goto end_msg; // Jumps to end of message
            } else {
                // Stores message coords inside (x2, y2)
                x2 = choice_r;
                y2 = choice_c;
            }
            
            /* Calculates and displays the distance between the user location and message location */
            int distanceVal = distance(x1, x2, y1, y2);
            
            /* ================================= MESSAGE FORWARDING STUFF ================================= */
        
            if (distanceVal > 2) // If the distance between the two coords is greater than 2 OUT OF RANGE
            {
                // MOV command skips distance check //
                if (strcmp(message.command, "MOV") == 0) {
                    goto ignore_distance;
                }
                
                printf("OUT OF RANGE\n\n");
                goto end_msg; // Avoids printing the data message and goes to the end of the transmission
            }
            
            if(distanceVal <= 2) // If is IN RANGE
            {
                // MOV command jumps here //
            ignore_distance:
                
                /* Error checking for VERSION */
                sscanf(bufferRecv, "%d", &messagVersion);
                if (messagVersion != VERSION) {
                    printf("\nERROR: Incompatible VERSION!\n");
                    continue;
                }
                
                if(message.destPort == originPort) {
                    
                    /* Prints and formats all data recieved */
                    printf("\n************************************\n"
                           "%s MESSAGE RECEIVED :              "
                           "\n************************************\n"
                           "version = %d \t\tlocation = %d     "
                           "\noriginPort = %d \tdestPort = %d  "
                           "\ncommand = %s  \t\tmsg_ID = %d    "
                           "\nMessagePath = (%s)               "
                           "\nhopCount = %d \t\tmessage = %s   "
                           "\n************************************\n\n",
                           message.command, message.version, message.location, message.originPort,
                           message.destPort, message.command, message.msg_id, message.sendPath, message.hopCount, message.msg);
                    
                    if (!strcmp(message.command, "ACK")){
                        continue;
                    }
                    
                    /* Checks for MOV command and continues after MOV command is executed */
                    if (strcmp(message.command, "MOV") == 0) {
                        printf("Move to location : %s\n", message.msg);
                        location = atoi(message.msg);
                        printf("(%d moved to %d)\n", originPort, location);
                        continue;
                    }
                    
                /* =============== ACK message stuff =============== */
                    
                    memset(bufferACK, '\0', 100);
                    printf("Sending ACK to sender...\n");
                    printf ("DMO MY PORTNUMBER IS  %d\n", originPort);
                    sprintf (message.sendPath, "%d", originPort); // DMO
                    /* Creates ACK message */
                    sprintf(bufferACK, "%d:ACK:%d:%d:%d:%d:%d:%s:%s",
                            VERSION, location, message.destPort, message.originPort, message.hopCount, message.msg_id,
                            message.sendPath, message.msg);
                    printf ("DMO sending ACK-> '%s'\n", bufferACK);
                    
                    int alreadySentACK = 0; /* Variable to check if message has already been sent */
                    /* Sends an ACK message back to the origin port when message is received */
                    for (int i = 0; i < MAXPARTNERS; i++)
                    {
                        /* Skips the 0th element */
                        if (i == 0) {
                            continue;
                        }
                        
                        char sendPort [6];
                        char * ptr1;
                        int send2Port;
                        send2Port = s1[i].port_num;
                        sprintf (sendPort, "%d", send2Port);
                        ptr1 = strstr(message.sendPath, sendPort);
                        
                        if (alreadySentACK < 1) {
                            alreadySentACK++; /* Increment the value (nullifying the send function for the ACK)*/
                            if ( ptr1 == NULL && send2Port != myPortNumber) {
                                strcpy(serverIP, s1[i].ip);
                                partner_address.sin_family = AF_INET;                   // Sets the address family for the transport address
                                partner_address.sin_port = htons(s1[i].port_num);       // Indexes to the 'port_num's of the struct array
                                partner_address.sin_addr.s_addr = inet_addr(serverIP);  // Indexes to the 'ip's of the struct array
                                sendData(bufferACK, sd, partner_address);               // Calls the 'sendData' funtion
                            }
                        }
                        continue;
                    }
                    
                /* ================== Forwarding Stuff ================== */
                
                } else if (message.originPort != originPort) {
                    message.hopCount--;
                    message.location = location;
                    
                    /* Update and append new port number to the message path */
//                    printf("\nSendP before: %s\n", message.sendPath);
                    printf("originPort: %d\n", originPort);
//                    printf("sendPort: %s\n", sendPort);
//                    printf("myPort: %d\n", myPortNumber);
//                    printf("message.op: %d\n", message.originPort);
                    sprintf (message.sendPath, "%s,%d", message.sendPath, originPort);
                    printf("SendPath = %s\n", message.sendPath);
                    
                    
                    if (message.hopCount > 0) {
                        
                        printf("\nForwarding SENDPATH : %s\n", message.sendPath);
                        /* Loop and send to each server */
                        sprintf(bufferSend, "%d:%s:%d:%d:%d:%d:%d:%s:%s",
                                VERSION, message.command, location, message.destPort, message.originPort, message.hopCount, message.msg_id,
                                message.sendPath, message.msg);
                        
                        printf("************************************\n");
                        printf("Forwarding message to next gnode --->\n");
                        printf("Sending: '%s'", bufferSend);
                        printf("\nHop Count: %d", message.hopCount);
                        printf ("\nDestination Port %d", message.destPort);
                        printf ("\nOrigin Port %d", message.originPort);
                        printf("\n************************************\n\n");
                        sprintf(bufferSend, "%d:%s:%d:%d:%d:%d:%d:%s:%s",
                                VERSION, message.command, location, message.originPort, message.destPort, message.hopCount, message.msg_id,
                                message.sendPath, message.msg);

                        int alreadySentMSG = 0;
                        for (int i = 0; i < MAXPARTNERS; i++)
                        {
                            /* Skips the 0th element */
                            if (i == 0) {
                                continue;
                            }
                            
                            portNumber = s1[i].port_num;
                            strcpy(serverIP, s1[i].ip);
                            partner_address.sin_family = AF_INET;
                            partner_address.sin_port = htons(portNumber);
                            partner_address.sin_addr.s_addr = inet_addr(serverIP);
                            
                            sprintf(sendPort, "%d", s1[i].port_num);
                            ptr1 = strstr(message.sendPath, sendPort);
                            
                            if (alreadySentMSG < 1) {
                                alreadySentMSG++;
                                if (ptr1 == NULL){              // if (ptr1 == NULL && portNumber != message.originPort) {
                                    if (alreadySentMSG >= 2) {
                                        continue;
                                        // goto end_msg;
                                    }
                                    sendData(bufferSend, sd, partner_address);
                                    // continue;
                                    // goto end_msg;
                                } else {
                                    printf("Not sending to %d\n", portNumber);
                                    // continue;
                                    // goto end_msg;
                                    
                                }
                            }
                        }
                        
                    } else {
                        printf("************************************\n");
                        printf("Hop Count: %d\n", message.hopCount);
                        printf("Ran out of hops!\n");
                        printf("************************************\n\n");
                    }
                }
            }
            
        // goto label to jump to end of the message
        end_msg:
             printf("\n");
        }
    }
    close(sd); // Closes the client socket
    return 0;
}

// ===============================================================================//
// ================================== FUNCTIONS ==================================//
// ===============================================================================//

// This function allows the client to send data to the server
int sendData(char *buffer, int sd, struct sockaddr_in server_address) {
    /*
       All information being sent out to the server :
            sd                                   ->  socket descriptor
            buffer                               ->  data being sent
            strlen(buffer)                       ->  how many bytes of data being sent
            0                                    ->  flags
            (struct sockaddr *) &server_address  ->  TO: address of the server
            sizeof(server_address)               ->  size of the data structure being sent
     */
    
    int rc = 0;
    rc = sendto(sd, buffer, strlen(buffer), 0, (struct sockaddr *) &server_address, sizeof(server_address));
    // printf ("DMO: sending '%s' to socket %d, address %s\n", buffer, sd, inet_ntoa(server_address.sin_addr));
    // Checks if sendto was successfully filled
    if(rc <= 0)
    {
        printf("ERROR: No bytes were sent/received... \n\n");
        exit(1);
    }
    return(0);
}

// =========================================================================//

// Parses through the sent message and tokenizes the colon delimited values.
void parseMe(char *line, struct _msg *message){
    
    int version;
    char command[20];
    int location;
    int originPort;
    int destPort;
    int hopCount;
    char *ptr;
    char msg[100];
    int msg_id;
    char sendPath[50];

    version = atoi(strtok(line, ":"));
    
    ptr = strtok(NULL, ":");
    sprintf(command, "%s", ptr);
    
    /* Checks if the command TYPE is a MOV */
    if (strcmp(command, "MOV") == 0) {
        // printf("MOV command was send...");
    }
    
    ptr = strtok(NULL, ":");
    location = atoi(ptr);
    
    ptr = strtok(NULL, ":");
    originPort = atoi(ptr);
    
    ptr = strtok(NULL, ":");
    destPort = atoi(ptr);
    
    ptr = strtok(NULL, ":");
    hopCount = atoi(ptr);
    
    ptr = strtok(NULL, ":");
    msg_id = atoi(ptr);
    
    ptr = strtok(NULL, ":");
    sprintf(sendPath, "%s", ptr);
    
    ptr = strtok(NULL, ":");
    sprintf(msg, "%s", ptr);
    
    /* Push all info into the message struct */
    message -> version = version;
    message -> location = location;
    message -> originPort = originPort;
    message -> destPort = destPort;
    message -> hopCount = hopCount;
    message -> msg_id = msg_id;
    sprintf(message -> sendPath, "%s", sendPath);
    sprintf(message -> command, "%s", command);
    sprintf(message -> msg, "%s", msg);
}

//=========================================================================//

// Generates coordinates for a given location within a given grid
int findCoordinates(int choice, int *row, int *column, int ROWS, int COLUMNS) {
  /*
     - ROWS and COLUMNS are the size of the grid
     - row and column are returned as location of the ‘choice’
     - choice is the cell you are in
   */
    
    *row = (choice - 1) / COLUMNS + 1;
    *column = (choice - 1) % COLUMNS + 1;
    
    if (*row > ROWS) {
        printf ("\nNOT IN GRID\n\n");
        return(-1);
    }else{
        //printf ("Your row/column: ( %d / %d ) \n",*row, *column);
    }

    return(1);
    
}

//=========================================================================//

// Uses the euclidean distance formula to calculate the distance between two points
int distance(int c1, int c2, int r1, int r2) {
    return (int)sqrt(pow(c2 - c1, 2) + pow(r2 - r1, 2));
}

//=========================================================================//


    


