#!/bin/bash

CWD=$(pwd)
set -x

echo "Cleaning dependencies/build directories"

sudo rm -rf ${CWD}/build/*
[ $? -eq 0 ] || { echo "Cannot cleanup ${CWD}/build/"; exit 1; }

sudo rm -rf ${CWD}/dependencies/*
[ $? -eq 0 ] || { echo "Cannot cleanup ${CWD}/dependencies/"; exit 1; }
