{
  description = "Floxpkgs/Project Template";

  inputs.flox.url = "github:flox/flox/latest";

  # Declaration of external resources
  # =================================

  # =================================

  outputs = args @ {flox, ...}: flox.project args (_: {});
}
