#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

#define ACTIVE 0
#define INACTIVE 1

char req_pipe_names[FIFO_NAME_SIZE][MAX_SESSION_COUNT];
char resp_pipe_names[FIFO_NAME_SIZE][MAX_SESSION_COUNT];
int session_id_queue[MAX_SESSION_COUNT];
int session_count = 0;
pthread_cond_t has_session_cond;
pthread_cond_t session_max_cond;
pthread_mutex_t mutex;

void * process_client() {
  pthread_mutex_lock(&mutex);
  while (session_count == 0) pthread_cond_wait(&has_session_cond, &mutex);
  pthread_mutex_unlock(&mutex);
  int session_id = 0;
  int client_session_id;
  int req_pipe_fd = open(req_pipe_names[session_id], O_RDONLY);
  if (req_pipe_fd == -1) {
    fprintf(stderr, "Failed to open file\n");
    exit(EXIT_FAILURE);
  }
  int resp_pipe_fd = open(resp_pipe_names[session_id], O_WRONLY);
  if (resp_pipe_fd == -1) {
    fprintf(stderr, "Failed to open file\n");
    exit(EXIT_FAILURE);
  }
  if (write_arg(resp_pipe_fd, &session_id, sizeof(int))) {
    fprintf(stderr, "failed writing session id\n");
  }

  int fifo_is_open = 1;
  while (fifo_is_open) {
    unsigned int event_id;
    int response;
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
        fprintf(stderr, "read failed\n");
        exit(EXIT_FAILURE);
    }

    if (read_pipe(req_pipe_fd, &client_session_id, sizeof(int))) {
      fprintf(stderr, "failed reading session id\n");
      exit(EXIT_FAILURE);
    }

    switch (op_code) {
      case OP_QUIT:
        printf("quitting\n");
        fifo_is_open = 0;
        break;
      case OP_CREATE:
        if (read_pipe(req_pipe_fd, &event_id, sizeof(unsigned int))) {
          fprintf(stderr, "failed reading op create\n");
        }
        if (read_pipe(req_pipe_fd, &num_rows, sizeof(size_t))) {
          fprintf(stderr, "failed reading op create\n");
        }
        if (read_pipe(req_pipe_fd, &num_columns, sizeof(size_t))) {
          fprintf(stderr, "failed reading op create\n");

        }
        printf("in create %d %d %d \n", event_id, num_rows, num_columns);
        response = ems_create(event_id, num_rows, num_columns);
        if (write_arg(resp_pipe_fd, &response, sizeof(int))) {
          fprintf(stderr, "failed writing response\n");
        }
        break;
      case OP_RESERVE:
        if (read_pipe(req_pipe_fd, &event_id, sizeof(unsigned int))) {
          fprintf(stderr, "failed reading op reserve\n");
        }
        if (read_pipe(req_pipe_fd, &num_coords, sizeof(size_t))) {
          fprintf(stderr, "failed reading op reserve\n");
        }

        if (read_pipe(req_pipe_fd, xs, sizeof(size_t) * num_coords)) {
          fprintf(stderr, "failed reading op reserve\n");
        }

        if (read_pipe(req_pipe_fd, ys, sizeof(size_t) * num_coords)) {
          fprintf(stderr, "failed reading op reserve\n");
        }

        printf("in reserve %d %d %d %d", event_id, num_coords, xs[0], ys[0]);
        response = ems_reserve(event_id, num_coords, xs, ys);

        if (write_arg(resp_pipe_fd, &response, sizeof(int))) {
          fprintf(stderr, "failed writing response\n");
        }
        break;
      case OP_SHOW:
        printf("in show");
        if (read_pipe(req_pipe_fd, &event_id, sizeof(unsigned int))) {
          fprintf(stderr, "failed reading op show\n");
        }
        response = ems_show(resp_pipe_fd, event_id);
        if (response) {
          if (write_arg(resp_pipe_fd, &response, sizeof(int))) {
            fprintf(stderr, "failed writing response\n");
          }
        }
        break;
      case OP_LIST:
        printf("in list");
        response = ems_list_events(resp_pipe_fd);
        if (response) {
          if (write_arg(resp_pipe_fd, &response, sizeof(int))) {
            fprintf(stderr, "failed writing response\n");
          }
        }
        break;
      default:
        fprintf(stderr, "Invalid op code: %c\n", op_code);
        break;
    }
  }
  int *returnValue = 0;
  return (void *)returnValue;
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

  pthread_t th[MAX_SESSION_COUNT];
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&session_max_cond, NULL);
  pthread_cond_init(&has_session_cond, NULL);
  for (unsigned int i = 0; i < 1; i++) {
    if (pthread_create(&th[i], NULL, process_client, NULL) != 0) {
        fprintf(stderr, "Failed to create thread");
        return 1;
    }
  }
  while (1) {
    char op_code;

    ssize_t bytes_read = read(server_pipe_fd, &op_code, sizeof(char));
    if (bytes_read == 0) {
      continue;
    }
    if (bytes_read == -1) {
        fprintf(stderr, "read failed\n");
        exit(EXIT_FAILURE);
    }

    if (op_code == OP_SETUP) {
      pthread_mutex_lock(&mutex);

      if (read_pipe(server_pipe_fd, req_pipe_names[0], FIFO_NAME_SIZE)) {
        fprintf(stderr, "req pipe name reading failed\n");
        return 1;
      }

      if (read_pipe(server_pipe_fd, resp_pipe_names[0], FIFO_NAME_SIZE)) {
        fprintf(stderr, "res pipe name reading failed\n");
        return 1;
      }
      session_count++;
      printf("signalling");
      pthread_cond_signal(&has_session_cond);
      printf("after signal");
      pthread_mutex_unlock(&mutex);
    }
    else {
      printf("%c", op_code);
      fprintf(stderr, "invalid operation code %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&session_max_cond);
  pthread_cond_destroy(&has_session_cond);
  for (unsigned int i = 0; i < 1; i++) {
    if (pthread_join(th[i], NULL) != 0) {
        fprintf(stderr, "Failed to create thread");
        return 1;
    }
  }
  close(server_pipe_fd);

  ems_terminate();
}