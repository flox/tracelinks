{
  description = "Floxpkgs/Project Template";

  inputs.flox.url = "github:flox/flox";

  # Declaration of external resources
  # =================================

  # =================================

  outputs = args @ {flox, ...}: flox.project args (_: {});
}
