{ self, stdenv, help2man, inputs }:
stdenv.mkDerivation {
  pname = "tracelinks";
  version = "1.0.0-${inputs.floxpkgs.lib.getRev self}";
  src = self;

  # Prevent the source from becoming a runtime dependency.
  disallowedReferences = [ self.outPath ];
  nativeBuildInputs = [ help2man ];
  makeFlags = [ "PREFIX=$(out)" ];
}
