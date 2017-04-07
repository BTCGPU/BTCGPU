#!/usr/bin/env bash

cd bitcoin
mkdir bitcoin-0.14.0
cd bitcoin-0.14.0
mkdir bin
cp ../src/bitcoin-cli ./bin
cp ../src/bitcoind ./bin
cp ../src/bitcoin-tx ./bin
strip ./bin/*

mkdir include
cp ../src/script/bitcoinconsensus.h ./include

mkdir lib
cp ../src/.libs/libbitcoinconsensus.so.0.0.0 ./lib
ln -s ./lib/libbitcoinconsensus.so.0.0.0 ./lib.libbitcoinconsensus.so.0
ln -s ./lib/libbitcoinconsensus.so.0.0.0 ./lib.libbitcoinconsensus.so
strip ./lib/libbitcoinconsensus.so.0.0.0

cd ..
tar zcvf bitcoin-0.14.0-linux64.tar.gz bitcoin-0.14.0
