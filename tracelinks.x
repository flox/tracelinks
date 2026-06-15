[NAME]
tracelinks \- report on symbolic links encountered in path traversals
[DESCRIPTION]
The
.BR tracelinks
command accepts a list of paths and traverses each one in turn,
reporting on symbolic links found in the path traversals.
.P
.B tracelinks
detects and reports on dangling and circular links
encountered along the way, exiting with a nonzero return code.
.P
When reporting the final element encountered in the path traversal
.B tracelinks
prints the
.I realpath
of the file along with its file type.
.SH OPTIONS
[EXAMPLES]
.TP
.B tracelinks /usr/bin/awk
Trace every symbolic link crossed while resolving
.IR /usr/bin/awk ,
printing the final target and its file type.
.TP
.B tracelinks -f ./result
Resolve the
.I result
symlink left behind by a build and annotate each reported path with the
filesystem it lives on.
.TP
.B tracelinks -k a b c
Trace several paths in one invocation, continuing past any path that
produces an error instead of stopping at the first failure.
[EXIT STATUS]
.TP
.B 0
All paths were traversed successfully.
.TP
.B nonzero
At least one path could not be fully traversed, for example because of a
dangling link, a circular link, or a missing file.
[SEE ALSO]
ln(1), readlink(1), realpath(1), stat(1), namei(1)
