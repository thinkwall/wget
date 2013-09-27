/*
 * Copyright(c) 2012 Tim Ruehsen
 *
 * This file is part of libmget.
 *
 * Libmget is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Libmget is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libmget.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * a collection of i/o routines
 *
 * Changelog
 * 25.04.2012  Tim Ruehsen  created
 *
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>
//#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef WIN32
#	include <winsock2.h>
#elif defined(HAVE_POLL_H)
#	include <sys/poll.h>
#else
#	include <sys/select.h>
#endif

#include <libmget.h>
#include "private.h"

// similar to getline(), but:
// - using a file descriptor
// - returns line without trailing \n
// *buf holds size_t[2] at it's end'
//
// casts like '(size_t *)(void *)' are to silence clang

ssize_t mget_fdgetline(char **buf, size_t *bufsize, int fd)
{
	ssize_t nbytes = 0;
	size_t *sizep, length = 0;
	char *p;

	if (!*buf || !*bufsize) {
		// first call
		*buf = malloc(*bufsize = 10240);
		sizep = (size_t *)(void *)(*buf + *bufsize - 2 * sizeof(size_t));
		sizep[0] = sizep[1] = 0;
	} else {
		sizep = (size_t *)(void*)(*buf + *bufsize - 2 * sizeof(size_t));
		if (sizep[1]) {
			// take care of remaining data from last call
			if ((p = memchr(*buf + sizep[0], '\n', sizep[1]))) {
				*p++ = 0;
				length = p - (*buf + sizep[0]);
				if (sizep[0])
					memmove(*buf, *buf + sizep[0], length); // copy line to beginning of buffer
				sizep[0] += length; // position of extra chars
				sizep[1] -= length; // number of extra chars
				return length - 1; // length of line in *buf
			}

			length = sizep[1];
			memmove(*buf, *buf + sizep[0], length + 1);
			sizep[0] = sizep[1] = 0;
		} else **buf = 0;
	}

	while ((nbytes = read(fd, *buf + length, *bufsize - 2 * sizeof(size_t) - length - 1)) > 0) {
		length += nbytes;
		if ((p = memchr(*buf + length - nbytes, '\n', nbytes))) {
			*p++ = 0;
			sizep[0] = p - *buf; // position of extra chars
			sizep[1] = length - sizep[0]; // number of extra chars
			return sizep[0] - 1; // length of line in *buf
		}

		if (length >= *bufsize - 2 * sizeof(size_t) - 1) {
			ptrdiff_t off = ((char *)sizep)-*buf;
			size_t *old;

			*buf = xrealloc(*buf, *bufsize = *bufsize * 2);
			old = (size_t *)(void *)(*buf + off);
			sizep = (size_t *)(void *)(*buf + *bufsize - 2 * sizeof(size_t));
			sizep[0] = old[0];
			sizep[1] = old[1];
		}
	}

	if (nbytes == -1 && errno != EAGAIN) {
		// file/socket descriptor is broken
		// if (errno != EBADF)
		// 	error_printf(_("%s: Failed to read, error %d\n"), __func__, errno);
	}

	if (length) {
		if ((*buf)[length - 1] == '\n')
			(*buf)[length - 1] = 0;
		else
			(*buf)[length] = 0;
		return length;
	} else **buf = 0;

	return -1;
}

ssize_t mget_getline(char **buf, size_t *bufsize, FILE *fp)
{
	ssize_t nbytes = 0;
	size_t *sizep, length = 0;
	char *p;

	if (!*buf || !*bufsize) {
		// first call
		*buf = xmalloc(*bufsize = 10240);
		sizep = (size_t *)(void *)(*buf + *bufsize - 2 * sizeof(size_t));
		sizep[0] = sizep[1] = 0;
	} else {
		sizep = (size_t *)(void *)(*buf + *bufsize - 2 * sizeof(size_t));
		if (sizep[1]) {
			// take care of remaining data from last call
			if ((p = memchr(*buf + sizep[0], '\n', sizep[1]))) {
				*p++ = 0;
				length = p - (*buf + sizep[0]);
				if (sizep[0])
					memmove(*buf, *buf + sizep[0], length); // copy line to beginning of buffer
				sizep[0] += length; // position of extra chars
				sizep[1] -= length; // number of extra chars
				return length - 1; // length of line in *buf
			}

			length = sizep[1];
			memmove(*buf, *buf + sizep[0], length + 1);
			sizep[0] = sizep[1] = 0;
		} else **buf = 0;
	}

	while ((nbytes = fread(*buf + length, 1, *bufsize - 2 * sizeof(size_t) - length - 1, fp)) > 0) {
		length += nbytes;
		if ((p = memchr(*buf + length - nbytes, '\n', nbytes))) {
			*p++ = 0;
			sizep[0] = p - *buf; // position of extra chars
			sizep[1] = length - sizep[0]; // number of extra chars
			return sizep[0] - 1; // length of line in *buf
		}

		if (length >= *bufsize - 2 * sizeof(size_t) - 1) {
			ptrdiff_t off = ((char *)sizep)-*buf;
			size_t *old;

			*buf = xrealloc(*buf, *bufsize = *bufsize * 2);
			old = (size_t *)(void *)(*buf + off);
			sizep = (size_t *)(void *)(*buf + *bufsize - 2 * sizeof(size_t));
			sizep[0] = old[0];
			sizep[1] = old[1];
		}
	}

	if (nbytes == -1 && errno != EAGAIN) {
		// socket is broken
		// if (errno != EBADF)
		//	error_printf(_("%s: Failed to read, error %d\n"), __func__, errno);
	}

	if (length) {
		if ((*buf)[length - 1] == '\n')
			(*buf)[length - 1] = 0;
		else
			(*buf)[length] = 0;
		return length;
	} else **buf = 0;

	return -1;
}

#ifdef POLLIN
static int _ready_2_transfer(int fd, int timeout, int mode)
{
	// 0: no timeout / immediate
	// -1: INFINITE timeout
	// >0: number of milliseconds to wait
	if (timeout) {
		int rc;

		if (mode == MGET_IO_READABLE)
			mode = POLLIN;
		else
			mode = POLLOUT;

		// wait for socket to be ready to read
		struct pollfd pollfd[1] = {
			{ fd, mode, 0}};

		if ((rc = poll(pollfd, 1, timeout)) <= 0)
			return rc < 0 ? -1 : 0;

		if (!(pollfd[0].revents & mode))
			return -1;
	}

	return 1;
}
#else
static int _ready_2_transfer(int fd, int timeout, int mode)
{
	// 0: no timeout / immediate
	// -1: INFINITE timeout
	// >0: number of milliseconds to wait
	if (timeout) {
		fd_set fdset;
		struct timeval tmo = { timeout / 1000, (timeout % 1000) * 1000 };
		int rc;

		FD_ZERO(&fdset);
		FD_SET(fd, &fdset);

		if (mode == MGET_IO_READABLE) {
			rc = select(fd + 1, &fdset, NULL, NULL, &tmo);
		} else {
			rc = select(fd + 1, NULL, &fdset, NULL, &tmo);
		}

		if (rc <= 0)
			return rc;
	}

	return 1;
}
#endif

int mget_ready_2_read(int fd, int timeout)
{
	return _ready_2_transfer(fd, timeout, MGET_IO_READABLE);
}

int mget_ready_2_write(int fd, int timeout)
{
	return _ready_2_transfer(fd, timeout, MGET_IO_WRITABLE);
}

char *mget_read_file(const char *fname, size_t *size)
{
	int fd;
	ssize_t nread;
	char *buf = NULL;

	if (strcmp(fname,"-")) {
		if ((fd = open(fname, O_RDONLY)) != -1) {
			struct stat st;

			if (fstat(fd, &st) == 0) {
				off_t total = 0;

				buf = xmalloc(st.st_size + 1);

				while (total < st.st_size && (nread = read(fd, buf + total, st.st_size - total)) > 0) {
					total += nread;
				}
				buf[total] = 0;

				if (size)
					*size = total;

				if (total != st.st_size)
					error_printf(_("WARNING: Size of %s changed from %zd to %zd while reading. This may lead to unwanted results !\n"), fname, st.st_size, total);
			} else
				error_printf(_("Failed to fstat %s\n"), fname);

			close(fd);
		} else
			error_printf(_("Failed to open %s\n"), fname);
	} else {
		// read data from STDIN.
		char tmp[4096];
		mget_buffer_t buffer;
		
		mget_buffer_init(&buffer, NULL, 4096);

		while ((nread = read(STDIN_FILENO, tmp, sizeof(tmp))) > 0) {
			mget_buffer_memcat(&buffer, tmp, nread);
		}

		if (size)
			*size = buffer.length;

		buf = buffer.data;
		buffer.data = NULL;

		mget_buffer_deinit(&buffer);
	}

	return buf;
}
