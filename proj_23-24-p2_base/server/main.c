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
#define OP_SETUP '1'
#define OP_QUIT '2'

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

  // Remove server FIFO if it already exists
  if (unlink(server_pipe_path) != 0 && errno != ENOENT) {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", server_pipe_path,
            strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Create server FIFO
  if (mkfifo(server_pipe_path, 0640) != 0) {
      fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
  }

  int server_pipe_fd = open(server_pipe_path, O_RDONLY);

  //TODO: create worker threads
  while (1) {
    char op_code[1];

    ssize_t ret = read(server_pipe_fd, op_code, 1);
    if (ret == 0) {
        // ret == 0 indicates EOF
        return 0;
    } else if (ret == -1) {
        // ret == -1 indicates error
        fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    switch (op_code[0]) {
      case OP_SETUP:
        printf("we got it");
        break;
      case OP_QUIT:
        printf("quit");
        break;
      default:
        fprintf(stderr, "invalid operation code %s\n", strerror(errno));
        exit(EXIT_FAILURE);
        break;
    }
  }

  close(server_pipe_fd);

  ems_terminate();
}