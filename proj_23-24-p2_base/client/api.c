#include "api.h"
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "common/io.h"
#include "common/constants.h"

int req_pipe_fd;
int resp_pipe_fd;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  int server_pipe_fd = open(server_pipe_path, O_WRONLY);
  if (server_pipe_fd == -1) {
    fprintf(stderr, "open failed\n");
    return 1;
  }

  if (print_str(server_pipe_fd, "1")) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  if(print_pipe_name(server_pipe_fd, req_pipe_path)) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }


  if(print_pipe_name(server_pipe_fd, resp_pipe_path)) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  sleep(1);

  req_pipe_fd = open(req_pipe_path, O_WRONLY);
  if (req_pipe_fd == -1) {
    fprintf(stderr, "Failed to open input file. Path: %s\n", req_pipe_path);
    return 1;
  }
  printf("open1\n");

  resp_pipe_fd = open(resp_pipe_path, O_RDONLY);
  if (resp_pipe_fd == -1) {
    fprintf(stderr, "Failed to open file\n");
    return 1;
  }
  printf("open2\n");

  return 0;
}

int ems_quit(void) { 
  if (print_str(req_pipe_fd, "2")) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  //TODO: close pipes
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}
