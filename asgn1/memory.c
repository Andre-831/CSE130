#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>

#define BUFFER_SIZE 4096

void invalid_command(void) {
    write(2, "Invalid Command\n", 16);
    exit(1);
}

void operation_failed(void) {
    write(2, "Operation Failed\n", 17);
    exit(1);
}

/*
* Write a string to an already-open text file.
    
    -writes num_chars bytes to fd
    -handles short writes by looping until all bytes are written

*/
void str_write(int fd, char *buf, size_t num_chars) {
    ssize_t result;

    size_t num_written = 0;

    while (num_written < num_chars) {
        result = write(fd, buf + num_written, num_chars - num_written);

        if (result <= 0)
            operation_failed();

        num_written += result;
    }
}

ssize_t str_read(int fd, char *buf, size_t size) {
    ssize_t total = 0;

    while (total < (ssize_t) size) {
        ssize_t result = read(fd, buf + total, size - total);

        if (result < 0)
            operation_failed();

        if (result == 0)
            break;

        total += result;
    }

    return total;
}

/*
    -reads a single line from fd ( ending with '\n')
    -reads one byte at a time until newline or EOF is reached


*/

ssize_t read_line(int fd, char *buf, size_t size) {
    size_t total = 0;
    char c;

    while (total < size) {
        ssize_t result = read(fd, &c, 1);

        if (result < 0)
            operation_failed();

        if (result == 0)
            break;

        buf[total++] = c;
        if (c == '\n')
            break;
    }
    buf[total] = '\0'; // null-terminate the string
    return total;
}

int main(void) {

    char command[BUFFER_SIZE];
    char filename[BUFFER_SIZE];
    char length_str[BUFFER_SIZE];

    // read the command, filename, and length from stdin
    if (read_line(0, command, BUFFER_SIZE) <= 0)
        invalid_command();

    //remove newline
    char *newline = strchr(command, '\n');
    if (newline == NULL) {
        invalid_command();
    }
    *newline = '\0';

    //read filename
    if (read_line(0, filename, BUFFER_SIZE) <= 0)
        invalid_command();

    newline = strchr(filename, '\n');
    if (newline == NULL) {
        invalid_command();
    }
    *newline = '\0';

    //check if filename is valid
    if (strlen(filename) == 0 || strlen(filename) > PATH_MAX)
        invalid_command();

    if (strcmp(command, "get") == 0) {

        char extra; //no extra input after filename
        if (read_line(0, &extra, 1) != 0)
            invalid_command();

        int fd = open(filename, O_RDONLY);

        if (fd < 0)
            invalid_command();

        char buf[BUFFER_SIZE];

        ssize_t bytes;

        //read file and write to stdout
        while ((bytes = read(fd, buf, BUFFER_SIZE)) > 0) {
            str_write(1, buf, bytes);
        }

        if (bytes < 0) {
            close(fd);
            invalid_command();
        }

        if (close(fd) < 0)
            operation_failed();

        close(fd);
        return 0;
    }

    else if (strcmp(command, "set") == 0) {

        if (read_line(0, length_str, BUFFER_SIZE) <= 0)
            invalid_command(); //length line

        newline = strchr(length_str, '\n');
        if (!newline) {
            invalid_command();
        }
        *newline = '\0';

        char *endptr; //length str to int
        long length = strtol(length_str, &endptr, 10);
        if (*endptr != '\0')
            invalid_command();

        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0660);
        if (fd < 0)
            operation_failed();

        char buffer[BUFFER_SIZE];
        long remaining = length;

        while (remaining > 0) { // read "length" bytes
            ssize_t to_read = remaining > BUFFER_SIZE ? BUFFER_SIZE : remaining;
            ssize_t bytes_read = read(0, buffer, to_read);
            if (bytes_read <= 0) {
                close(fd);
                operation_failed();
            }

            str_write(fd, buffer, bytes_read);
            remaining -= bytes_read;
        }
        if (close(fd) < 0)
            operation_failed();

        str_write(1, "OK\n", 3);
        return 0;
    }
    invalid_command();
    return 0;
}
