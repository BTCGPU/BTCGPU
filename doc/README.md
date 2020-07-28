BTG Core
=============

Setup
---------------------
BTG Core (aka Bitcoin Gold Core, BTCGPU) is the original BTG client and it builds the backbone of the network. It is based on Bitcoin Core. It downloads and, by default, stores the entire history of BTG transactions, which includes Bitcoin transactions from 2009 to 2017 (currently more than 100 GBs); depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more.

You can find BTG Core downloads on the [BTCGPU Github pages](https://github.com/BTCGPU/BTCGPU) and the official [Bitcoin Gold web site](https://bitcoingold.org/).


Running
---------------------
The following are some helpful notes on how to run BTG Core on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/bitcoin-qt` (GUI) or
- `bin/bgoldd` (headless)

### Windows

Unpack the files into a directory, and then run bitcoin-qt.exe.

### macOS

Drag BTG Core to your applications folder, and then run BTG Core.

### Need Help?

For most BTG Core functionality, you can reference commonly availabe Bitcoin Core documentation, including:
* The documentation at the [Bitcoin Wiki](https://en.bitcoin.it/wiki/Main_Page)
for help and more information.

Building
---------------------
The following are developer notes on how to build Bitcoin Core on various native platforms, they generally apply to BTG Core. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [macOS Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [NetBSD Build Notes](build-netbsd.md)
- [Gitian Building Guide](gitian-building.md)

Development
---------------------
The Bitcoin repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://dev.visucore.com/bitcoin/doxygen/)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [Travis CI](travis-ci.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
