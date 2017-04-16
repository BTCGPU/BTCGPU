#!/usr/bin/env bash

if [ "x$1" == "x" ] || [ "x$2" == "x" ]; then
  echo "./build.sh bitcoin-dir number-of-processes" >&2
  echo "           for example: ./build.sh ../ 4" >&2
  echo "more processes = faster build, more CPU" >&2
  exit 1
fi


BITCOINPATH=`realpath "${1/#\~/$HOME}"`

if [ ! -f $BITCOINPATH/src/bitcoind.cpp ]; then 
  echo "First parameter doesn't seem like a bitcoin path" >&2
  echo "" >&2
  echo "./build.sh bitcoin-dir number-of-processes" >&2
  echo "           for example: ./build.sh ../ 4" >&2
  echo "more processes = faster build, more CPU" >&2

  exit 1
fi

re='^[0-9]+$'
if ! [[ $2 =~ $re ]] ; then
  echo "Second parameter not a number" >&2
  echo "" >&2
  echo "./build.sh bitcoin-dir number-of-processes" >&2
  echo "           for example: ./build.sh ../ 4" >&2
  echo "more processes = faster build, more CPU" >&2

  exit 1
fi

set -ex

docker build -t bitcoind_build .

docker run -v $BITCOINPATH:/bitcoin bitcoind_build /run.sh $2

cd $BITCOINPATH/out

# renaming from Bitcoin name to Bitcore name.... stupid thing
rm -f $BITCOINPATH/out/bitcoin-0.14.0-x86_64-linux-gnu-debug.tar.gz
rename s/x86_64-linux-gnu/linux64/ $BITCOINPATH/out/*.tar.gz

sha256sum *.tar.gz > SHA256SUMS
gpg --digest-algo sha256 --clearsign SHA256SUMS
