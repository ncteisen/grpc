#!/bin/bash

# EXPERIMENTAL

# This is a utility script for doing a clean install of gRPC and/or
# protobuf. Simply check out the commit from which you would like to
# do this install, and run this.

usage() {
  echo -e "Usage:\tsudo ./clean-install.sh"
  echo ""
  echo -e "\tsudo ./clean-install.sh just-grpc"
  echo -e "\tsudo ./clean-install.sh just-protobuf"
}

SYSTEM=`uname`

if [[ $SYSTEM -ne "Darwin" ]] && [[ $SYSTEM -ne "Linux" ]]; then
  echo "This has only been tested on Darwin and Linux"
  exit 1
fi

# This must run as root since we are installing things on the system
if [[ "$EUID" -ne 0 ]]; then
  echo "You must run this script as root"
  echo ""
  usage
  exit 1
fi

# just-grpc or just-protobuf are the only supported parameters
if [[ $# -gt 0 ]]; then
  if [[ "$1" != "just-grpc" ]] && [[ "$1" != "just-protobuf" ]]; then
    usage
    exit
  fi
fi

clean_install_protobuf() {
  git submodule foreach git clean -xfd
  git submodule update --init
  cd third_party/protobuf
  ./autogen.sh
  ./configure
  make -j2
  make install
  if [[ $SYSTEM == "Linux" ]]; then
    ldconfig
  fi
  cd ../..
}

clean_install_grpc() {
  git clean -xfd
  make -j2
  make install
}

if [[ "$1" != "just-grpc" ]]; then 
  clean_install_protobuf
fi
if [[ "$1" != "just-protobuf" ]]; then
  clean_install_grpc
fi
