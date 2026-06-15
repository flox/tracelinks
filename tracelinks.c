/*
 * tracelinks - report on symbolic links encountered in path traversals.
 *
 * Given one or more paths, tracelinks walks each path one component at a
 * time, following any symbolic links it meets and printing the chain of
 * links as it goes. Dangling and circular links are detected and reported
 * as errors. The final, fully resolved component is reported along with its
 * file type (and, with -f, the filesystem it lives on).
 *
 * See README.md and tracelinks(1) for usage and examples.
 */

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
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include <mntent.h>
#include <sys/statfs.h>
#endif

static int debug_flag = 0;
static int absolute_flag = 0;
static int print_fstype_flag = 0;
static int keep_going_flag = 0;

/*
 * Program name used in usage and diagnostics. Set from argv[0] in main() so it
 * matches the name err(3)/warn(3) prepend; "tracelinks" is only a fallback.
 * (getprogname() would be simpler but is a BSD extension absent from glibc.)
 */
static const char *progname = "tracelinks";

/* Maximum expected number of recursive calls (not symlinks). */
#define MAXITERATIONS 1024
static struct {
  char *root;
  char *path;
} seenTuples[MAXITERATIONS];
static int seenCount;    /* number of (root,path) tuples currently recorded */
static int seenDisabled; /* set once a traversal exceeds MAXITERATIONS */

/* Forget the history recorded for the current traversal, freeing its copies. */
static void reset_seen(void) {
  for (int i = 0; i < seenCount; i++) {
    free(seenTuples[i].root);
    free(seenTuples[i].path);
  }
  seenCount = 0;
  seenDisabled = 0;
}

static void usage(FILE *stream) {
  fprintf(stream, "Usage: %s [OPTION]... PATH [PATH]...\n", progname);
  fprintf(stream,
          "Report on symbolic links encountered in path traversals.\n\n");
  fprintf(stream, "  -a, --absolute    report paths as absolute paths\n");
  fprintf(stream,
          "  -k, --keep-going  keep reporting on other paths after an error\n");
  fprintf(stream,
          "  -f, --fstype      print filesystem type for each path reported\n");
  fprintf(stream, "  -h, --help        print this help message\n");
  fprintf(stream, "  -d, --debug       print extra debugging to STDERR\n");
  fprintf(stream, "  -v, --version     print version string\n");
}

/* Print a brief "try --help" hint to stderr and exit unsuccessfully. */
static void try_help(void) {
  fprintf(stderr, "Try '%s --help' for more information.\n", progname);
  exit(EXIT_FAILURE);
}

/* Report a usage error to stderr and exit unsuccessfully. */
static void error(const char *format, ...) {
  va_list argp;
  va_start(argp, format);
  vwarnx(format, argp);
  va_end(argp);
  try_help();
}

/*
 * Loop check: verify that we haven't seen this (root,path) tuple before.
 *
 * The tuples are duplicated with strdup() so that the history owns its strings
 * and does not depend on the lifetime of the caller's stack buffers. They are
 * released by reset_seen() between traversals.
 */
static int loopcheck(const char *root, const char *path) {
  if (debug_flag)
    fprintf(stderr, "loopcheck('%s', '%s')\n", root, path);

  if (seenDisabled)
    return (EXIT_SUCCESS);
  if (seenCount >= MAXITERATIONS) {
    warnx("encountered maximum path traversals (%d),"
          " disabling loop detection",
          MAXITERATIONS);
    seenDisabled = 1;
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

  char *rootdup = strdup(root);
  char *pathdup = strdup(path);
  if (rootdup == NULL || pathdup == NULL) {
    free(rootdup);
    free(pathdup);
    warnx("out of memory, disabling loop detection");
    seenDisabled = 1;
    return (EXIT_SUCCESS);
  }
  seenTuples[seenCount].root = rootdup;
  seenTuples[seenCount++].path = pathdup;
  return (EXIT_SUCCESS);
}

/*
 * Print " (fstype)" for the filesystem backing path, or " (unknown)" if it
 * cannot be determined. This is a best-effort annotation and never fails the
 * traversal.
 */
static void print_filesystem_type(const char *path) {
#ifdef __linux__
  /*
   * /proc/mounts lists every mount point; the filesystem backing path is the
   * one whose mount point is the longest prefix of path on a component
   * boundary. getmntent() reuses its returned struct, so copy the type out.
   */
  FILE *mntFile = setmntent("/proc/mounts", "r");
  if (mntFile != NULL) {
    struct mntent *mnt;
    char best_type[64] = "";
    size_t best_len = 0;
    while ((mnt = getmntent(mntFile)) != NULL) {
      size_t len = strlen(mnt->mnt_dir);
      if (len < best_len || strncmp(path, mnt->mnt_dir, len) != 0)
        continue;
      /*
       * path matches mnt_dir for its full length, so path is at least len
       * bytes long and path[len] is a valid read. Require the match to land on
       * a component boundary so that e.g. "/foobar" does not match "/foo".
       */
      if (path[len] == '\0' || path[len] == '/' ||
          mnt->mnt_dir[len - 1] == '/') {
        best_len = len;
        snprintf(best_type, sizeof(best_type), "%s", mnt->mnt_type);
      }
    }
    endmntent(mntFile);
    if (best_len > 0) {
      printf(" (%s)", best_type);
      return;
    }
  }
  printf(" (unknown)");
#elif __APPLE__
  struct statfs fsinfo;
  if (statfs(path, &fsinfo) == 0)
    printf(" (%s)", fsinfo.f_fstypename);
  else
    printf(" (unknown)");
#else
  (void)path;
  printf(" (unknown)");
#endif
}

/* Print 2*depth spaces of indentation to show the depth of a link chain. */
static void print_indent(int depth) { printf("%*s", depth * 2, ""); }

/*
 * Walk one path component at a time, following symlinks. path is written to in
 * place (each '/' is temporarily replaced with a NUL while that component is
 * examined), so it is non-const; callers always pass a writable buffer. root is
 * only ever read.
 */
static int tracelinks(int indent, const char *root, char *path) {
  struct stat sb;
  char pathbuf[PATH_MAX];
  char *pathend = pathbuf;
  memset(pathbuf, 0, sizeof(pathbuf));

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
        if (getcwd(pathbuf, sizeof(pathbuf)) == NULL) {
          perror("getcwd()");
          return (EXIT_FAILURE);
        }
        // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83404
        strncat(pathbuf, "/", sizeof(pathbuf) - strlen(pathbuf) - 1);
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

  /* Leave room for the NUL and a possible trailing "/" appended below. */
  if (strlen(root) + strlen(path) + 2 > sizeof(pathbuf)) {
    warnx("path too long: %s%s", root, path);
    return (EXIT_FAILURE);
  }
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
    memset(linkbuf, 0, sizeof(linkbuf));

    /* Reserve a byte so linkbuf is always NUL-terminated. */
    ssize_t nbytes = readlink(pathbuf, linkbuf, sizeof(linkbuf) - 1);
    if (nbytes == -1) {
      perror(pathbuf);
      return (EXIT_FAILURE);
    }

    print_indent(indent++);
    printf("%s -> %.*s", pathbuf, (int)nbytes, linkbuf);

    if (print_fstype_flag)
      print_filesystem_type(pathbuf);
    printf("\n");

    /* Re-append the portion of path that followed this link, if any. */
    if (d) {
      *d = '/';
      if ((size_t)nbytes + strlen(d) + 1 > sizeof(linkbuf)) {
        warnx("path too long: %s%s", linkbuf, d);
        return (EXIT_FAILURE);
      }
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
      printf("%s: block device", pathbuf);
      break;
    case S_IFCHR:
      printf("%s: character device", pathbuf);
      break;
    case S_IFDIR:
      printf("%s: directory", pathbuf);
      break;
    case S_IFIFO:
      printf("%s: FIFO/pipe", pathbuf);
      break;
    case S_IFREG:
      printf("%s: regular file", pathbuf);
      break;
    case S_IFSOCK:
      printf("%s: socket", pathbuf);
      break;
    default:
      printf("%s: unknown?", pathbuf);
    }
    if (print_fstype_flag)
      print_filesystem_type(pathbuf);
    printf("\n");
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

  /* Report ourselves by the basename we were invoked as. */
  if (argv[0] != NULL && *argv[0] != '\0') {
    const char *slash = strrchr(argv[0], '/');
    progname = slash != NULL ? slash + 1 : argv[0];
  }

  while (1) {
    static struct option long_options[] = {{"absolute", no_argument, 0, 'a'},
                                           {"fstype", no_argument, 0, 'f'},
                                           {"keep-going", no_argument, 0, 'k'},
                                           {"version", no_argument, 0, 'v'},
                                           {"debug", no_argument, 0, 'd'},
                                           {"help", no_argument, 0, 'h'},
                                           {0, 0, 0, 0}};

    /* getopt_long stores the option index here. */
    int option_index = 0;

    c = getopt_long(argc, argv, "afkvdh", long_options, &option_index);

    /* Detect the end of the options. */
    if (c == -1)
      break;

    switch (c) {
    case 'a':
      absolute_flag = 1;
      break;

    case 'f':
      print_fstype_flag = 1;
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
      usage(stdout);
      exit(EXIT_SUCCESS);

    case '?':
      /* getopt_long already printed an error message. */
      try_help();

    default:
      error("?? getopt returned character code 0%o ??", c);
    }
  }

  if (optind == argc)
    error("no paths provided");

  while (optind < argc) {
    reset_seen(); /* start each traversal with an empty, freed history */
    int rc = tracelinks(0, "", argv[optind++]);
    maxrc = MAX(rc, maxrc);
    /*
     * keep_going_flag only governs whether we continue on to the next
     * top-level PATH after a failure; a failure within a single traversal
     * always stops that traversal.
     */
    if (rc > 0 && keep_going_flag == 0)
      break;
    if ((argc - optind) > 0)
      printf("\n");
  }
  reset_seen(); /* free the final traversal's history */

  exit(maxrc);
}
