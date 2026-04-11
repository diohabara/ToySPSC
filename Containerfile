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

WORKDIR /work
