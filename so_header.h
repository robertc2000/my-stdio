#include "so_stdio.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

#define SO_FILE_BUFFER_SIZE 4096
#define ERROR -1

#define READ 1
#define WRITE 2

struct _so_file {
	int fd;
	int read_cursor;
	int write_cursor;

	unsigned char *buffer;
	int last_op;
	int nr_bytes_read;
	long file_cursor;
	int eof_status;
	int error;
	int pid;
};
