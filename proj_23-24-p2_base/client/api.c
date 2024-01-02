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

  char op_code = OP_SETUP;
  if (write_arg(server_pipe_fd, &op_code, sizeof(char))) {
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
  char op_code = OP_QUIT;
  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  //TODO: close pipes
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  char op_code = OP_CREATE;
  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  if (write_arg(req_pipe_fd, &num_rows, sizeof(size_t))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  if (write_arg(req_pipe_fd, &num_cols, sizeof(size_t))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  char op_code = OP_RESERVE;
  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  char op_code = OP_SHOW;
  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  return 0;
}

int ems_list_events(int out_fd) {
  char op_code = OP_LIST;
  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  return 0;
}
