{
  description = "Minimal Ojuny login splash for mango";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      eachSystem = function:
        nixpkgs.lib.genAttrs systems (system: function nixpkgs.legacyPackages.${system});

      binaryInfo =
        if builtins.pathExists ./binary-url.json then
          builtins.fromJSON (builtins.readFile ./binary-url.json)
        else
          null;

      mkBinary =
        {
          system,
          tag ? (
            if binaryInfo == null then
              throw "ojuny-login-splash: binary-url.json is missing"
            else
              binaryInfo.latest
          ),
          package ? "ojuny-login-splash",
        }:
        let
          release =
            if binaryInfo == null then
              throw "ojuny-login-splash: binary-url.json is missing"
            else
              binaryInfo.releases.${tag}
                or (throw "ojuny-login-splash: release '${tag}' is not recorded in binary-url.json");
          packageInfo =
            release.packages.${package}
              or (throw "ojuny-login-splash: package '${package}' is not recorded in release '${tag}'");
          pkgs = import nixpkgs { inherit system; };
        in
        pkgs.callPackage ./nix/binary.nix {
          name = package;
          program = packageInfo.program or package;
          inherit (packageInfo) file;
          info = {
            inherit (binaryInfo) repo system;
            inherit (release) tag version;
            inherit (packageInfo) url hash;
          };
        };
    in
    {
      packages = eachSystem (pkgs: {
        assets = pkgs.callPackage ./nix/assets.nix { };
        default = pkgs.callPackage ./nix/package.nix {
          assets = pkgs.callPackage ./nix/assets.nix { };
        };
      } // nixpkgs.lib.optionalAttrs
        (binaryInfo != null
          && binaryInfo.system == pkgs.stdenv.hostPlatform.system
          && binaryInfo.latest != "")
        {
          ojuny-login-splash-binary = mkBinary {
            system = pkgs.stdenv.hostPlatform.system;
          };
        });

      devShells = eachSystem (pkgs: {
        default = pkgs.callPackage ./nix/devshell.nix { };
      });

      checks = eachSystem (pkgs: {
        unit = pkgs.callPackage ./nix/checks.nix { };
        default = pkgs.runCommand "check-default" { } ''
          echo ${self.checks.${pkgs.stdenv.hostPlatform.system}.unit} >/dev/null
          touch $out
        '';
      });

      apps = eachSystem (pkgs: {
        default = {
          type = "app";
          program = "${self.packages.${pkgs.stdenv.hostPlatform.system}.default}/bin/ojuny-login-splash";
          meta = {
            description = "Run the Ojuny login splash";
          };
        };
        ci = {
          type = "app";
          program = "${
            pkgs.writeShellApplication {
              name = "ci";
              runtimeInputs = [ pkgs.nix ];
              text = ''
                exec nix flake check "$@"
              '';
            }
          }/bin/ci";
          meta = {
            description = "Run nix flake check";
          };
        };
      });

      homeManagerModules.default = import ./nix/module.nix self;
    }
    // {
      hydraJobs = self.checks;
      lib.mkBinary = mkBinary;
    };
}
