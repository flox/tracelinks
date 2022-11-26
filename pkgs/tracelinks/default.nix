{ self, stdenv, help2man, withRev }:
stdenv.mkDerivation {
  pname = "tracelinks";
  version = withRev "1.0.0";
  src = self;

  # Prevent the source from becoming a runtime dependency.
  disallowedReferences = [ self.outPath ];
  nativeBuildInputs = [ help2man ];
  makeFlags = [ "PREFIX=$(out)" ];
}
