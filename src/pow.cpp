// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2016-2017 The Zcash developers
// Copyright (c) 2018 The Bitcoin Private developers
// Copyright (c) 2017-2018 The Bitcoin Gold developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "crypto/equihash.h"
#include "primitives/block.h"
#include "streams.h"
#include "uint256.h"
#include "util.h"

#include <algorithm>
#include <iostream>

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock,
                                 const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    int nHeight = pindexLast->nHeight + 1;
    bool postfork = nHeight >= params.BTGHeight;

    if (postfork == false) {
        // Original Bitcoin PoW.
        return BitcoinGetNextWorkRequired(pindexLast, pblock, params);
    }
    else if (nHeight < params.BTGHeight + params.BTGPremineWindow) {
        // PoW limit for premine period.
        unsigned int nProofOfWorkLimit = UintToArith256(params.PowLimit(true)).GetCompact();
        return nProofOfWorkLimit;
    }
    else if (nHeight < params.BTGHeight + params.BTGPremineWindow + params.nDigishieldAveragingWindow) {
        // Pow limit start for warm-up period.
        return UintToArith256(params.powLimitStart).GetCompact();
    }
    else if (nHeight < params.BTGZawyLWMAHeight) {
        // Regular Digishield v3.
        return DigishieldGetNextWorkRequired(pindexLast, pblock, params);
    } else if (nHeight < params.BTGEquihashForkHeight) {
        // Zawy's LWMA (testnet launch).
        return LwmaGetNextWorkRequired(pindexLast, pblock, params);
    } else if (nHeight < params.BTGEquihashForkHeight + params.nZawyLwmaAveragingWindow) {
        // Reduce the difficulty of the first forked block by 100x and keep it for N blocks.
        if (nHeight == params.BTGEquihashForkHeight) {
            return ReduceDifficultyBy(pindexLast, 100, params);
        } else {
            return pindexLast->nBits;
        }
    } else {
        // Zawy's LWMA.
        return LwmaGetNextWorkRequired(pindexLast, pblock, params);
    }
}

unsigned int LwmaGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    // Special difficulty rule for testnet:
    // If the new block's timestamp is more than 2 * 10 minutes
    // then allow mining of a min-difficulty block.
    if (params.fPowAllowMinDifficultyBlocks &&
        pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing * 2) {
        return UintToArith256(params.PowLimit(true)).GetCompact();
    }
    return LwmaCalculateNextWorkRequired(pindexLast, params);
}

unsigned int LwmaCalculateNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting) {
        return pindexLast->nBits;
    }

    const int height = pindexLast->nHeight + 1;
    const int64_t T = params.nPowTargetSpacing;
    const int N = params.nZawyLwmaAveragingWindow;
    const int k = params.ZawyLwmaAdjustedWeight(height);
    const int dnorm = params.ZawyLwmaMinDenominator(height);
    const bool limit_st = params.bZawyLwmaSolvetimeLimitation;
    assert(height > N);

    arith_uint256 sum_target;
    int t = 0, j = 0;

    // Loop through N most recent blocks.
    for (int i = height - N; i < height; i++) {
        const CBlockIndex* block = pindexLast->GetAncestor(i);
        const CBlockIndex* block_Prev = block->GetAncestor(i - 1);
        int64_t solvetime = block->GetBlockTime() - block_Prev->GetBlockTime();

        if (limit_st && solvetime > 6 * T) {
            solvetime = 6 * T;
        }

        j++;
        t += solvetime * j;  // Weighted solvetime sum.

        // Target sum divided by a factor, (k N^2).
        // The factor is a part of the final equation. However we divide sum_target here to avoid
        // potential overflow.
        arith_uint256 target;
        target.SetCompact(block->nBits);
        sum_target += target / (k * N * N);
    }
    // Keep t reasonable in case strange solvetimes occurred.
    if (t < N * k / dnorm) {
        t = N * k / dnorm;
    }

    const arith_uint256 pow_limit = UintToArith256(params.PowLimit(true));
    arith_uint256 next_target = t * sum_target;
    if (next_target > pow_limit) {
        next_target = pow_limit;
    }

    return next_target.GetCompact();
}

unsigned int DigishieldGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock,
                                           const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.PowLimit(true)).GetCompact();  // Always postfork.

    const CBlockIndex* pindexFirst = pindexLast;
    arith_uint256 bnTot {0};
    for (int i = 0; pindexFirst && i < params.nDigishieldAveragingWindow; i++) {
        arith_uint256 bnTmp;
        bnTmp.SetCompact(pindexFirst->nBits);
        bnTot += bnTmp;
        pindexFirst = pindexFirst->pprev;
    }
    
    if (pindexFirst == NULL)
        return nProofOfWorkLimit;
    
    arith_uint256 bnAvg {bnTot / params.nDigishieldAveragingWindow};
    return DigishieldCalculateNextWorkRequired(bnAvg, pindexLast, pindexFirst, params);
}

unsigned int DigishieldCalculateNextWorkRequired(arith_uint256 bnAvg, const CBlockIndex* pindexLast, const CBlockIndex* pindexFirst, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;
    
    int64_t nLastBlockTime = pindexLast->GetMedianTimePast();
    int64_t nFirstBlockTime = pindexFirst->GetMedianTimePast();
    // Limit adjustment
    int64_t nActualTimespan = nLastBlockTime - nFirstBlockTime;
    
    if (nActualTimespan < params.DigishieldMinActualTimespan())
        nActualTimespan = params.DigishieldMinActualTimespan();
    if (nActualTimespan > params.DigishieldMaxActualTimespan())
        nActualTimespan = params.DigishieldMaxActualTimespan();

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.PowLimit(true));
    arith_uint256 bnNew {bnAvg};
    bnNew /= params.DigishieldAveragingWindowTimespan();
    bnNew *= nActualTimespan;
    
    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

unsigned int BitcoinGetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.PowLimit(false)).GetCompact();
    
    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    int nHeightFirst = pindexLast->nHeight - (params.DifficultyAdjustmentInterval()-1);
    assert(nHeightFirst >= 0);
    const CBlockIndex* pindexFirst = pindexLast->GetAncestor(nHeightFirst);
    assert(pindexFirst);

    return BitcoinCalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int ReduceDifficultyBy(const CBlockIndex* pindexLast, int64_t multiplier, const Consensus::Params& params) {
    arith_uint256 target;
    target.SetCompact(pindexLast->nBits);
    target *= multiplier;
    const arith_uint256 pow_limit = UintToArith256(params.PowLimit(true));
    if (target > pow_limit) {
        target = pow_limit;
    }
    return target.GetCompact();
}

unsigned int BitcoinCalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;
    
    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespanLegacy/4)
        nActualTimespan = params.nPowTargetTimespanLegacy/4;
    if (nActualTimespan > params.nPowTargetTimespanLegacy*4)
        nActualTimespan = params.nPowTargetTimespanLegacy*4;
    
    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.PowLimit(false));
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespanLegacy;
    
    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;
    
    return bnNew.GetCompact();
}

bool CheckEquihashSolution(const CBlockHeader *pblock, const CChainParams& params)
{
    int height = pblock->nHeight;
    unsigned int n = params.EquihashN(height);
    unsigned int k = params.EquihashK(height);

    // Hash state
    crypto_generichash_blake2b_state state;
    EhInitialiseState(n, k, state, params.EquihashUseBTGSalt(height));

    // I = the block header minus nonce and solution.
    CEquihashInput I{*pblock};
    // I||V
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;
    ss << pblock->nNonce;

    // H(I||V||...
    crypto_generichash_blake2b_update(&state, (unsigned char*)&ss[0], ss.size());

    bool isValid;
    EhIsValidSolution(n, k, state, pblock->nSolution, isValid);
    if (!isValid)
        return error("CheckEquihashSolution(): invalid solution");

    return true;
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, bool postfork, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.PowLimit(postfork)))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
