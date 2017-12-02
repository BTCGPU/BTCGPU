// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONSENSUS_H
#define BITCOIN_CONSENSUS_CONSENSUS_H

#include <stdlib.h>
#include <stdint.h>

static const unsigned int BITCOINX_FORK_HEIGHT = 500000;
static const unsigned int LEGACY_MAX_BLOCK_BASE_SIZE = (1 * 1000 * 1000);

/** The maximum allowed size for a serialized block, in bytes (only for buffer size limits) */
static const unsigned int MAX_BLOCK_SERIALIZED_SIZE = 8000000;

/** The maximum allowed weight for a block, see BIP 141 (network rule) */
static const unsigned int MAX_BLOCK_WEIGHT = 8000000;
/** The maximum allowed number of signature check operations in a block (network rule) */
static const int64_t MAX_BLOCK_SIGOPS_COST = 160000;

/** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
static const int COINBASE_MATURITY = 100;

static const int WITNESS_SCALE_FACTOR = 4;

static const size_t MIN_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 60; // 60 is the lower bound for the size of a valid serialized CTransaction
static const size_t MIN_SERIALIZABLE_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 10; // 10 is the lower bound for the size of a serialized CTransaction

static inline unsigned int BitcoinX_MaxBlockBaseSize(unsigned int forkHeight, bool isSegwitActive)
{
	if (!isSegwitActive || forkHeight < BITCOINX_FORK_HEIGHT)
		return LEGACY_MAX_BLOCK_BASE_SIZE;

	return (2 * 1000 * 1000);
}

static inline unsigned int BitcoinX_MaxBlockBaseSize()
{
    return BitcoinX_MaxBlockBaseSize(99999999, true);
}

static const uint64_t MAX_BLOCK_BASE_SIGOPS = 20000;

static inline uint64_t BitcoinX_MaxBlockSigOpsCost(unsigned int forkHeight, bool isSegwitActive)
{
    if (!isSegwitActive || forkHeight < BITCOINX_FORK_HEIGHT)
        return (MAX_BLOCK_BASE_SIGOPS * WITNESS_SCALE_FACTOR);

    return ((2 * MAX_BLOCK_BASE_SIGOPS) * WITNESS_SCALE_FACTOR);
}

static inline uint64_t BitcoinX_MaxBlockSigOpsCost()
{
    return BitcoinX_MaxBlockSigOpsCost(99999999, true);
}

/** The maximum allowed number of transactions per block */
static const unsigned int MAX_BLOCK_VTX_SIZE = 1000000;

/** The minimum allowed size for a transaction */
static const unsigned int MIN_TRANSACTION_BASE_SIZE = 10;
/** The maximum allowed size for a transaction, excluding witness data, in bytes */
static const unsigned int MAX_TX_BASE_SIZE = 1000000;
/** The maximum allowed number of transactions per block */
static const unsigned int MAX_BLOCK_VTX = (BitcoinX_MaxBlockBaseSize() / MIN_TRANSACTION_BASE_SIZE);

/** Flags for nSequence and nLockTime locks */
enum {
    /* Interpret sequence numbers as relative lock-time constraints. */
    LOCKTIME_VERIFY_SEQUENCE = (1 << 0),

    /* Use GetMedianTimePast() instead of nTime for end point timestamp. */
    LOCKTIME_MEDIAN_TIME_PAST = (1 << 1),
};

#endif // BITCOIN_CONSENSUS_CONSENSUS_H
