#include "so_header.h"
#include "string.h"

SO_FILE *alloc_so_file(void)
{
	SO_FILE *so_file = calloc(1, sizeof(SO_FILE));

	if (!so_file)
		return NULL;

	so_file->buffer = calloc(SO_FILE_BUFFER_SIZE, sizeof(unsigned char));
	if (!so_file->buffer) {
		free(so_file);
		return NULL;
	}

	so_file->last_op = READ; // by default

	return so_file;
}

void free_so_file(SO_FILE *so_file)
{
	free(so_file->buffer);
	free(so_file);
}

void open_file(const char *pathname, const char *mode, SO_FILE *so_file)
{
	int fd, flags, permissions;

	permissions = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

	if (!strcmp(mode, "r"))
		flags = O_RDONLY;
	else if (!strcmp(mode, "r+"))
		flags = O_RDWR;

	else if (!strcmp(mode, "w"))
		flags = O_WRONLY | O_CREAT | O_TRUNC;

	else if (!strcmp(mode, "w+"))
		flags = O_RDWR | O_CREAT | O_TRUNC;

	else if (!strcmp(mode, "a"))
		flags = O_RDWR | O_CREAT | O_APPEND;

	else if (!strcmp(mode, "a+"))
		flags = O_RDWR | O_CREAT | O_APPEND;

	so_file->fd = open(pathname, flags, permissions);
}

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *so_file = alloc_so_file();

	if (!so_file)
		return NULL;

	open_file(pathname, mode, so_file);

	if (so_file->fd == -1) {
		// error opening file
		free_so_file(so_file);
		return NULL;
	}

	return so_file;
}

unsigned char get_current_read_char(SO_FILE *stream)
{
	return stream->buffer[stream->read_cursor];
}

int reset_read_buffer_condition(SO_FILE *stream)
{
	int rc = stream->read_cursor;
	int buffer_size = SO_FILE_BUFFER_SIZE;
	int nr_bytes_read = stream->nr_bytes_read;

	if (rc == buffer_size || rc == nr_bytes_read)
		return 1;
	else
		return 0;
}

int so_fgetc(SO_FILE *stream)
{
	int nr_bytes;

	if (stream->last_op == WRITE) {
		memset(stream->buffer, 0, stream->write_cursor);
		stream->write_cursor = 0;
		stream->read_cursor = 0;
		stream->last_op = READ;
	}

	if (stream->read_cursor == 0) {
		nr_bytes = read(stream->fd, stream->buffer, SO_FILE_BUFFER_SIZE);

		if (nr_bytes == 0 || nr_bytes == ERROR) {
			if (nr_bytes < 0)
				stream->error = ERROR;

			stream->eof_status = SO_EOF;
			return SO_EOF;
		}

		stream->nr_bytes_read = nr_bytes;
	}

	unsigned char c = get_current_read_char(stream);

	stream->file_cursor++;
	stream->read_cursor++;
	if (reset_read_buffer_condition(stream)) {
		memset(stream->buffer, 0, stream->read_cursor);
		stream->read_cursor = 0;
		stream->nr_bytes_read = 0;
	}

	return (int) c;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int i;

	for (i = 0; i < nmemb; i++) {
		char *data = calloc(size, sizeof(char));

		if (!data)
			return -1;

		for (int j = 0; j < size; j++) {
			int status = so_fgetc(stream);

			if (status == SO_EOF) {
				free(data);
				return i;
			}

			data[j] = (unsigned char) status;
		}


		memcpy(ptr + i * size, data, size);
		free(data);
	}

	return i;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_fflush(SO_FILE *stream)
{
	int nr_bytes;

	if (stream->last_op == READ)
		return -2;

	nr_bytes = write(stream->fd, stream->buffer, stream->write_cursor);
	memset(stream->buffer, 0, stream->write_cursor);
	stream->write_cursor = 0;

	if (nr_bytes >= 0)
		return 0;
	else
		return -1;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	int result;

	// first flush the buffer (write) or empty it (read)
	if (stream->last_op == WRITE) {
		if (stream->write_cursor != 0) {
			int status = so_fflush(stream);

			if (status == -1)
				stream->error = ERROR;
		}
	} else {
		memset(stream->buffer, 0, stream->read_cursor);
		stream->read_cursor = 0;
	}

	// fseek
	result = lseek(stream->fd, offset, whence);
	if (result >= 0) {
		stream->eof_status = 0;
		stream->file_cursor = result;
		return 0;
	} else {
		return -1;
	}
}

long so_ftell(SO_FILE *stream)
{
	return stream->file_cursor;
}

int so_fputc(int c, SO_FILE *stream)
{
	if (stream->last_op == READ) {
		stream->read_cursor = 0;
		stream->write_cursor = 0;
		stream->last_op = WRITE;
	}

	if (stream->write_cursor == SO_FILE_BUFFER_SIZE) {
		int status = so_fflush(stream);

		if (status == -1) {
			stream->error = ERROR;
			return SO_EOF;
		}
	}

	// write at write_cursor
	stream->buffer[stream->write_cursor] = (unsigned char) c;
	stream->write_cursor++;
	return c;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int i;

	for (i = 0; i < nmemb; i++) {
		unsigned char *c = (char *) ptr + i * size;

		for (int j = 0; j < size; j++) {
			int status = so_fputc((int) c[j], stream);

			if (status == SO_EOF)
				return 0;
		}
	}

	stream->file_cursor += i * size;
	return i;
}

int so_fclose(SO_FILE *stream)
{
	int status;

	status = so_fflush(stream);

	if (status == ERROR) {
		free_so_file(stream);
		return ERROR;
	}

	status = close(stream->fd);
	free_so_file(stream);

	return status;
}

int so_feof(SO_FILE *stream)
{
	return stream->eof_status;
}

int so_ferror(SO_FILE *stream)
{
	return stream->error;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	SO_FILE *so_file = alloc_so_file();

	if (!so_file)
		return NULL;

	int fd[2];
	int result = pipe(fd);

	if (result == -1) {
		free_so_file(so_file);
		return NULL;
	}

	pid_t pid = fork();

	so_file->pid = pid;
	switch (pid) {
	case -1:
		free_so_file(so_file);
		return NULL;
	case 0:
			// child
		if (!strcmp(type, "r")) {
			close(fd[0]);
			dup2(fd[1], STDOUT_FILENO);
		} else if (!strcmp(type, "w")) {
			close(fd[1]);
			dup2(fd[0], STDIN_FILENO);
		}
		execlp("sh", "sh", "-c", command, NULL);
		break;
	default:
		//parent
		if (!strcmp(type, "r")) {
			close(fd[1]);
			so_file->fd = fd[0];
		} else if (!strcmp(type, "w")) {
			close(fd[0]);
			so_file->fd = fd[1];
		}
	}

	return so_file;
}

int so_pclose(SO_FILE *stream)
{
	int result, status;

	so_fflush(stream);
	close(stream->fd);
	result = waitpid(stream->pid, &status, 0);

	if (result == -1)
		status = -1;

	free_so_file(stream);
	return status;
}
