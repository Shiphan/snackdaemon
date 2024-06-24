#!/usr/bin/bash

cd "$(dirname "$0")"

mkdir -p bin
echo "Compiling..."
go build main.go

# echo
# read -p "sudo copy compiled binary file to /usr/bin? [y/N] " yn
# case $yn in
#     [Yy]* ) sudo cp ./bin/snackdaemon /usr/bin/snackdaemon;;
#     * ) ;;
# esac

# echo
# read -p "Copy default config file to $HOME/.config/snackdaemon? [y/N] " yn
# case $yn in
#     [Yy]* ) cp ./snackdaemon.conf $HOME/.config/snackdaemon;;
#     * ) ;;
# esac
