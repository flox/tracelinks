# tracelinks

[![C/C++ CI](https://github.com/flox/tracelinks/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/flox/tracelinks/actions/workflows/c-cpp.yml)

> Follow a path one component at a time and report every symbolic link along the way.

`tracelinks` is a small, dependency-free C utility that resolves a path the way
the kernel does — component by component — and prints each symbolic link it
crosses, the target the link points at, and the file type of the final
resolved object. Unlike `realpath`, which only prints the destination, or
`ls -l`, which only shows one link at a time, `tracelinks` shows you the
**whole chain** and flags the things that go wrong: dangling links, circular
links, and missing components.

It is developed and maintained by [Flox](https://flox.dev) and ships as part of
the Flox CLI installer. Flox environments are assembled from symlinks into the Nix store
and synced through [FloxHub](https://hub.flox.dev), so understanding how a given
link resolves — and catching a broken or looping one — is a routine part of
working with and debugging them.

## Features

- **Traces the full link chain**, indenting each hop so nested links are easy to follow.
- **Detects dangling links** (a link whose target does not exist) and exits nonzero.
- **Detects circular links** and prints the cycle it found.
- **Reports the file type** of the final target (regular file, directory, device, FIFO, socket, …).
- **`--fstype`** annotates each reported path with the filesystem backing it.
- **`--absolute`** reports resolved paths as absolute paths.
- **`--keep-going`** continues to the next path after an error instead of stopping.
- **Cross-platform:** builds and runs on Linux and macOS with no third-party libraries.

## Installation

### With Flox

`tracelinks` is bundled with the [Flox](https://flox.dev) CLI installer, so if you have
Flox installed it is already on your `PATH`.

Alternatively, if you are using the Flox CLI installed on a pre-existing Nix installation, you can install it with `flox install tracelinks`.

### From source

The build needs only a C compiler and `make`; `help2man` is used to generate the
man page and `git` is used to fetch the software and determine its version string:

```console
$ git clone https://github.com/flox/tracelinks
$ cd tracelinks
$ make                    # builds ./tracelinks and ./tracelinks.1
$ make install PREFIX=/usr/local
```

`make install` honors `PREFIX` (default `/usr/local`) and installs both the
binary and its man page. Set `VERSION` to stamp the build, e.g.
`make VERSION=1.0.1`.

## Usage

```
tracelinks [OPTION]... PATH [PATH]...
```

| Option | Description |
| ------ | ----------- |
| `-a`, `--absolute`   | report resolved paths as absolute paths |
| `-k`, `--keep-going` | keep reporting on other paths after an error |
| `-f`, `--fstype`     | print the filesystem type for each path reported |
| `-h`, `--help`       | print a help message and exit |
| `-d`, `--debug`      | print extra debugging to stderr |
| `-v`, `--version`    | print the version string and exit |

### Examples

Trace a symlink that points at a regular file:

```console
$ tracelinks tests/good
./tests/good -> file
  ./tests/file: regular file
```

Spot a dangling link (note the nonzero exit status):

```console
$ tracelinks tests/dangling; echo "exit: $?"
./tests/dangling -> noexist
./tests/noexist: No such file or directory
exit: 1
```

Catch a cycle:

```console
$ tracelinks tests/loop1; echo "exit: $?"
./tests/loop1 -> loop2
  ./tests/loop2 -> loop3
    ./tests/loop3 -> loop1
ERROR: loop detected in path: ./tests/loop1 -> ./tests/loop2 -> ./tests/loop3 -> ./tests/loop1
exit: 1
```

Annotate each hop with the filesystem it lives on:

```console
$ tracelinks --fstype /usr/bin/awk
/usr/bin/awk: regular file (apfs)
```

Trace several paths at once, continuing past failures with `--keep-going`:

```console
$ tracelinks --keep-going missing tests/good
./missing: No such file or directory

./tests/good -> file
  ./tests/file: regular file
```

## Exit status

`tracelinks` exits `0` when every path was traversed successfully, and nonzero
when at least one path could not be fully resolved — for example because of a
dangling link, a circular link, or a missing component. With `--keep-going` the
exit status reflects the worst result across all paths.

## How it works

`tracelinks` walks each argument one component at a time. For each component it
calls `lstat(2)`; when the component is a symbolic link it reads the target with
`readlink(2)`, prints the hop, and continues resolving from the link's target
(re-appending whatever path remained after the link). Every `(root, path)` pair
visited during a single traversal is recorded so that a repeat — i.e. a cycle —
can be detected and reported rather than followed forever.

## Development

Development uses the bundled [Flox environment](.flox/env/manifest.toml), which
provides the full toolchain (gcc, gnumake, help2man, clang-tools, gdb, and — on
macOS — the Apple SDK). Activate it, then build and test:

```console
$ flox activate           # enter the development environment
$ make                    # build ./tracelinks and ./tracelinks.1
$ make test               # run the test suite
```

The test suite has two parts:

- **Golden-output tests** under [`tests/`](tests): each fixture is a path (often
  a symlink) with companion `.rc`, `.out`, and `.err` files giving the expected
  exit code, stdout, and stderr. The `Makefile` runs every `tests/*.rc` fixture
  and compares the results.
- **Functional CLI tests** in [`tests/cli-test`](tests/cli-test): a POSIX shell
  script that exercises command-line behavior the golden suite cannot express —
  flags (`-a`, `-f`, `-k`), multiple paths, version/help output, and error
  handling.

`make lint` runs `clang-tidy` (configured by [`.clang-tidy`](.clang-tidy)) and
`make format` applies `clang-format`. On macOS the Flox environment exports
`SDKROOT` so `clang-tidy` can find the system headers.

To build the package exactly as it ships in the Flox CLI installer, use
`flox build`. It runs `make`, `make test`, and `make install` in a sandbox and
links the output into `./result-tracelinks`, which you can then run directly:

```console
$ flox build tracelinks
$ ./result-tracelinks/bin/tracelinks tests/good
./tests/good -> file
  ./tests/file: regular file
```

## See also

`namei(1)` (Linux) covers similar ground; `tracelinks` additionally detects
cycles, annotates filesystem types, and runs on macOS as well as Linux. See also
`ln(1)`, `readlink(1)`, `realpath(1)`, and `stat(1)`.

## License

Released under the terms of the [LICENSE](LICENSE) file in this repository.
