FROM docker.io/nixos/nix:latest

ENV NIX_CONFIG="experimental-features = nix-command flakes"

RUN nix profile install \
    nixpkgs#cmake \
    nixpkgs#ninja \
    nixpkgs#gcc14 \
    nixpkgs#clang-tools \
    nixpkgs#lcov \
    nixpkgs#doxygen \
    nixpkgs#linuxPackages.perf \
    nixpkgs#valgrind

RUN nix profile install nixpkgs#python3 nixpkgs#perl

RUN nix-shell -p git --run 'git clone --depth 1 https://github.com/brendangregg/FlameGraph.git /opt/FlameGraph'

WORKDIR /work
