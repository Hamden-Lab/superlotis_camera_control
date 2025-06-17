#include <stdio.h>                // Standard I/O functions
#include <stdlib.h>               // Standard library functions (malloc, exit, etc.)
#include <string.h>               // String manipulation (strcpy, strcmp, etc.)
#include <unistd.h>               // Unix standard functions (close, read, write, etc.)
#include <sys/types.h>            // Data types used in system calls
#include <sys/socket.h>           // Main sockets header
#include <netinet/in.h>           // Internet address family (sockaddr_in)
#include <ctype.h>                // Character utilities (isdigit, etc.)
#include <fstream>                // C++ file streams (not used here)
#include <vector>                 // C++ vectors (not used here)


// #include "picam.h"             // Unused, camera library

#include "camera.h"               // Project: camera control header
#include "server.h"               // Project: server definitions
#include "socketid.h"             // Project: camera port and socket constants
#include "fitsio.h"               // CFITSIO for FITS file operations
#include "fitsio2.h"              // Additional CFITSIO definitions

#define OK 0                      // Success return value
#define ERR -1                    // Error return value

// String to print when user asks for help
const char helpstr[]=" \
\nCommands:\
\ngain [piint]\
\ntemp [piflt]\
\nshutter_mode [piint]\
\nexptime [piflt]\
\nexpose\
\ndark\
\nbias\
\nstatus\
\nburst\
\ncommit_params\
\ngenfits\n";

// status 
///exposing, reading out, idle

// Print error message from system and return
void error(const char *msg)
{
    perror(msg);                  // Print error message using perror
}

// Variables needed for sockets
int sockfd, newsockfd, portno;    // File descriptors and port number
socklen_t clilen;                 // Length of client address struct
char buffer[256], response[2048]; // Buffers for request and response
int resplen, n;                   // Length of response, bytes read/written
int port_open = 0;                // Whether the port is open (boolean)
struct sockaddr_in serv_addr, cli_addr; // Server and client address structs
int argct, retval;                // Argument count, return value

// Open a TCP server socket for camera control
int open_server(){
  sockfd = socket(AF_INET, SOCK_STREAM, 0);             // Create a TCP socket
  if (sockfd < 0) {                                     // Check if socket creation failed
    error("ERROR opening socket");                      // Print error message
    return ERR;                                         // Return error code
  };

  struct linger lo = { 1, 0 };                          // Linger option: close immediately
  setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &lo, sizeof(lo)); // Set socket option
  bzero((char *) &serv_addr, sizeof(serv_addr));         // Zero out server address struct

  portno = CAM_PORT;                                     // Set port number from header
  serv_addr.sin_family = AF_INET;                        // IPv4
  serv_addr.sin_addr.s_addr = INADDR_ANY;                // Accept any incoming IP address
  serv_addr.sin_port = htons(portno);                    // Convert port to network byte order
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
	   sizeof(serv_addr)) < 0){                       // Bind socket to address
    error("ERROR on binding");                          // Print error if binding failed
    return ERR;                                         // Return error code
  };
  
  listen(sockfd,5);                                      // Listen for incoming connections
  clilen = sizeof(cli_addr);                             // Set client address length

  port_open = 1;                                         // Mark port as open
  printf("Server opened.\n");                            // Print confirmation
  return OK;                                             // Return success
};

// Close the server socket if open
int close_server(){
  if(port_open){                                         // If port is open
    close(sockfd);                                       // Close socket file descriptor
    printf("Server closed\n");                           // Print confirmation
    port_open = 0;                                       // Mark as closed
  }; // port_open
  return OK;                                             // Return success
};

// Listen for a command on the server socket and process it
int listen_server(){
  char *cmd;                // Pointer to command string
  char *arg;                // Pointer to argument string
  int argc;                 // Argument count
  int res,val;              // Result and value variables
  char *res_char;           // Unused pointer
  piflt fval;               // Float value for arguments
  if(port_open){                                               // Only proceed if port is open
    newsockfd = accept(sockfd, 
		       (struct sockaddr *) &cli_addr, 
		       &clilen);                                // Accept incoming connection
    if (newsockfd < 0) {                                      // Check for accept error
      error("ERROR on accept");                               // Print error message
      return OK;                                              // Return OK but do nothing
    };
    bzero(buffer,256);                                        // Zero out buffer for request
    n = read(newsockfd,buffer,255);                           // Read command from client
    if (n < 0){                                               // If read failed
      error("ERROR reading from socket");                     // Print error
      return OK;                                              // Return OK but do nothing
    };

    printf("Received command: %s.\n",buffer);                 // Print received command
    printf("%d\n",strcmp(buffer,"exit"));                     // Print result of exit check

    // *********************************************
    // Process commands
    // set up the defaults;
    argc = 0;                                                 // Default: no arguments
    retval = 1;                                               // Default: keep server running
    resplen=sprintf(response,"Invalid command.");              // Default response
    buffer[strcspn(buffer, "\n")] = '\0';                     // Remove newline from buffer

    if(strchr(buffer, '=')){                                  // If argument provided as cmd=val
      cmd = strtok(buffer,"=");                               // Split command at '='
      printf("Command 1 %s\n",cmd);                           // Print command
      if (cmd != NULL){                                       // If command exists
        arg = strtok(NULL,"=");                               // Extract argument string
        if(arg != NULL){                                      // If argument exists
          argc = 1;                                           // Set argument count to 1
          printf("Argument 1 %s\n",arg);                      // Print argument
        } else {
          argc = 0;                                           // No argument found
        };
      };
    } else {                                                  // No '=' found, just command
      cmd = buffer;                                           // Set command to buffer
      printf("Command 2 %s\n",cmd);                           // Print command
      argc = 0;                                               // No argument
    };

    // Set/get exposure time
    if (strcmp(cmd, "exptime") == 0) {
      if(argc == 1){                                          // If argument provided
        printf(arg);                                          // Print argument string

        char ints[] = "0123456789";                           // Valid digits string
        const char *nonDigits = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()-_=+[]{}|;:'\",.<>?/\\ \t\n"; // Invalid characters
        int value = atoi(arg);                                // Convert argument to integer

        if (value >= 0 && value <= 240000 && strpbrk(arg, ints)!=NULL && strpbrk(arg, nonDigits)==NULL){
          fval = atof(arg);                                   // Convert argument to float

          printf("[DEBUG] server.c Parsed fval from arg: %f\n", fval); // Print debug value

          res = set_exposure_time(fval);                      // Set exposure time
          printf("[DEBUG] server.c Result of set_exposure_time: %d\n", res);

          if (res) {
              resplen = sprintf(response, "Error setting exposure time."); // Error response
          } else {
              resplen = sprintf(response, "%0.2f", fval);                 // Success response
          }
          printf("[DEBUG] server.c Response: %s\n", response);
          printf("[DEBUG] server.c resplen: %d\n", resplen);
        // } else {
        //     printf("err");
        //     return ERR;
        }
      };
      if(argc == 0){                                         // No argument: get exposure time
        printf("Number of arguments: %d\n", argc);
        res = get_exposure_time(&fval);                      // Get exposure time from camera
        printf("[DEBUG] Result of get_exposure_time (res): %d\n", res);
        printf("[DEBUG] Result of get_exposure_time (fval): %d\n", fval);
        printf("[DEBUG] Result of get_exposure_time (&fval): %d\n", &fval);
        printf("[DEBUG] Result of get_exposure_time (response): %d\n", response);

        if (res) {
            resplen = sprintf(response, "Error getting exposure time."); // Error response
        } else {
            resplen = sprintf(response, "%0.2f", fval);                 // Success response
        };
      };

      };
  
      //for bias: if favl != 0 -> return ERR
      //add DEBUG preprocesser directives

      // exit command received
      if( strcmp(cmd,"exit")==0){
        retval = 0;                                           // Set return value to exit
        resplen = sprintf(response,"Exit requested.");         // Response string
      };

      // help
      if( strcmp(cmd,"help")==0){
        resplen = sprintf(response,"%s",helpstr);              // Response contains help string
      };

      // set/get analog gain
      if (strcmp(cmd,"analog_gain")==0){
        if(argc == 1){                                        // If argument provided
          char ints[] = "0123";                               // Valid gain values
          const char *nonDigits = "456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()-_=+[]{}|;:'\",.<>?/\\ \t\n"; 
          int value = atoi(arg);                              // Parse argument to int
          if (value >= 0 && value <= 3 && strpbrk(arg, ints)!=NULL && strpbrk(arg, nonDigits)==NULL){
            fval = atof(arg);                                 // Convert argument to float (should be int in reality)
            res = set_analog_gain(fval);                      // Set analog gain parameter
            if(res){
              resplen = sprintf(response,"Error setting analog gain."); // Error response
            } else { 
              resplen = sprintf(response,"%0.2f",val);        // Success response
            }
          }
        }
        if(argc == 0){                                        // No arg: get analog gain
            res = get_analog_gain(&val);                      // Get analog gain value
            if (res){
              resplen = sprintf(response,"Error getting analog gain."); // Error response
            } else {
              resplen = sprintf(response,"%0.2f",val);        // Success response
            };
        }; //argument list
      
      };
      // Set/get temperature
      if (strcmp(cmd, "temp") == 0) {
        if(argc == 1){
          printf(arg);

          char ints[] = "0123456789";
          const char *nonDigits = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_=+[]{}|;:'\",.<>?/\\ \t\n"; 
          int value = atoi(arg);

          if (value >= -80 && value <= 90 && strpbrk(arg, ints)!=NULL && strpbrk(arg, nonDigits)==NULL){
            fval = atof(arg);                                // Convert to float

            printf("[DEBUG] Parsed fval from arg: %f\n", fval);

            res = set_temp(fval);                            // Set camera temperature
            printf("[DEBUG] Result of set_temp: %d\n", res);

            if (res) {
                resplen = sprintf(response, "Error setting temp.");
            } else {
                resplen = sprintf(response, "%0.2f", fval);  // Return set value
            }
            printf("[DEBUG] Response: %s\n", response);
            printf("[DEBUG] resplen: %d\n", resplen);
          // } else {
          //     printf("err");
          //     return ERR;
          }
        };
        if(argc == 0){                                       // No arg: get temperature
          printf("Number of arguments: %d\n", argc);
          res = get_temp(&fval);                             // Get camera temperature
          printf("[DEBUG] Result of get_temp: %d\n", res);
          printf("[DEBUG] Result of get_temp pointer: %d\n", fval);

          if (res) {
              resplen = sprintf(response, "Error getting temp.");
          } else {
              resplen = sprintf(response, "%0.2f", fval);
          };
        };

        };

      // burst
      if (strcmp(cmd, "burst") == 0) {
          if (argc == 1) {  
              fval = atof(arg);                                 // Convert argument to float
              res = burst((int)fval);                           // Call burst with number of exposures
              if (res == 0) {
                  resplen = sprintf(response, "Burst exposure of %d images complete.", (int)fval); // Success response
              } else {
                  resplen = sprintf(response, "Burst exposure failed."); // Error response
              };
          } else {
              resplen = sprintf(response, "Usage: burst <number_of_exposures>"); // Usage info
          };
      };

      // Take a single exposure
      if (strcmp(cmd,"expose")==0){
        res = expose("exposure_file");                         // Call expose
        if (res){
          resplen = sprintf(response,"Exposure error.");        // Error response
        } else {
          resplen = sprintf(response,"Exposure complete.");     // Success
        };
      };

      // Take a dark frame
      if (strcmp(cmd,"dark")==0){
        res = dark("dark_file");                               // Call dark
        if (res){
          resplen = sprintf(response,"Exposure error.");        // Error
        } else {
          resplen = sprintf(response,"Exposure complete.");     // Success
        };
      };

      // Take a bias frame
      if (strcmp(cmd,"bias")==0){
        res = bias("bias_file");                               // Call bias
        if (res){
          resplen = sprintf(response,"Exposure error.");        // Error
        } else {
          resplen = sprintf(response,"Exposure complete.");     // Success
        };
      };

      // TODO: Add more commands here (e.g., genfits, status, etc.)

    // *********************************************
    
    n = write(newsockfd,response,resplen);                     // Write response to client
    if (n < 0) error("ERROR writing to socket");               // Error check
    close(newsockfd);                                          // Close client connection
    return retval;                                             // Return (1 for continue, 0 for exit)
  } else { 
    return OK;                                                 // If port not open, return OK
  };
  return 1;                                                    // Should not reach here
};
