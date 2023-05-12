{
  description = "Floxpkgs/Project Template";

  inputs.flox-floxpkgs.url = "github:flox/floxpkgs";

  outputs = args @ {flox-floxpkgs, ...}: flox-floxpkgs.project args (_: {});
}
