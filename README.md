# BTG Core (Bitcoin Gold Core)

[![Build Status](https://travis-ci.org/BTCGPU/BTCGPU.svg?branch=master)](https://travis-ci.org/BTCGPU/BTCGPU)

BTG Core (aka Bitcoin Gold Core, BTCGPU) is the original BTG client and it builds the backbone of the network. It is based on Bitcoin Core. BTG began as a fork of Bitcoin after block height 491406 on Tue, 24 Oct 2017 01:17:35 GMT and began being mined as a separate chain at block height 491407 on Sun, 12 Nov 2017 13:34:01 GMT.

This project began out of the desire to create a variant of Bitcoin that is more decentralized by using an ASIC-resistant Proof of Work algorithm, removing barriers to entry for new miners around the world and reducing the concentration of power in the hands of massive-scale mining operations.

BTG preserves and implements Bitcoin features such as SegWit, which enables advanced scaling technologies like Lightning Network. Significant differences at launch time included:

- ASIC-resistant GPU-minable PoW algorithm (Equihash)
- Per-block difficulty adjustment algorithm
- Replay protection (SIGHASH_FORK_ID)
- Unique wallet addresses (prefixes of G and A, to avoid user confusion with Bitcoin)

Although BTG was bootstrapped on 12 Nov to create an entirely new network, it contains the entire Bitcoin blockchain until block 491406. This means BTG was a full fork at that time, with all Bitcoin transaction history since 2009. Any Bitcoin wallet address which held Bitcoin in BTC block 491406 before the fork held an equal number of Bitcoin Gold in BTG block 491407 after the fork, and those funds are accessible through the original private keys.

## Releases

This is the staging tree of Bitcoin Gold. For release version, please switch to [0.17 branch](https://github.com/BTCGPU/BTCGPU/tree/0.17) or [release page](https://github.com/BTCGPU/BTCGPU/releases).

To compile from source, please check the build documentations under [doc](https://github.com/BTCGPU/BTCGPU/tree/master/doc).

## Documentation

- [Usage](doc/README.md)
    - [macOS](doc/README_osx.md)
    - [Windows](doc/README_windows.txt)
- [Configuration and system service](doc/init.md)

## Responsible Disclosure

At the Bitcoin Gold, we consider the security of our systems a top priority. But no matter how much effort we put into system security, there can still be vulnerabilities present.

If you discover a vulnerability, we would like to know about it so we can take steps to address it as quickly as possible. We would like to ask you to help us better protect our users and our systems.

Please check our [Responsible Disclosure](https://github.com/BTCGPU/Developer-Portal/blob/master/responsible-disclosure.md) page for more details.

## Links

* Website: https://bitcoingold.org
* Tech Spec: https://github.com/BTCGPU/BTCGPU/wiki/Technical-Spec
* Twitter: https://twitter.com/bitcoingold
* Facebook: https://www.facebook.com/bitcoingoldofficial/
* Forum: https://forum.bitcoingold.org
* Discord: [invitation](https://discord.gg/HmVUU6S)
* Telegram (Global): https://t.me/BitcoinGoldHQ
