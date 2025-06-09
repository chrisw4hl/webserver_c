#include <arpa/inet.h>
#include <errno.h>
#include <iso646.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MYPORT "80"
#define BACKLOG 10
#define MAX_LINE_LENGTH 256

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void *append_body(char *header, int *header_size, void *body, size_t body_len) {
  printf("%s\n", header);
  void *new_dest = realloc(header, *header_size + body_len);
  printf("%s\n", header);
  if (new_dest == NULL) {
    perror("append failure");
  }
  memcpy(new_dest + *header_size, body, body_len);
  *header_size += body_len;
  printf("%s\n", header);
  return new_dest;
}

int main(void) {
  struct addrinfo hints, *res;
  socklen_t addr_size;
  int rv;
  struct sockaddr_storage their_addr;
  int sockfd, new_fd;
  char ipstr[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, MYPORT, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) ==
      -1) {
    perror("server: socket");
  }

  if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    close(sockfd);
    perror("server: bind");
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  freeaddrinfo(res);

  printf("server: waiting for connections...\n");

  char recv_buf[1024];
  while (1) {
    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr),
              ipstr, sizeof ipstr);

    printf("server: got connection from %s\n", ipstr);

    if (!fork()) {
      close(sockfd);

      if (recv(new_fd, recv_buf, 1024, 0) == -1)
        perror("send");
      printf("%s\n", recv_buf);

      char comp_get[4] = "GET";
      char comp_dir_start[2] = "/";
      char comp_dir_ver[9] = "HTTP/1.1";
      char *test_get = strstr(recv_buf, comp_get);
      char *test_dir_start = strstr(recv_buf, comp_dir_start);
      char *test_dir_end = strstr(recv_buf, comp_dir_ver);
      char *directory = (char *)calloc(1, test_dir_end - test_dir_start + 1);
      memcpy(directory, test_dir_start, test_dir_end - test_dir_start - 1);

      bool is_get = false;
      char *header_fmt = "HTTP/1.1 %s\nContent-Type: %s\nDate:%d-%02d-%02d "
                         "%02d:%02d:%02d\nLast-Modified: Fri, 10 Sep 2019 "
                         "02:20:20 GMT\nContent-Length: %zu\r\n\r\n";
      // printf("%s\n", directory);

      char filename[256];

      char content_type[32];

      if (0 == strncmp(directory, "/", strlen(directory))) {
        snprintf(filename, sizeof(filename), "./index.html");
      } else {
        snprintf(filename, sizeof(filename), ".%s", directory);
      }

      printf("%s\n", &filename[1]);
      char *file_extension_start = strstr(&filename[1], ".");
      char *file_extension_end = filename + strlen(filename);
      char *file_extension =
          (char *)calloc(1, file_extension_end - file_extension_start + 1);
      memcpy(file_extension, file_extension_start,
             file_extension_end - file_extension_start);

      if (strncmp(file_extension, ".html", strlen(file_extension)) == 0) {
        snprintf(content_type, sizeof(content_type),
                 "text/html; charset=UTF-8");
      } else if (strncmp(file_extension, ".jpeg", strlen(file_extension)) ==
                 0) {
        snprintf(content_type, sizeof(content_type), "image/jpeg");
      }

      FILE *html_file = fopen(filename, "r");
      if (html_file == NULL) {
        perror("fileread");
      }

      fseek(html_file, 0, SEEK_END);
      long length = ftell(html_file);
      fseek(html_file, 0, SEEK_SET);
      void *html_doc = malloc(length);

      if (html_doc) {
        fread(html_doc, 1, length, html_file);
      }
      fclose(html_file);

      // printf("%s", html_doc);

      /*char *html_doc =
          "<!DOCTYPE html>\n<html>\n<head>\n<title>Server Hello World: "
          "Static "
          "Website</title>\n</head>\n<body>\n<h1>Happy Coding</h1>\n<p>Hello "
          "world!</p>\n</body>\n</html>\n";
      */
      char *not_found_doc =
          "<!DOCTYPE html><html><head><title>SERVER 404: FILE "
          "NOT FOUND</title></head><body><h1>404 NOT "
          "FOUND</h1></body></html>\r\n\r\n";

      char *status;

      if (test_get == recv_buf) {
        is_get = true;
      }

      if (is_get == true) {

        status = "200 OK";
        //   printf("%s\n", directory);
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char *header = (char *)calloc(400, sizeof(char));
        if (html_file == NULL) {
          //    printf("%s\n", header);
          status = "404 NOT FOUND";
          printf("SENDING 404:\n");
          snprintf(header, 400, header_fmt, status, content_type,
                   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                   tm.tm_min, tm.tm_sec, strlen(not_found_doc));
          int *header_len = malloc(sizeof(int));
          *header_len = strlen(header);
          header =
              append_body(header, header_len, html_doc, strlen(not_found_doc));
          //   printf("%s", header);
          int result = send(new_fd, header, *header_len, 0);
          //  printf("%zu\n", sizeof(header));
          // printf("%i\n", result);
        } else {

          // printf("%s\n", header);
          snprintf(header, 400, header_fmt, status, content_type,
                   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                   tm.tm_min, tm.tm_sec, length);
          int *header_len = malloc(sizeof(int));
          *header_len = strlen(header);
          printf("%i\n", *header_len);
          header = append_body(header, header_len, html_doc, length);

          printf("%s\n", header);
          int result = send(new_fd, header, *header_len, 0);
          printf("%i\n", result);
          free(header);
          free(header_len);
        }
      }
      free(directory);
      free(html_doc);
      free(file_extension);

      exit(0);
    }

    printf("connection closed");
    close(new_fd);
  }
  return 0;
}
