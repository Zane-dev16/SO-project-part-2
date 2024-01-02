#include "api.h"
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "common/io.h"
#include "common/constants.h"

int req_pipe_fd;
int resp_pipe_fd;
char req_path[FIFO_NAME_SIZE];
char resp_path[FIFO_NAME_SIZE];
int session_id;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  strcpy(req_path, req_pipe_path);
  strcpy(resp_path, resp_pipe_path);

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

  create_fifo(req_pipe_path);
  create_fifo(resp_pipe_path);

  if(print_pipe_name(server_pipe_fd, req_pipe_path)) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  if(print_pipe_name(server_pipe_fd, resp_pipe_path)) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  req_pipe_fd = open(req_pipe_path, O_WRONLY);
  if (req_pipe_fd == -1) {
    fprintf(stderr, "Failed to open input file. Path: %s\n", req_pipe_path);
    return 1;
  }

  resp_pipe_fd = open(resp_pipe_path, O_RDONLY);
  if (resp_pipe_fd == -1) {
    fprintf(stderr, "Failed to open file\n");
    return 1;
  }
  if (read_pipe(resp_pipe_fd, &session_id, sizeof(int))) {
    return 1;
  }

  return 0;
}

int ems_quit(void) { 
  char op_code = OP_QUIT;
  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  if (write_arg(req_pipe_fd, &session_id, sizeof(int))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  unlink_fifo(req_path);
  unlink_fifo(resp_path);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  char op_code = OP_CREATE;
  int response = 0;

  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  if (write_arg(req_pipe_fd, &session_id, sizeof(int))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  if (write_arg(req_pipe_fd, &event_id, sizeof(unsigned int))) {
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
  if (read_pipe(resp_pipe_fd, &response, sizeof(int))) {
    return 1;
  }
  return response;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  char op_code = OP_RESERVE;
  int response = 0;
  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  if (write_arg(req_pipe_fd, &session_id, sizeof(int))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  if (write_arg(req_pipe_fd, &event_id, sizeof(unsigned int))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  if (write_arg(req_pipe_fd, &num_seats, sizeof(size_t))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  if (write_arg(req_pipe_fd, xs, sizeof(size_t) * num_seats)) {
      fprintf(stderr, "write to pipe failed\n");
      return 1;
  }

  if (write_arg(req_pipe_fd, ys, sizeof(size_t) * num_seats)) {
      fprintf(stderr, "write to pipe failed\n");
      return 1;
  }
  if (read_pipe(resp_pipe_fd, &response, sizeof(int))) {
    return 1;
  }
  return response;
}

int ems_show(int out_fd, unsigned int event_id) {
  int response = 0;
  char op_code = OP_SHOW;
  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  if (write_arg(req_pipe_fd, &session_id, sizeof(int))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  if (write_arg(req_pipe_fd, &event_id, sizeof(unsigned int))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  if (read_pipe(resp_pipe_fd, &response, sizeof(int))) {
    return 1;
  }
  if (response) {
    return response;
  }

  size_t rows;
  size_t cols;
  if (read_pipe(resp_pipe_fd, &rows, sizeof(size_t))) {
    fprintf(stderr, "failed reading op show response\n");
    return 1;
  }
  if (read_pipe(resp_pipe_fd, &cols, sizeof(size_t))) {
    fprintf(stderr, "failed reading op show response\n");
    return 1;
  }

  unsigned int *data = calloc(cols * rows, sizeof(unsigned int));
  if (read_pipe(resp_pipe_fd, data, sizeof(unsigned int) * rows * cols)) {
    fprintf(stderr, "failed reading op show response\n");
    return 1;
  }
  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      char buffer[16];
      sprintf(buffer, "%u", data[i * cols + j]);

      if (print_str(out_fd, buffer)) {
        perror("Error writing to file descriptor");
        return 1;
      }

      if (j < cols - 1) {
        if (print_str(out_fd, " ")) {
          perror("Error writing to file descriptor");
          return 1;
        }
      }
    }

    if (print_str(out_fd, "\n")) {
      perror("Error writing to file descriptor");
      return 1;
    }
  }
  free(data);
  return response;
}

int ems_list_events(int out_fd) {
  char op_code = OP_LIST;
  size_t num_events;
  int response = 0;

  if (write_arg(req_pipe_fd, &op_code, sizeof(char))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }
  if (write_arg(req_pipe_fd, &session_id, sizeof(int))) {
    fprintf(stderr, "write to pipe failed\n");
    return 1;
  }

  if (read_pipe(resp_pipe_fd, &response, sizeof(int))) {
    return 1;
  }
  if (response) {
    return response;
  }
  if (read_pipe(resp_pipe_fd, &num_events, sizeof(size_t))) {
      fprintf(stderr, "failed reading op list response\n");
      return 1;
  }

  unsigned int *event_ids = malloc(num_events * sizeof(unsigned int));
  if (event_ids == NULL) {
      perror("Memory allocation failed");
      return 1;
  }

  if (read_pipe(resp_pipe_fd, event_ids, num_events * sizeof(unsigned int))) {
      fprintf(stderr, "failed reading op list response\n");
      free(event_ids);
      return 1;
  }

  // Process the received data as needed
  for (size_t i = 0; i < num_events; i++) {
    char buff[] = "Event: ";
    if (print_str(out_fd, buff)) {
      perror("Error writing to file descriptor");
      return 1;
    }
    char id[16];
    sprintf(id, "%u\n", event_ids[i]);
    if (print_str(out_fd, id)) {
      perror("Error writing to file descriptor");
      return 1;
    }
  }
  // Free the allocated memory
  free(event_ids);

  return response;
}
