#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <limits.h>

static int debug_flag = 0;
static int absolute_flag = 0;
static int keep_going_flag = 0;

#define MAXITERATIONS 1024	// arbitrary limit
static struct { char *root; char *path; } seenTuples[MAXITERATIONS];
static int seenCount;

static void
usage()
{
	printf("Usage: tracelinks [OPTION] PATH [PATH]...\n");
	printf("Report on symbolic links encountered in path traversals.\n\n");
	printf("  -a, --absolute    report paths as absolute paths\n");
	printf("  -k, --keep-going  keep reporting on other paths after an error\n");
	printf("  -h, --help        print this help message\n");
	printf("  -d, --debug       print extra debugging to STDERR\n");
	printf("  -v, --version     print version string\n");
}

/*
 * Loop check: verify that we haven't seen this (root,path) tuple before.
 */
int
loopcheck (char *root, char *path)
{
	if (debug_flag)
		fprintf(stderr, "loopcheck('%s', '%s')\n", root, path);

	if (seenCount < 0) return(0);
	if (seenCount >= MAXITERATIONS) {
		fprintf(stderr, "WARNING: encountered maximum path traversals (%d),"
			" disabling loop detection\n", MAXITERATIONS);
		seenCount = -1;
		return(0);
	}

	for (int i=0; i<seenCount; i++)
		if ((strcmp(root, seenTuples[i].root) == 0) &&
		    (strcmp(path, seenTuples[i].path) == 0)) {
			fprintf(stderr, "ERROR: loop detected in path: ");
			for (int j=i; j<seenCount; j++)
				fprintf(stderr, "%s%s -> ", seenTuples[j].root, seenTuples[j].path);
			fprintf(stderr, "%s%s\n", root, path);
			return(EXIT_FAILURE);
		}
	seenTuples[seenCount].root = root;
	seenTuples[seenCount++].path = path;
	return(0);
}

#define print_indent(x) printf("%*s", x*2, "")

int
tracelinks (int indent, char *root, char *path)
{
	struct stat sb;
	char *buf, *pathbuf;
	ssize_t nbytes;

	if (debug_flag)
		fprintf(stderr, "tracelinks('%s', '%s')\n", root, path);

	int rc = loopcheck(root, path);
	if (rc)
		return(rc);

	pathbuf = malloc(PATH_MAX);
	if (pathbuf == NULL) {
		perror("malloc()");
		return(EXIT_FAILURE);
	}
	bzero(pathbuf, PATH_MAX);

	if (strlen(root) == 0) {
		if (*path == '/') {
			/* absolute path */
			return(tracelinks(indent, "/", (path+1)));
		} else {
			/* relative path */
			if (absolute_flag) {
				/* Report paths as absolute */
				if (getcwd(pathbuf, PATH_MAX) == NULL) {
					perror("getcwd()");
					return(EXIT_FAILURE);
				}
				strcat(pathbuf, "/");
				return(tracelinks(indent, pathbuf, path));
			} else
				/* Report paths relative to "." */
				return(tracelinks(indent, "./", path));
		}
	}

	/* Recursively step through path stat()ing as we go. */
	char *d = strchr(path, '/');
	if (d != NULL)
		*d = '\0';
	strcat(pathbuf, root);
	strcat(pathbuf, path);

	if (debug_flag)
		fprintf(stderr, "lstat('%s')\n", pathbuf);
	if (lstat(pathbuf, &sb) == -1) {
		perror(pathbuf);
		return(EXIT_FAILURE);
	}

	if ((sb.st_mode & S_IFMT) == S_IFLNK) {		// symlink
		buf = malloc(PATH_MAX);
		if (buf == NULL) {
			perror("malloc()");
			return(EXIT_FAILURE);
		}
		bzero(buf, PATH_MAX);

		nbytes = readlink(pathbuf, buf, PATH_MAX);
		if (nbytes == -1) {
			perror(pathbuf);
			return(EXIT_FAILURE);
		}

		print_indent(indent++);
		printf("%s -> %.*s\n", pathbuf, (int) nbytes, buf);

		if (d) {
			strcat(buf, "/");
			strcat(buf, d+1);
		}
		if (*buf == '/') {
			/* Absolute link, reset root to "" and replace path */
			return(tracelinks(indent, "", buf));
		} else {
			/* Relative link, replace path only */
			return(tracelinks(indent, root, buf));
		}

		free(buf);
	}
	else {
		/*
		 * Recurse if this is a directory and there are further
		 * directories remaining, otherwise just report it as
		 * a directory.
		 */
		if (d && (sb.st_mode & S_IFMT) == S_IFDIR) {
			strcat(pathbuf, "/");
			return(tracelinks(indent, pathbuf,d+1));
		}
		print_indent(indent++);
		switch (sb.st_mode & S_IFMT) {
			case S_IFBLK:  printf("%s: block device\n", pathbuf); break;
			case S_IFCHR:  printf("%s: character device\n", pathbuf); break;
			case S_IFDIR:  printf("%s: directory\n", pathbuf); break;
			case S_IFIFO:  printf("%s: FIFO/pipe\n", pathbuf); break;
			case S_IFREG:  printf("%s: regular file\n", pathbuf); break;
			case S_IFSOCK: printf("%s: socket\n", pathbuf); break;
			default: printf("%s: unknown?\n", pathbuf);
		}
	}
	free(pathbuf);
	return(0);
}

int
main (int argc, char **argv)
{
	int c;
	int maxrc = 0;

	while (1) {
		static struct option long_options[] = {
			{"absolute",	no_argument, 0, 'a'},
			{"keep-going",	no_argument, 0, 'k'},
			{"version",	no_argument, 0, 'v'},
			{"debug",	no_argument, 0, 'd'},
			{"help",	no_argument, 0, 'h'},
			{0, 0, 0, 0}
		};

		/* getopt_long stores the option index here. */
		int option_index = 0;

		c = getopt_long (argc, argv, "akvdh", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		switch (c) {
			case 0:
				/* If this option set a flag, do nothing else now. */
				if (long_options[option_index].flag != 0)
					break;
				printf ("option %s", long_options[option_index].name);
				if (optarg)
					printf (" with arg %s", optarg);
				printf ("\n");
				break;

			case 'a':
				absolute_flag = 1;
				break;

			case 'k':
				keep_going_flag = 1;
				break;

			case 'v':
				puts(VERSION);
				exit(0);

			case 'd':
				debug_flag = 1;
				break;

			case 'h':
				usage();
				exit(0);

			case '?':
				/* getopt_long already printed an error message. */
				break;

			default:
				abort ();
		}
	}

	if (optind < argc) {
		while (optind < argc) {
			seenCount = 0;	/* reset with each new path traversal */
			int rc = tracelinks(0, "", argv[optind++]);
			if (rc > 0 && keep_going_flag == 0)
				exit(rc);
			if ((argc - optind) > 0) printf("\n");
			maxrc = MAX(rc, maxrc);
		}
	}

	exit(maxrc);
}
