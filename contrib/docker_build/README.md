Building bitcoind for fun and profit
-----

Run the script like this

```
./build.sh ../.. 4
```

for building in the directory above, with 4 cores

Building takes about 15-25 minutes with 4 cores (on my PC), most of it downloading and building boost/zmq/...

QT gui, tests and benchmarks are all disabled and are not built, since we don't need that. If you want to change that, change `CONFIGFLAGS` in `run_inside.sh`.

Boost is downloaded from my Dropbox instead of sourceforge as in the original bitcoin build, since sourceforge is slow and the download took 1 hour, dominating the total time of the build, for no real reason. If you don't like that, just remove the `boost_download_path` in `run_inside.sh` or change it to your own URL.

The `run_inside.sh` is kinda complicated, but that's because it's copied from [here](https://github.com/bitcoin/bitcoin/blob/master/contrib/gitian-descriptors/gitian-linux.yml) :) I don't really understand 50% of it
