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
[SEE ALSO]
ln(1), realpath(1), stat(1)
