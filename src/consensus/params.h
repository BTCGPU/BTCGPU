// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"
#include <map>
#include <string>

namespace Consensus {

enum DeploymentPos
{
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_CSV, // Deployment of BIP68, BIP112, and BIP113.
    DEPLOYMENT_SEGWIT, // Deployment of BIP141, BIP143, and BIP147.
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;
};

/**
 * Parameters that influence chain consensus.
 */
struct Params {
    uint256 hashGenesisBlock;
    int nSubsidyHalvingInterval;
    /** Block height and hash at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Block height at which Bitcoin GPU hard fork becomes active */
    int BTGHeight;
    /** Premining blocks for Bitcoin GPU hard fork **/
    int BTGPremineWindow;
    bool BTGPremineEnforceWhitelist;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    uint256 powLimit;
    uint256 powLimitLegacy;
    uint256 powLimitStart;
    
    const uint256& PowLimit(bool postfork) const { return postfork ? powLimit : powLimitLegacy; }
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespanLegacy;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespanLegacy / nPowTargetSpacing; }
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;
    
    //Zcash logic for diff adjustment
    int64_t nPowAveragingWindow;
    int64_t nPowMaxAdjustDown;
    int64_t nPowMaxAdjustUp;
    int64_t AveragingWindowTimespan() const { return nPowAveragingWindow * nPowTargetSpacing; }
    int64_t MinActualTimespan() const { return (AveragingWindowTimespan() * (100 - nPowMaxAdjustUp  )) / 100; }
    int64_t MaxActualTimespan() const { return (AveragingWindowTimespan() * (100 + nPowMaxAdjustDown)) / 100; }
    
    
};
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
