# tracelinks
Report symbolic links encountered in a path traversal

Utility to report on symbolic links encountered in path traversals.
Accepts multiple paths, reporting on symbolic links encountered along
the way. Detects and reports on common error conditions such as dangling
or cyclic links, in which case it returns a nonzero exit code.

Includes man page and unit test framework.
