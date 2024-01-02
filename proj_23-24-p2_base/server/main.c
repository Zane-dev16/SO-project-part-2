#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

char req_pipe_names[FIFO_NAME_SIZE][MAX_SESSION_COUNT];
char resp_pipe_names[FIFO_NAME_SIZE][MAX_SESSION_COUNT];

void unlink_fifo(const char *fifo_name) {
    // Unlink existing FIFO
    if (unlink(fifo_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void create_fifo(const char *fifo_name) {
    // Unlink existing FIFO
    unlink_fifo(fifo_name);

    // Create new FIFO
    if (mkfifo(fifo_name, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void process_client(int req_pipe_fd, int resp_pipe_fd) {
  int fifo_is_open = 1;
  while (fifo_is_open) {
    unsigned int event_id, delay, thr_id;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
    char op_code;

    ssize_t ret = read(req_pipe_fd, &op_code, sizeof(char));
    if (ret == 0) {
      printf("client no longer accessible\n");
      break;
    }
    if (ret == -1) {
        // ret == -1 indicates error
        fprintf(stderr, "read failed here\n");
        exit(EXIT_FAILURE);
    }

    switch (op_code) {
      case OP_QUIT:
        printf("quitting...\n");
        fifo_is_open = 0;
        break;
      case OP_CREATE:
        if (read_pipe(req_pipe_fd, &num_rows, sizeof(size_t))) {
          fprintf(stderr, "failed reading op create\n");
        }
        if (read_pipe(req_pipe_fd, &num_columns, sizeof(size_t))) {
          fprintf(stderr, "failed reading op create\n");
        }
        printf("in create %d %d\n", num_rows, num_columns);
        break;
      case OP_RESERVE:
        printf("in reserve\n");
        break;
      case OP_SHOW:
        printf("in show\n");
        break;
      case OP_LIST:
        printf("in list\n");
        break;
      default:
        fprintf(stderr, "Invalid op code: %c\n", op_code);
        break;
    }
  }
  unlink_fifo(req_pipe_names[0]);
  unlink_fifo(resp_pipe_names[0]);
}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  const char *server_pipe_path = argv[1];

  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  create_fifo(server_pipe_path);

  int server_pipe_fd = open(server_pipe_path, O_RDONLY);
  if (server_pipe_fd == -1) {
    fprintf(stderr, "Failed to open file\n");
    return 1;
  }
  //TODO: create worker threads
  while (1) {
    char op_code;

    ssize_t ret = read(server_pipe_fd, &op_code, sizeof(char));
    if (ret == 0) {
      continue;
    }
    if (ret == -1) {
        // ret == -1 indicates error
        fprintf(stderr, "read failed\n");
        exit(EXIT_FAILURE);
    }

    if (op_code == OP_SETUP) {
      if (read_pipe(server_pipe_fd, req_pipe_names[0], FIFO_NAME_SIZE) == -1) {
        fprintf(stderr, "req pipe name reading failed\n");
        return 1;
      }
      create_fifo(req_pipe_names[0]);

      if (read_pipe(server_pipe_fd, resp_pipe_names[0], FIFO_NAME_SIZE) == -1) {
        fprintf(stderr, "res pipe name reading failed\n");
        return 1;
      }
      create_fifo(resp_pipe_names[0]);
      printf("opening\n");
      int req_pipe_fd = open(req_pipe_names[0], O_RDONLY);
      printf("opened\n");
      if (req_pipe_fd == -1) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
      }

      printf("opening\n");
      int resp_pipe_fd = open(resp_pipe_names[0], O_WRONLY);
      printf("opened\n");

      if (resp_pipe_fd == -1) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
      }
      process_client(req_pipe_fd, resp_pipe_fd);
    }
    else {
      printf("%c", op_code);
      fprintf(stderr, "invalid operation code %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  close(server_pipe_fd);

  ems_terminate();
}