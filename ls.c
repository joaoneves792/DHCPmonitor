#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "topics.h"

int fd; //socket file descriptor

int main(int argc, char** argv) {
  char buffer[1024];
  char *cmd;
  int lsPort = 58050;
  int recvBytes;
  socklen_t addrlen;
  struct sockaddr_in serveraddr, clientaddr;

  //parse command line arguments
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-p")) lsPort = atoi(argv[++i]);
  }

  //TOPICS
  int nTopics = loadTopics();

  //build the AWT response
  char *awt = (char *) calloc(1024, sizeof(char));
  char *ptr = awt + sprintf(awt, "AWT %s %i", getSubject(), nTopics);
  for (int i = 0; i < nTopics; i++) ptr += sprintf(ptr, " %s", getTopic(i)->name);
  sprintf(ptr, "\n");

  //setup UDP server
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    printf("Error: socket()\n");
    exit(-1);
  }
  memset((void*) &serveraddr, 0, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short) lsPort);

  if (bind(fd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) == -1) {
    printf("Error: bind()\n");
    close(fd);
    exit(-1);
  }

  addrlen = sizeof(clientaddr);

  //wait for connections
  while ((recvBytes = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*) &clientaddr, &addrlen)) != -1) {
    //buffer[recvBytes] = 0; //trim
    //printf("Buffer: '%s'\n", buffer);
    cmd = strtok(buffer, " \n");
    printf("%s %s:%i\n", cmd, inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port);

    if (!strcmp(cmd, "RQT")) {
      sendto(fd, awt, strlen(awt)+1, 0, (struct sockaddr*) &clientaddr, addrlen);
    }
    else if (!strcmp(cmd, "RQC")) {
      Topic *topic = getTopic(atoi(strtok(NULL, " \n"))-1);
      char *awc = (char *) calloc(48, sizeof(char));
      sprintf(awc, "AWC %s %s %i\n", topic->name, topic->ip, topic->port);
      sendto(fd, awc, strlen(awc)+1, 0, (struct sockaddr*) &clientaddr, addrlen);
      free(awc);
    }
    else {
      printf("Error: unknown command: %s\n", cmd);
      close(fd);
      exit(-1);
    }
  }

  //recvfrom returned -1 breaking the cycle
  close(fd);
  printf("Error: recvfrom()\n");
  exit(-1);
  return 0;
}
