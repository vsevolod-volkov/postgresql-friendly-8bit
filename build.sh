#!/bin/bash

set -e

docker=no
platform=""

while [ -n "$1" ]; do
   case "$1" in
      "--docker")
         docker=yes
      ;;
      "--platform")
         shift
         platform=" --platform $1"
      ;;
      *)
         echo "Invalid switch \"$1\"."
         usage
         exit 1
      ;;
   esac
   shift
done

if [ "$docker" = "yes" ]; then
   docker run $platform --interactive --tty --rm --volume .:/project $(docker build $platform --quiet docker/) make
else
   make
fi