/*
  Copyright 2012, 2015 EditShare LLC
  Copyright 2012, 2015 Ralph Boehme <slow@samba.org>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <err.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdlib.h>

static int verbose = 0;

static char results[9][9];

enum mode {
	mread = 0,
	mwrite,
	mread_write,
	n_modes
};

enum lock {
	lexclusive = 0,
	lshared,
	lnone,
	n_locks
};

enum locktype {
	tsharemode = 0,
	tflock,
	tfcntl,
	n_locktypes
};

int modes[] = {
	[mread]	      = O_RDONLY,
	[mwrite]      = O_WRONLY,
	[mread_write] = O_RDWR
};

int locks[] = {
	[lnone]	      = 0,
	[lshared]     = O_SHLOCK,
	[lexclusive]  = O_EXLOCK
};

int flocks[] = {
	[lnone]	     = 0,
	[lshared]    = LOCK_SH,
	[lexclusive] = LOCK_EX
};

int fcntls[] = {
	[lnone]	     = 0,
	[lshared]    = F_RDLCK,
	[lexclusive] = F_WRLCK
};

char *mode_names[] = {
	[mread]	      = "read only",
	[mwrite]      = "write only",
	[mread_write] = "read/write"
};

char *lock_names[] = {
	[lnone]	      = "no",
	[lshared]     = "shared",
	[lexclusive]  = "exclusive"
};

char *locktype_names[] = {
	[tsharemode]  = "sharemode",
	[tflock]      = "flock",
	[tfcntl]      = "fcntl"
};

char *tableNames[] = {
	"exclusive   R	 ",
	"	     W	 ",
	"	     RW	 ",
	"shared	     R	 ",
	"	     W	 ",
	"	     RW	 ",
	"none	     R	 ",
	"	     W	 ",
	"	     RW	 "
};

static void usage(const char *myname)
{
	printf("usage: %s [-f] PATH1 PATH2\n"
	       "	       -f use ressource fork, default is data fork\n"
	       "	       -v verbose, print error information\n"
	       "	       -vv extra verbose, print all open calls\n"
	       "	       -(s|l|c)(s|l|c)\n"
	       "		  Use sharemode (lock on open), flock, or fcntl,\n"
	       "		  for lock on first and second open respectively,\n"
	       "		  defaulting to sharemode.\n",
	       myname);
	exit(1);
}

bool lock(int fd, enum lock lock, enum locktype locktype)
{
	int result;
	int flags;

	if (lock == lnone) {
		return true;
	}

	switch (locktype) {
	case tsharemode:
		/*
		 * This is done upon open.
		 */
		break;

	case tflock:
		flags = flocks[lock] | LOCK_NB;

		result = flock(fd, flags);
		if (result < 0) {
			if (verbose) {
				printf("flock(%d, %02x)\n", fd, flags);
			}
			return false;
		} else {
			if (verbose > 1) {
				printf("flock(%d, %02x)\n", fd, flags);
			}
		}
		break;

	case tfcntl: {
		struct flock flock_lock = {
			.l_start = 0,
			.l_len = 0,
			.l_whence = SEEK_SET,
			.l_type = fcntls[lock]
		};

		result = fcntl(fd, F_SETLK, &flock_lock);
		if (result < 0) {
			if (verbose) {
				printf("fcntl(%d, F_SETLK, {.l_type = %02x})\n",
				       fd, fcntls[lock]);
			}
			return false;
		} else {
			if (verbose > 1) {
				printf("fcntl(%d, F_SETLK, {.l_type = %02x})\n",
				       fd, fcntls[lock]);
			}
		}
		break;
        }
	default:
		break;
	}

	return true;
}

bool test(char *path1, int flags1, enum lock lock1, enum locktype locktype1,
	  char *path2, int flags2, enum lock lock2, enum locktype locktype2)
{
	int result;
	int fd1, fd2;
	bool ok;
	int pid;
	int status;

	/*
	 * Sharemodes need to be applied in open arguments before
	 * opening, rather than to fd afterwards
	 */

	if (locktype1 == tsharemode) {
		flags1 |= locks[lock1];
	}
	if (locktype2 == tsharemode) {
		flags2 |= locks[lock2];
	}

	fd1 = open(path1, flags1 | O_NONBLOCK);
	if (fd1 < 0) {
		if (verbose) {
			printf("outer open(%s, %02x)\n", path1, flags1);
		}
		return false;
	} else {
		if (verbose > 1) {
			printf("outer open(%s, %02x)\n", path1, flags1);
		}
	}

	ok = lock(fd1, lock1, locktype1);
	if (!ok) {
		return false;
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(1);
	}

	if (pid == 0) {
		fd2 = open(path2, flags2 | O_NONBLOCK);
		if (fd2 < 0) {
			if (verbose) {
				printf("inner open(%s, %02x)\n",
				       path2, flags2);
			}
			exit(1);
		} else {
			if (verbose > 1) {
				printf("inner open(%s, %02x)\n",
				       path2, flags2);
			}
		}
		ok = lock(fd2, lock2, locktype2);
		if (!ok) {
			exit(1);
		}
		exit(0);
	}

	waitpid(pid, &status, 0);
	result = close(fd1);
	if (result != 0) {
		perror("close");
		exit(1);
	}

	if (WIFEXITED(status)) {
		return !WEXITSTATUS(status);
	} else {
		return false;
	}
}

int main(int argc, char **argv) {
	bool resource = false;
	char *path1, *path2, *myname = argv[0];
	bool ok;
	int result;
	char ch;
	char str1[36], str2[36];
	int nlockargs = 0;
	enum locktype locktypes[2] = {tsharemode, tsharemode};
	char *p;

	while ((ch = getopt(argc, argv, "fvslc")) != -1) {
		switch (ch) {
		case 'f':
			resource = true;
			break;

		case 'v':
			++verbose;
			break;

		case 's':
		case 'l':
		case 'c':
			if (nlockargs >= 2) {
				usage(myname);
			}

			switch (ch) {
			case 's':
				break;
			case 'l':
				locktypes[nlockargs] = tflock;
				break;
			case 'c':
				locktypes[nlockargs] = tfcntl;
				break;
			default:
				usage(myname);
			}
			++nlockargs;
			break;

		case '?':
		case 'h':
		default:
			usage(myname);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2) {
		usage(myname);
	}

	if (resource) {
		result = asprintf(&path1, "%s/..namedfork/rsrc", argv[0]);
		if (result == -1) {
			printf("asprintf %s\n", argv[0]);
			exit(1);
		}

		result = asprintf(&path2, "%s/..namedfork/rsrc", argv[1]);
		if (result == -1) {
			printf("asprintf %s\n", argv[1]);
			exit(1);
		}
	} else {
		path1 = argv[0];
		path2 = argv[1];
	}

	if (verbose) {
		printf("       %-32s %-32s\n", path1, path2);
	}

	p = &results[0][0];
	for (enum lock lock1 = 0; lock1 < n_locks; ++lock1) {
		for (enum mode mode1 = 0; mode1 < n_modes; ++mode1) {
			for (enum lock lock2 = 0; lock2 < n_locks; ++lock2) {
				for (enum mode mode2 = 0; mode2 < n_modes; ++mode2) {
					ok = test(path1, modes[mode1], lock1, locktypes[0],
						  path2, modes[mode2], lock2, locktypes[1]);
					if (verbose) {
						snprintf(str1, sizeof(str1), "%s with %s %s",
							 mode_names[mode1],
							 lock_names[lock1],
							 locktype_names[locktypes[0]]);
						snprintf(str2, sizeof(str2), "%s with %s %s",
							 mode_names[mode2],
							 lock_names[lock2],
							 locktype_names[locktypes[1]]);
						printf("%s : %-36s %-36s \n",
						       ok ? " ok " : "fail",
						       str1, str2);
					}
					*p++ = ok ? '.' : 'x';
				}
			}
		}
	}

	printf("	   \\	 %-10s\n"
	       "%-10s  \\   Attempted mode\n"
	       "Current mode \\	 exclusive   | shared	   | none\n"
	       "		R   W  RW   | R	  W  RW	  | R	W  RW\n",
	       locktype_names[locktypes[1]],
	       locktype_names[locktypes[0]]);

	for (int i = 0; i < 9; i++) {
		if (i == 3 || i == 6) {
			printf("----------\n");
		}

		printf("%s", tableNames[i]);

		for (int j = 0; j < 9; j++) {
			printf("%s%c   ",
			       (j == 3 || j == 6) ? "  " : "",
			       results[i][j]);
		}
		printf("\n");
	}

	return 0;
}
