/*
 * Include <TargetConditionals.h> to address error:
 *   'TARGET_OS_IPHONE' is not defined
 * https://developer.apple.com/documentation/xcode/identifying-and-addressing-framework-module-issues
 */
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include <err.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int debug_flag = 0;
static int absolute_flag = 0;
static int keep_going_flag = 0;

/* Maximum expected number of recursive calls (not symlinks). */
#define MAXITERATIONS 1024
static struct {
  const char *root;
  const char *path;
} seenTuples[MAXITERATIONS];
static int seenCount;

static void usage(const int rc) {
  printf("Usage: tracelinks [OPTION] PATH [PATH]...\n");
  printf("Report on symbolic links encountered in path traversals.\n\n");
  printf("  -a, --absolute    report paths as absolute paths\n");
  printf("  -k, --keep-going  keep reporting on other paths after an error\n");
  printf("  -h, --help        print this help message\n");
  printf("  -d, --debug       print extra debugging to STDERR\n");
  printf("  -v, --version     print version string\n");
  exit(rc);
}

static void error(const char *format, ...) {
  va_list argp;
  va_start(argp, format);
  vwarnx(format, argp);
  va_end(argp);
  usage(EXIT_FAILURE);
}

/*
 * Loop check: verify that we haven't seen this (root,path) tuple before.
 */
static int loopcheck(const char *root, const char *path) {
  if (debug_flag)
    fprintf(stderr, "loopcheck('%s', '%s')\n", root, path);

  if (seenCount < 0)
    return (EXIT_SUCCESS);
  if (seenCount >= MAXITERATIONS) {
    warnx("encountered maximum path traversals (%d),"
          " disabling loop detection",
          MAXITERATIONS);
    seenCount = -1;
    return (EXIT_SUCCESS);
  }

  for (int i = 0; i < seenCount; i++)
    if ((strcmp(root, seenTuples[i].root) == 0) &&
        (strcmp(path, seenTuples[i].path) == 0)) {
      fprintf(stderr, "ERROR: loop detected in path: ");
      for (int j = i; j < seenCount; j++)
        fprintf(stderr, "%s%s -> ", seenTuples[j].root, seenTuples[j].path);
      fprintf(stderr, "%s%s\n", root, path);
      return (EXIT_FAILURE);
    }
  seenTuples[seenCount].root = root;
  seenTuples[seenCount++].path = path;
  return (EXIT_SUCCESS);
}

#define print_indent(x) printf("%*s", x * 2, "")

static int tracelinks(int indent, const char *root, const char *path) {
  struct stat sb;
  char pathbuf[PATH_MAX];
  char *pathend = pathbuf;
  memset(pathbuf, 0, PATH_MAX);

  if (debug_flag)
    fprintf(stderr, "tracelinks('%s', '%s')\n", root, path);

  int rc = loopcheck(root, path);
  if (rc)
    return (rc);

  if (strlen(root) == 0) {
    if (*path == '/') {
      /* absolute path */
      return (tracelinks(indent, "/", (path + 1)));
    } else {
      /* relative path */
      if (absolute_flag) {
        /* Report paths as absolute */
        if (getcwd(pathbuf, PATH_MAX) == NULL) {
          perror("getcwd()");
          return (EXIT_FAILURE);
        }
        strncat(pathbuf, "/", 1);
        return (tracelinks(indent, pathbuf, path));
      } else {
        /* Report paths relative to "." */
        return (tracelinks(indent, "./", path));
      }
    }
  }

  /* Recursively step through path stat()ing as we go. */
  char *d = strchr(path, '/');
  if (d != NULL)
    *d = '\0';
  pathend = stpcpy(pathend, root);
  pathend = stpcpy(pathend, path);

  if (debug_flag)
    fprintf(stderr, "lstat('%s')\n", pathbuf);
  if (lstat(pathbuf, &sb) == -1) {
    perror(pathbuf);
    return (EXIT_FAILURE);
  }

  if (S_ISLNK(sb.st_mode)) {
    char linkbuf[PATH_MAX];
    memset(linkbuf, 0, PATH_MAX);

    ssize_t nbytes = readlink(pathbuf, linkbuf, PATH_MAX);
    if (nbytes == -1) {
      perror(pathbuf);
      return (EXIT_FAILURE);
    }

    print_indent(indent++);
    printf("%s -> %.*s\n", pathbuf, (int)nbytes, linkbuf);

    if (d) {
      *d = '/';
      stpcpy(linkbuf + nbytes, d);
    }
    if (*linkbuf == '/')
      /* Absolute link, reset root to "" and replace path */
      return (tracelinks(indent, "", linkbuf));
    else
      /* Relative link, replace path only */
      return (tracelinks(indent, root, linkbuf));
  } else {
    /*
     * Recurse if this is a directory and there are further
     * directories remaining, otherwise just report it as
     * a directory.
     */
    if (d && S_ISDIR(sb.st_mode)) {
      stpcpy(pathend, "/");
      return (tracelinks(indent, pathbuf, d + 1));
    }
    print_indent(indent++);
    switch (sb.st_mode & S_IFMT) {
    case S_IFBLK:
      printf("%s: block device\n", pathbuf);
      break;
    case S_IFCHR:
      printf("%s: character device\n", pathbuf);
      break;
    case S_IFDIR:
      printf("%s: directory\n", pathbuf);
      break;
    case S_IFIFO:
      printf("%s: FIFO/pipe\n", pathbuf);
      break;
    case S_IFREG:
      printf("%s: regular file\n", pathbuf);
      break;
    case S_IFSOCK:
      printf("%s: socket\n", pathbuf);
      break;
    default:
      printf("%s: unknown?\n", pathbuf);
    }
    if (d) {
      warnx("extra trailing characters: %s", d + 1);
      return (EXIT_FAILURE);
    }
  }
  return (EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  int c;
  int maxrc = 0;

  while (1) {
    static struct option long_options[] = {
        {"absolute", no_argument, 0, 'a'}, {"keep-going", no_argument, 0, 'k'},
        {"version", no_argument, 0, 'v'},  {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},     {0, 0, 0, 0}};

    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long(argc, argv, "akvdh", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case 'a':
      absolute_flag = 1;
      break;

    case 'k':
      keep_going_flag = 1;
      break;

    case 'v':
      puts(VERSION);
      exit(EXIT_SUCCESS);

    case 'd':
      debug_flag = 1;
      break;

    case 'h':
      usage(EXIT_SUCCESS);

    case '?':
      /* getopt_long already printed an error message. */
      usage(EXIT_FAILURE);

    default:
      error("?? getopt returned character code 0%o ??", c);
    }
  }

  if (optind == argc)
    error("no paths provided\n");

  while (optind < argc) {
    seenCount = 0; /* reset with each new path traversal */
    int rc = tracelinks(0, "", argv[optind++]);
    if (rc > 0 && keep_going_flag == 0)
      exit(rc);
    if ((argc - optind) > 0)
      printf("\n");
    maxrc = MAX(rc, maxrc);
  }

  exit(maxrc);
}
