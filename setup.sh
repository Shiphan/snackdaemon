#!/usr/bin/bash

mkdir -p bin
echo "Compiling..."
g++ -o bin/snackdaemon main.cpp
echo "Compiled."

echo
read -p "sudo copy compiled binary to /usr/bin? [y/N] " yn
case $yn in
    [Yy]* ) sudo cp ./bin/snackdaemon /usr/bin/snackdaemon;;
    * ) ;;
esac

echo
read -p "Copy default config file to $HOME/.config/snackdaemon? [y/N] " yn
case $yn in
    [Yy]* ) cp ./snackdaemon.conf $HOME/.config/snackdaemon;;
    * ) ;;
esac
