#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PORT "6969"
#define MAXDATASIZE 100

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void *get_in_addr_print(struct addrinfo *sa) {
  if (sa->ai_family == AF_INET) {
    return (struct sockaddr_in *)sa->ai_addr;
  }
  return (struct sockaddr_in6 *)sa->ai_addr;
}

int main(int argc, char *argv[]) {
  struct addrinfo hints, *res, *p;
  int status;
  char ipstr[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
    fprintf(stderr, "gettaddrinfo: %s\n", gai_strerror(status));
    return 2;
  }

  char addr_print[INET_ADDRSTRLEN];
  void *in_addr_print = get_in_addr_print(res);
  inet_ntop(res->ai_family, in_addr_print, addr_print, sizeof(addr_print));
  printf("Client IP addresses for %s:\n", addr_print);

  int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

  int connect_stat;
  if ((connect_stat = connect(s, res->ai_addr, res->ai_addrlen)) == -1) {
    perror("connect");
  }

  freeaddrinfo(res);
  char out_buf[MAXDATASIZE];
  int numbytes;

  if ((numbytes = recv(s, out_buf, MAXDATASIZE - 1, 0)) == -1) {
    perror("recv");
  }

  printf("%i\n", numbytes);

  out_buf[numbytes] = '\0';
  printf("%s\n", out_buf);
  return 0;
}
