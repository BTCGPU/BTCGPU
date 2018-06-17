// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include "arith_uint256.h"
#include "consensus/params.h"

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class CChainParams;
class uint256;

/** PoW algorithm entry. */
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);

/** Zawy's LWMA - next generation algorithm for testnet currently */
unsigned int LwmaGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int LwmaCalculateNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params);

/** Digishield v3 - used in Bitcoin Gold mainnet currently */
unsigned int DigishieldGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int DigishieldCalculateNextWorkRequired(arith_uint256 bnAvg, const CBlockIndex* pindexLast, const CBlockIndex* pindexFirst, const Consensus::Params& params);

/** Original Bitcoin difficulty adjustment algorithm */
unsigned int BitcoinGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params);
unsigned int BitcoinCalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params);

/** Reduce the difficulty by a given multiplier. It doesn't check uint256 overflow! */
unsigned int ReduceDifficultyBy(const CBlockIndex* pindexLast, int64_t multiplier, const Consensus::Params& params);

/** Check whether the Equihash solution in a block header is valid */
bool CheckEquihashSolution(const CBlockHeader *pblock, const CChainParams&);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, bool postfork, const Consensus::Params&);

#endif // BITCOIN_POW_H
