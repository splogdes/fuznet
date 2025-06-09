{
  description = "fuznet - Vivado fuzz / PnR / equiv flow (pure Nix)";

  inputs = {
    nixpkgs.url     = "github:nixos/nixpkgs/063f43f2dbdef86376cc29ad646c45c46e93234c";
    flake-utils.url = "github:numtide/flake-utils";

    tomlplusplus-src.url = "github:marzer/tomlplusplus/v3.4.0";
    tomlplusplus-src.flake = false;

    yaml-cpp.url = "github:jbeder/yaml-cpp/master";
    yaml-cpp.flake = false;

    cli11-src.url = "github:CLIUtils/CLI11/v2.5.0";
    cli11-src.flake = false;

    json.url = "github:nlohmann/json/v3.11.3";
    json.flake = false;

  };

  outputs = inputs @ { self, nixpkgs, flake-utils, ... }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        cmakeDeps = {
          tomlplusplus = inputs.tomlplusplus-src;
          cli11        = inputs.cli11-src;
          yaml-cpp     = inputs.yaml-cpp;
          json         = inputs.json;
        };
      in
      rec {
        packages = rec {
          
          fuznet = pkgs.stdenv.mkDerivation {
            pname   = "fuznet";
            version = "0.5.3";
            src     = ./.;

            nativeBuildInputs = with pkgs; [ cmake ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              "-DFETCHCONTENT_SOURCE_DIR_YAML_CPP=${cmakeDeps.yaml-cpp}"
              "-DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=${cmakeDeps.tomlplusplus}"
              "-DFETCHCONTENT_SOURCE_DIR_CLI11=${cmakeDeps.cli11}"
              "-DFETCHCONTENT_SOURCE_DIR_JSON=${cmakeDeps.json}"
            ];
          };


          default = fuznet;
        };

        devShells.default = pkgs.mkShell {
          nativeBuildInputs = [ packages.fuznet ];     
          packages = with pkgs; [
            yosys
            verilator
            python3
            cmake
            git
            z3
            jq
            bashInteractive
          ];
          
          shellHook = ''echo "Welcome to the fuznet development shell!"'';
        };
      });
}
