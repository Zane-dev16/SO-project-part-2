#ifndef COMMON_IO_H
#define COMMON_IO_H

#include <unistd.h>


/// Parses an unsigned integer from the given file descriptor.
/// @param fd The file descriptor to read from.
/// @param value Pointer to the variable to store the value in.
/// @param next Pointer to the variable to store the next character in.
/// @return 0 if the integer was read successfully, 1 otherwise.
int parse_uint(int fd, unsigned int *value, char *next);

/// Prints an unsigned integer to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param value The value to write.
/// @return 0 if the integer was written successfully, 1 otherwise.
int print_uint(int fd, unsigned int value);

/// Writes a string to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param str The string to write.
/// @return 0 if the string was written successfully, 1 otherwise.
int print_str(int fd, const char *str);

int print_pipe_name(int fd, const char *str);

int read_pipe(int fd, void *buffer, size_t num_chars);

int write_arg(int fd, const void *buf, size_t count);

void create_fifo(const char *fifo_name);

void unlink_fifo(const char *fifo_name);

#endif  // COMMON_IO_H
