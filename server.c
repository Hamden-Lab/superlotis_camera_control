#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>  // Include ctype.h for isdigit()
#include <fstream>
#include <vector>
// #include <algorithm>
// #include <cstring>
// #include <cstdint>
// #include <iomanip>

// #include "picam.h"

#include "camera.h"
#include "server.h"
#include "socketid.h"
#include "fitsio.h"
#include "fitsio2.h"

#define OK 0
#define ERR -1

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

void error(const char *msg)
{
    perror(msg);
}

// variables needed for sockets
int sockfd, newsockfd, portno;
socklen_t clilen;
char buffer[256], response[2048];
int resplen, n;
int port_open = 0;
struct sockaddr_in serv_addr, cli_addr;
int argct, retval;


int open_server(){
  // make a socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR opening socket");
    return ERR;
  };

  // set options and clear values
  struct linger lo = { 1, 0 };
  setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &lo, sizeof(lo));
  bzero((char *) &serv_addr, sizeof(serv_addr));

  portno = CAM_PORT;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  if (bind(sockfd, (struct sockaddr *) &serv_addr,
	   sizeof(serv_addr)) < 0){
    error("ERROR on binding");
    return ERR;
  };
  
  // set up listening.
  listen(sockfd,5);
  clilen = sizeof(cli_addr);

  port_open = 1;
  printf("Server opened.\n");
  return OK;
};

int close_server(){
  if(port_open){
    close(sockfd);
    printf("Server closed\n");
    port_open = 0;
  }; // port_open
  return OK;
};

int listen_server(){
  char *cmd;
  char *arg;
  int argc;
  int res,val;
  char *res_char;
  piflt fval;
  if(port_open){
    newsockfd = accept(sockfd, 
		       (struct sockaddr *) &cli_addr, 
		       &clilen);
    if (newsockfd < 0) {
      error("ERROR on accept");
      return OK;
    };
    bzero(buffer,256);
    n = read(newsockfd,buffer,255);
    if (n < 0){
      error("ERROR reading from socket");
      return OK;
    };

    printf("Received command: %s.\n",buffer);
    printf("%d\n",strcmp(buffer,"exit"));

    // *********************************************
    // OK. Here is where we process commands
    // set up the defaults;
    argc = 0;
    retval = 1;
    resplen=sprintf(response,"Invalid command.");
    buffer[strcspn(buffer, "\n")] = '\0';  

    if(strchr(buffer, '=')){
      cmd = strtok(buffer,"=");
      printf("Command 1 %s\n",cmd);
      if (cmd != NULL){
        arg = strtok(NULL,"=");
        if(arg != NULL){
          argc = 1;
          printf("Argument 1 %s\n",arg);
        } else {
          argc = 0;
        };
      };
    } else {
      cmd = buffer;
      printf("Command 2 %s\n",cmd);
      argc = 0;
    };

    if (strcmp(cmd, "exptime") == 0) {
      if(argc == 1){
        printf(arg);

        char ints[] = "0123456789";
        const char *nonDigits = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()-_=+[]{}|;:'\",.<>?/\\ \t\n"; 
        int value = atoi(arg);

        if (value >= 0 && value <= 240000 && strpbrk(arg, ints)!=NULL && strpbrk(arg, nonDigits)==NULL){
          fval = atof(arg); //convert to string

          printf("[DEBUG] server.c Parsed fval from arg: %f\n", fval);

          res = set_exposure_time(fval);
          printf("[DEBUG] server.c Result of set_exposure_time: %d\n", res);

          if (res) {
              resplen = sprintf(response, "Error setting exposure time.");
          } else {
              resplen = sprintf(response, "%0.2f", fval);
          }
          printf("[DEBUG] server.c Response: %s\n", response);
          printf("[DEBUG] server.c resplen: %d\n", resplen);
        // } else {
        //     printf("err");
        //     return ERR;
        }
      };
      if(argc == 0){
        printf("Number of arguments: %d\n", argc);
        res = get_exposure_time(&fval);
        printf("[DEBUG] Result of get_exposure_time (res): %d\n", res);
        printf("[DEBUG] Result of get_exposure_time (fval): %d\n", fval);
        printf("[DEBUG] Result of get_exposure_time (&fval): %d\n", &fval);
        printf("[DEBUG] Result of get_exposure_time (response): %d\n", response);

        if (res) {
            resplen = sprintf(response, "Error getting exposure time.");
        } else {
            resplen = sprintf(response, "%0.2f", fval);
        };
      };

      };
  
        //for bias: if favl != 0 -> return ERR
        //add DEBUG preprocesser directives

            // exit command received
      if( strcmp(cmd,"exit")==0){
        retval = 0;
        resplen = sprintf(response,"Exit requested.");
      };

      // help
      if( strcmp(cmd,"help")==0){
	resplen = sprintf(response,"%s",helpstr);
      };

  //     // set/get shutter mode
  //       if (strcmp(cmd,"shutter_mode")==0){
	// if(argc == 1){
	//   fval = atof(arg);
	//     res = set_shutter(fval);
	//     if(res){
	//       resplen = sprintf(response,"Error setting shutter mode.");
	//     } else { 
	//       resplen = sprintf(response,"%0.2f",val);
	//     };
	//   } else {
	//     res = get_shutter(&val);
	//     if (res){
	//       resplen = sprintf(response,"Error getting shutter mode.");
	//     } else {
	//       resplen = sprintf(response,"%0.2f",fval);
	//     };
	// }; //argument list
  //     };

    if (strcmp(cmd, "temp") == 0) {
      if(argc == 1){
        printf(arg);

        char ints[] = "0123456789";
        const char *nonDigits = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_=+[]{}|;:'\",.<>?/\\ \t\n"; 
        int value = atoi(arg);

        if (value >= -80 && value <= 90 && strpbrk(arg, ints)!=NULL && strpbrk(arg, nonDigits)==NULL){
          fval = atof(arg); //convert to string

          printf("[DEBUG] Parsed fval from arg: %f\n", fval);

          res = set_temp(fval);
          printf("[DEBUG] Result of set_temp: %d\n", res);

          if (res) {
              resplen = sprintf(response, "Error setting temp.");
          } else {
              resplen = sprintf(response, "%0.2f", fval);
          }
          printf("[DEBUG] Response: %s\n", response);
          printf("[DEBUG] resplen: %d\n", resplen);
        // } else {
        //     printf("err");
        //     return ERR;
        }
      };
      if(argc == 0){
        printf("Number of arguments: %d\n", argc);
        res = get_temp(&fval);
        printf("[DEBUG] Result of get_temp: %d\n", res);
        printf("[DEBUG] Result of get_temp pointer: %d\n", fval);

        if (res) {
            resplen = sprintf(response, "Error getting temp.");
        } else {
            resplen = sprintf(response, "%0.2f", fval);
        };
      };

      };

    // //set/get analog gain
    // if (strcmp(cmd, "gain") == 0) {
    //   if(argc == 1){
    //     printf(arg);

    //     char ints[] = "0123";
    //     const char *nonDigits = "456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()-_=+[]{}|;:'\",.<>?/\\ \t\n"; 
    //     int value = atoi(arg);

    //     if (value >= 0 && value <= 3 && strpbrk(arg, ints)!=NULL && strpbrk(arg, nonDigits)==NULL){
    //       fval = atof(arg); //convert to string

    //       printf("[DEBUG] Parsed fval from arg: %f\n", fval);

    //       res = set_analog_gain(fval);
    //       printf("[DEBUG] Result of set_analog_gain: %d\n", res);

    //       if (res) {
    //           resplen = sprintf(response, "Error setting gain.");
    //       } else {
    //           resplen = sprintf(response, "%0.2f", fval);
    //       }
    //       printf("[DEBUG] Response: %s\n", response);
    //       printf("[DEBUG] resplen: %d\n", resplen);
    //     // } else {
    //     //     printf("err");
    //     //     return ERR;
    //     }
    //   };
    //   if(argc == 0){
    //     printf("Number of arguments: %d\n", argc);
    //     res = get_analog_gain(&fval);
    //     printf("[DEBUG] Result of get_analog_gain: %d\n", res);
    //     if (res) {
    //         resplen = sprintf(response, "Error getting gain.");
    //     } else {
    //         resplen = sprintf(response, "%0.2f", fval);
    //     };
    //   };
    // };

      // set/get analog gain
        if (strcmp(cmd,"analog_gain")==0){
          if(argc == 1){
            char ints[] = "0123";
            const char *nonDigits = "456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()-_=+[]{}|;:'\",.<>?/\\ \t\n"; 
            int value = atoi(arg);
            if (value >= 0 && value <= 3 && strpbrk(arg, ints)!=NULL && strpbrk(arg, nonDigits)==NULL){
              fval = atof(arg);
                res = set_analog_gain(fval);
                if(res){
                  resplen = sprintf(response,"Error setting analog gain.");
                } else { 
                  resplen = sprintf(response,"%0.2f",val);
                }
            }
          }
          if(argc == 0){
              res = get_analog_gain(&val);
              if (res){
                resplen = sprintf(response,"Error getting analog gain.");
              } else {
                resplen = sprintf(response,"%0.2f",val);
              };
          }; //argument list
        
        };

//new
  //       if (strcmp(cmd,"rois")==0){
	// if(argc == 4){
	//   fval = atof(arg);
	//     res = set_rois(fval);
	//     if(res){
	//       resplen = sprintf(response,"Error setting roi values.");
	//     } else { 
	//       resplen = sprintf(response,"%0.2f",val);
	//     };
	//   } else {
	//     res = get_analog_gain(&val);
	//     if (res){
	//       resplen = sprintf(response,"Error getting analog gain.");
	//     } else {
	//       resplen = sprintf(response,"%0.2f",val);
	//     };
	// }; //argument list
  //     };

      // burst
    if (strcmp(cmd, "burst") == 0) {
        if (argc == 1) {  
            fval = atof(arg);  // Ensure fval is declared here
            res = burst((int)fval);
            if (res == 0) {
                resplen = sprintf(response, "Burst exposure of %d images complete.", (int)fval);
            } else {
                resplen = sprintf(response, "Burst exposure failed.");
            };
        } else {
            resplen = sprintf(response, "Usage: burst <number_of_exposures>");
        };
    };
//end new
     if (strcmp(cmd,"expose")==0){
	// res = expose("exposure_file.raw");

  res = expose("exposure_file");
	if (res){
	  resplen = sprintf(response,"Exposure error.");
	} else {
	  resplen = sprintf(response,"Exposure complete.");
	};
      };

      if (strcmp(cmd,"dark")==0){
	res = dark("dark_file");
	if (res){
	  resplen = sprintf(response,"Exposure error.");
	} else {
	  resplen = sprintf(response,"Exposure complete.");
	};
      };

      if (strcmp(cmd,"bias")==0){
	res = bias("bias_file");
	if (res){
	  resplen = sprintf(response,"Exposure error.");
	} else {
	  resplen = sprintf(response,"Exposure complete.");
	};
      };

  // if (strcmp(cmd,"genfits")==0){
  //   int handle = 1;
  //   res = generate_fits("bin/exposure_file.raw", 1, &handle);

  //   if (res){
  //     resplen = sprintf(response," .");
  //   } else {
  //     resplen = sprintf(response,"Exposure complete.");
  //   };
  //       };
      
    // } else {
    //   // if the command is null, return the defaults
    // };

    
    // *********************************************
    
    n = write(newsockfd,response,resplen);
    if (n < 0) error("ERROR writing to socket");
    close(newsockfd);
    return retval;
  } else { 
    return OK;
  };
  return 1;
};
