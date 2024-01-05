#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"


#define ACTIVE 0
#define INACTIVE 1

char req_pipe_names[FIFO_NAME_SIZE][MAX_SESSION_COUNT];
char resp_pipe_names[FIFO_NAME_SIZE][MAX_SESSION_COUNT];
int session_id_queue[MAX_SESSION_COUNT];
int consptr = 0;
int prodptr = 0;
int active_session_count = 0;
int session_request_count = 0;
int session_states[MAX_SESSION_COUNT];

pthread_cond_t has_session_cond;
pthread_cond_t session_max_cond;
pthread_mutex_t mutex;

volatile sig_atomic_t sigusr1_received = 0;

void sigusr1_handler() {
  sigusr1_received = 1;
}

void * process_client() {
  // ignores SIGUSR1 signal
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
  while (1) {
    int client_session_id;

    if (pthread_mutex_lock(&mutex)) {
      fprintf(stderr, "Lock Error");
      exit(EXIT_FAILURE);
    }
    while (session_request_count == 0) {
      if (pthread_cond_wait(&has_session_cond, &mutex)) {
        fprintf(stderr, "Condition Variable Error");
        exit(EXIT_FAILURE);
      }
    }
    int session_id = session_id_queue[consptr]; 
    consptr++; 
    if (consptr == MAX_SESSION_COUNT) consptr = 0;
    session_request_count--;
    active_session_count++;
    if (pthread_mutex_unlock(&mutex)) {
      fprintf(stderr, "Lock Error");
      exit(EXIT_FAILURE);
    }

    int req_pipe_fd = open(req_pipe_names[session_id], O_RDONLY);
    if (req_pipe_fd == -1) {
      fprintf(stderr, "Failed to open file\n");
      if (pthread_mutex_lock(&mutex)) {
        fprintf(stderr, "Lock Error");
        exit(EXIT_FAILURE);
      }
      session_states[session_id] = INACTIVE;
      active_session_count--;
      if (pthread_cond_signal(&session_max_cond)) {
        fprintf(stderr, "Condition Variable Error");
        exit(EXIT_FAILURE);
      }
      if (pthread_mutex_unlock(&mutex)) {
        fprintf(stderr, "Lock Error");
        exit(EXIT_FAILURE);
      }
      continue;
    }
    int resp_pipe_fd = open(resp_pipe_names[session_id], O_WRONLY);
    if (resp_pipe_fd == -1) {
      fprintf(stderr, "Failed to open file\n");
      if (close(req_pipe_fd)) {
        fprintf(stderr, "Error closing fd");
        exit(EXIT_FAILURE);
      }
      if (pthread_mutex_lock(&mutex)) {
        fprintf(stderr, "Lock Error");
        exit(EXIT_FAILURE);
      }
      session_states[session_id] = INACTIVE;
      active_session_count--;
      if (pthread_cond_signal(&session_max_cond)) {
        fprintf(stderr, "Condition Variable Error");
        exit(EXIT_FAILURE);
      }
      if (pthread_mutex_unlock(&mutex)) {
        fprintf(stderr, "Lock Error");
        exit(EXIT_FAILURE);
      }
      continue;
    }
    if (write_arg(resp_pipe_fd, &session_id, sizeof(int))) {
      fprintf(stderr, "failed writing session id\n");
      if (close(req_pipe_fd)) {
        fprintf(stderr, "Error closing fd");
        exit(EXIT_FAILURE);
      }
      if (close(resp_pipe_fd)) {
        fprintf(stderr, "Error closing fd");
        exit(EXIT_FAILURE);
      }
      if (pthread_mutex_lock(&mutex)) {
        fprintf(stderr, "Lock Error");
        exit(EXIT_FAILURE);
      }
      session_states[session_id] = INACTIVE;
      active_session_count--;
      if (pthread_cond_signal(&session_max_cond)) {
        fprintf(stderr, "Condition Variable Error");
        exit(EXIT_FAILURE);
      }
      if (pthread_mutex_unlock(&mutex)) {
        fprintf(stderr, "Lock Error");
        exit(EXIT_FAILURE);
      }
      continue;
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
        break;
      }
      if (ret == -1) {
          // ret == -1 indicates error
          fprintf(stderr, "read failed\n");
          break;
      }

      if (read_pipe(req_pipe_fd, &client_session_id, sizeof(int))) {
        fprintf(stderr, "failed reading session id\n");
        break;
      }

      switch (op_code) {
        case OP_QUIT:
          fifo_is_open = 0;
          break;
        case OP_CREATE:
          if (read_pipe(req_pipe_fd, &event_id, sizeof(unsigned int))) {
            fprintf(stderr, "failed reading op create\n");
            fifo_is_open = 0;
          }
          if (read_pipe(req_pipe_fd, &num_rows, sizeof(size_t))) {
            fprintf(stderr, "failed reading op create\n");
            fifo_is_open = 0;
          }
          if (read_pipe(req_pipe_fd, &num_columns, sizeof(size_t))) {
            fprintf(stderr, "failed reading op create\n");
            fifo_is_open = 0;
          }
          response = ems_create(event_id, num_rows, num_columns);
          if (write_arg(resp_pipe_fd, &response, sizeof(int))) {
            fprintf(stderr, "failed writing response\n");
            fifo_is_open = 0;
          }
          break;
        case OP_RESERVE:
          if (read_pipe(req_pipe_fd, &event_id, sizeof(unsigned int))) {
            fprintf(stderr, "failed reading op reserve\n");
            fifo_is_open = 0;
          }
          if (read_pipe(req_pipe_fd, &num_coords, sizeof(size_t))) {
            fprintf(stderr, "failed reading op reserve\n");
            fifo_is_open = 0;
          }

          if (read_pipe(req_pipe_fd, xs, sizeof(size_t) * num_coords)) {
            fprintf(stderr, "failed reading op reserve\n");
            fifo_is_open = 0;
          }

          if (read_pipe(req_pipe_fd, ys, sizeof(size_t) * num_coords)) {
            fprintf(stderr, "failed reading op reserve\n");
            fifo_is_open = 0;
          }

          response = ems_reserve(event_id, num_coords, xs, ys);

          if (write_arg(resp_pipe_fd, &response, sizeof(int))) {
            fprintf(stderr, "failed writing response\n");
            fifo_is_open = 0;
          }
          break;
        case OP_SHOW:
          if (read_pipe(req_pipe_fd, &event_id, sizeof(unsigned int))) {
            fprintf(stderr, "failed reading op show\n");
            fifo_is_open = 0;
          }
          response = ems_show(resp_pipe_fd, event_id);
          if (response == -1) {
            fifo_is_open = 0;
          }
          if (response == 1) {
            if (write_arg(resp_pipe_fd, &response, sizeof(int))) {
              fprintf(stderr, "failed writing response\n");
            }
          }
          break;
        case OP_LIST:
          response = ems_list_events(resp_pipe_fd);
          if (response == -1) {
            fifo_is_open = 0;
          }
          if (response == 1) {
            if (write_arg(resp_pipe_fd, &response, sizeof(int))) {
              fprintf(stderr, "failed writing response\n");
            }
          }
          break;
        default:
          fprintf(stderr, "Invalid op code: %c\n", op_code);
          fifo_is_open = 0;
          break;
      }
    }
    if (pthread_mutex_lock(&mutex)) {
      fprintf(stderr, "Lock Error");
      exit(EXIT_FAILURE);
    }
    session_states[session_id] = INACTIVE;
    if (close(req_pipe_fd)) {
      fprintf(stderr, "Error closing fd");
      exit(EXIT_FAILURE);
    }
    if (close(resp_pipe_fd)) {
      fprintf(stderr, "Error closing fd");
      exit(EXIT_FAILURE);
    }
    active_session_count--;
    if (pthread_cond_signal(&session_max_cond)) {
      fprintf(stderr, "Condition Variable Error");
      exit(EXIT_FAILURE);
    }
    if (pthread_mutex_unlock(&mutex)) {
      fprintf(stderr, "Lock Error");
      exit(EXIT_FAILURE);
    }
  }
}

int main(int argc, char* argv[]) {

  // redefines SIGUSR1 function treatment
  struct sigaction sa;
  sa.sa_handler = sigusr1_handler;
  sa.sa_flags = 0;
  sigaction(SIGUSR1, &sa, NULL);

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

  int server_pipe_fd = open(server_pipe_path, O_RDWR);
  if (server_pipe_fd == -1) {
    fprintf(stderr, "Failed to open file\n");
    return 1;
  }

  pthread_t th[MAX_SESSION_COUNT];
  if (pthread_mutex_init(&mutex, NULL)) {
    fprintf(stderr, "Lock Error");
    exit(EXIT_FAILURE);
  }
  if (pthread_cond_init(&session_max_cond, NULL)) {
    fprintf(stderr, "Condition Variable Error");
    exit(EXIT_FAILURE);
  }
  if (pthread_cond_init(&has_session_cond, NULL)) {
    fprintf(stderr, "Condition Variable Error");
    exit(EXIT_FAILURE);
  }
  for (unsigned int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (pthread_create(&th[i], NULL, process_client, NULL) != 0) {
        fprintf(stderr, "Failed to create thread");
        return 1;
    }
  }

  for (int i = 0 ; i < MAX_SESSION_COUNT ; i++) {
    session_states[i] = INACTIVE;
  }

  while (1) {

    char op_code;

    if (sigusr1_received) {
      show_status();
      sigusr1_received = 0;
    }

    ssize_t bytes_read = read(server_pipe_fd, &op_code, sizeof(char));

    if (bytes_read <= 0) {
      // check SIGNAL
      if (errno == EINTR) {
        continue;
      }
      fprintf(stderr, "read failed\n");
      exit(EXIT_FAILURE);
    }

    if (op_code == OP_SETUP) {
      if (pthread_mutex_lock(&mutex)) {
        fprintf(stderr, "Lock Error");
        exit(EXIT_FAILURE);
      }
      while (active_session_count == MAX_SESSION_COUNT || session_request_count == MAX_SESSION_COUNT) {
        if (pthread_cond_wait(&session_max_cond, &mutex)) {
          fprintf(stderr, "Condition Variable Error");
          exit(EXIT_FAILURE);
        }
      }
      int session_id;
      for (session_id = 0 ; session_id < MAX_SESSION_COUNT ; session_id++) {
        if (session_states[session_id] == INACTIVE) break;
      }

      if (session_id == MAX_SESSION_COUNT) {
        fprintf(stderr, "cannot activate session");
        exit(EXIT_FAILURE);
      }

      session_states[session_id] = ACTIVE;
      session_id_queue[prodptr] = session_id;
      prodptr++;
      if (prodptr == MAX_SESSION_COUNT) prodptr = 0;
      session_request_count++;

      if (read_pipe(server_pipe_fd, req_pipe_names[session_id], FIFO_NAME_SIZE)) {
        fprintf(stderr, "req pipe name reading failed\n");
        return 1;
      }

      if (read_pipe(server_pipe_fd, resp_pipe_names[session_id], FIFO_NAME_SIZE)) {
        fprintf(stderr, "res pipe name reading failed\n");
        return 1;
      }
      if (pthread_cond_signal(&has_session_cond)) {
        fprintf(stderr, "Condition Variable Error");
        exit(EXIT_FAILURE);
      }
      if (pthread_mutex_unlock(&mutex)) {
        fprintf(stderr, "Lock Error");
        exit(EXIT_FAILURE);
      }
    }
    else {
      fprintf(stderr, "invalid operation code %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  if (pthread_mutex_destroy(&mutex)) {
    fprintf(stderr, "Lock Error");
    exit(EXIT_FAILURE);
  }
  if (pthread_cond_destroy(&session_max_cond)) {
    fprintf(stderr, "Lock Error");
    exit(EXIT_FAILURE);
  }
  if (pthread_cond_destroy(&has_session_cond)) {
    fprintf(stderr, "Lock Error");
    exit(EXIT_FAILURE);
  }
  for (unsigned int i = 0; i < MAX_SESSION_COUNT; i++) {
    if (pthread_join(th[i], NULL) != 0) {
        fprintf(stderr, "Failed to create thread");
        return 1;
    }
  }
  if (close(server_pipe_fd) == -1) {
    fprintf(stderr, "Error closing fd");
    exit(EXIT_FAILURE);
  }

  ems_terminate();
}