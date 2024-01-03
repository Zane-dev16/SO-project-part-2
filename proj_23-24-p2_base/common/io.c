#include "io.h"
#include "constants.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

int parse_uint(int fd, unsigned int *value, char *next) {
  char buf[16];

  int i = 0;
  while (1) {
    ssize_t read_bytes = read(fd, buf + i, 1);
    if (read_bytes == -1) {
      return 1;
    } else if (read_bytes == 0) {
      *next = '\0';
      break;
    }

    *next = buf[i];

    if (buf[i] > '9' || buf[i] < '0') {
      buf[i] = '\0';
      break;
    }

    i++;
  }

  unsigned long ul = strtoul(buf, NULL, 10);

  if (ul > UINT_MAX) {
    return 1;
  }

  *value = (unsigned int)ul;

  return 0;
}

int print_uint(int fd, unsigned int value) {
  char buffer[16];
  size_t i = 16;

  for (; value > 0; value /= 10) {
    buffer[--i] = '0' + (char)(value % 10);
  }

  if (i == 16) {
    buffer[--i] = '0';
  }

  while (i < 16) {
    ssize_t written = write(fd, buffer + i, 16 - i);
    if (written == -1) {
      return 1;
    }

    i += (size_t)written;
  }

  return 0;
}

int print_str(int fd, const char *str) {
  size_t len = strlen(str);
  while (len > 0) {
    ssize_t written = write(fd, str, len);
    if (written == -1) {
      printf("wrinting failed");
      return 1;
    }
    str += (size_t)written;
    len -= (size_t)written;
  }
  return 0;
}

int write_arg(int fd, const void *buf, size_t count) {
    size_t bytes_written_total = 0;

    while (bytes_written_total < count) {
        ssize_t bytes_written = write(fd, (char *)buf + bytes_written_total, count - bytes_written_total);

        if (bytes_written == -1) {
            fprintf(stderr, "Error writing to FIFO");
            return 1;
        }

        bytes_written_total += (size_t)bytes_written;
    }

    return 0;
}

int read_pipe(int fd, void *buffer, size_t num_chars) {
    size_t total_read = 0;

    while (total_read < num_chars) {
        ssize_t bytes_read = read(fd, (char *)buffer + total_read, num_chars - total_read);

        if (bytes_read == -1) {
            fprintf(stderr, "read failed");
            return 1;  // Read error
        } else if (bytes_read == 0) {
            break;  // End of file
        }

        total_read += (size_t)bytes_read;
    }

    return 0;  // Return the total number of characters read
}

void unlink_fifo(const char *fifo_name) {
    // Unlink existing FIFO
    if (unlink(fifo_name) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

void create_fifo(const char *fifo_name) {
    unlink_fifo(fifo_name);

    if (mkfifo(fifo_name, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}
