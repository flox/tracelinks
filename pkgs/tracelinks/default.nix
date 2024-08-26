{ stdenv, help2man }:
let
  self = ../..;
in
stdenv.mkDerivation {
  pname = "tracelinks";
  version = "1.0.0";
  src = self;

  # Prevent the source from becoming a runtime dependency.
  disallowedReferences = [ self ];
  nativeBuildInputs = [ help2man ];
  makeFlags = [ "PREFIX=$(out)" ];
}
