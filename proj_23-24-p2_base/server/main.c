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

void unlink_fifo(const char *fifo_name) {
    // Unlink existing FIFO
    if (unlink(fifo_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", fifo_name, strerror(errno));
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

  char req_pipe_names[FIFO_NAME_SIZE][MAX_SESSION_COUNT];
  char resp_pipe_names[FIFO_NAME_SIZE][MAX_SESSION_COUNT];
  int server_pipe_fd = open(server_pipe_path, O_RDONLY);
  if (server_pipe_fd == -1) {
    fprintf(stderr, "Failed to open file\n");
    return 1;
  }
  //TODO: create worker threads
  while (1) {
    char op_code[2];

    ssize_t ret = read(server_pipe_fd, op_code, 1);
    if (ret == 0) {
      continue;
    }
    if (ret == -1) {
        // ret == -1 indicates error
        fprintf(stderr, "read failed\n");
        exit(EXIT_FAILURE);
    }

    switch (op_code[0]) {
      case OP_SETUP: {
        if (read_pipe(server_pipe_fd, req_pipe_names[0], FIFO_NAME_SIZE)) {
          fprintf(stderr, "req pipe name reading failed\n");
          return 1;
        }
        create_fifo(req_pipe_names[0]);

        if (read_pipe(server_pipe_fd, resp_pipe_names[0], FIFO_NAME_SIZE)) {
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
        break;
      }

      case OP_QUIT:
        unlink_fifo(req_pipe_names[0]);
        unlink_fifo(resp_pipe_names[0]);
        break;
      default:
        printf("%c", op_code[0]);
        fprintf(stderr, "invalid operation code %s\n", strerror(errno));
        exit(EXIT_FAILURE);
        break;
    }
  }

  close(server_pipe_fd);

  ems_terminate();
}