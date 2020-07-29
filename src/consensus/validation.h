// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_VALIDATION_H
#define BITCOIN_CONSENSUS_VALIDATION_H

#include <string>
#include <version.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <primitives/transaction.h>
#include <primitives/block.h>

/** "reject" message codes */
static const unsigned char REJECT_MALFORMED = 0x01;
static const unsigned char REJECT_INVALID = 0x10;
static const unsigned char REJECT_OBSOLETE = 0x11;
static const unsigned char REJECT_DUPLICATE = 0x12;
static const unsigned char REJECT_NONSTANDARD = 0x40;
// static const unsigned char REJECT_DUST = 0x41; // part of BIP 61
static const unsigned char REJECT_INSUFFICIENTFEE = 0x42;
static const unsigned char REJECT_CHECKPOINT = 0x43;

/**
 * A "reason" why something was invalid, suitable for determining whether the
 * provider of the object should be banned/ignored/disconnected/etc. These are
 * much more granular than the rejection codes, which may be more useful for
 * some other use-cases.
 */
enum class ValidationInvalidReason {
    // txn and blocks:
    //! not actually invalid
    NONE,
    //! invalid by consensus rules (excluding any below reasons)
    CONSENSUS,
    /**
     * Invalid by a recent change to consensus rules.
     * Currently unused as there are no such consensus rule changes.
     */
    RECENT_CONSENSUS_CHANGE,
    // Only blocks (or headers):
    //! this object was cached as being invalid, but we don't know why
    CACHED_INVALID,
    //! invalid proof of work or time too old
    BLOCK_INVALID_HEADER,
    //! the block's data didn't match the data committed to by the PoW
    BLOCK_MUTATED,
    //! We don't have the previous block the checked one is built on
    BLOCK_MISSING_PREV,
    //! A block this one builds on is invalid
    BLOCK_INVALID_PREV,
    //! block timestamp was > 2 hours in the future (or our clock is bad)
    BLOCK_TIME_FUTURE,
    //! the block failed to meet one of our checkpoints
    BLOCK_CHECKPOINT,
    //! block finalization problems.
    BLOCK_FINALIZATION,
    // Only loose txn:
    //! didn't meet our local policy rules
    TX_NOT_STANDARD,
    //! a transaction was missing some of its inputs
    TX_MISSING_INPUTS,
    //! transaction spends a coinbase too early, or violates locktime/sequence
    //! locks
    TX_PREMATURE_SPEND,
    /**
     * Tx already in mempool or conflicts with a tx in the chain
     * TODO: Currently this is only used if the transaction already exists in
     * the mempool or on chain,
     * TODO: ATMP's fMissingInputs and a valid CValidationState being used to
     * indicate missing inputs
     */
    TX_CONFLICT,
    //! violated mempool's fee/size/descendant/etc limits
    TX_MEMPOOL_POLICY,

    UNKNOWN,
};

/** Capture information about block/transaction validation */
class CValidationState {
private:
    enum mode_state {
        MODE_VALID,   //!< everything ok
        MODE_INVALID, //!< network rule violation (DoS value may be set)
        MODE_ERROR,   //!< run-time error
    } mode;
    int nDoS;
    ValidationInvalidReason m_reason = ValidationInvalidReason::UNKNOWN;
    std::string strRejectReason;
    unsigned int chRejectCode;
    bool corruptionPossible;
    std::string strDebugMessage;
public:
    CValidationState() : mode(MODE_VALID), nDoS(0), chRejectCode(0), corruptionPossible(false) {}
    bool DoS(int level, bool ret = false,
             unsigned int chRejectCodeIn=0, const std::string &strRejectReasonIn="",
             bool corruptionIn=false,
             const std::string &strDebugMessageIn="") {
        chRejectCode = chRejectCodeIn;
        strRejectReason = strRejectReasonIn;
        corruptionPossible = corruptionIn;
        strDebugMessage = strDebugMessageIn;
        if (mode == MODE_ERROR)
            return ret;
        nDoS += level;
        mode = MODE_INVALID;
        return ret;
    }
    bool Invalid(bool ret = false,
                 unsigned int _chRejectCode=0, const std::string &_strRejectReason="",
                 const std::string &_strDebugMessage="",
                 ValidationInvalidReason reasonIn=ValidationInvalidReason::UNKNOWN) {
        m_reason = reasonIn;
        return DoS(0, ret, _chRejectCode, _strRejectReason, false, _strDebugMessage);
    }
    bool Error(const std::string& strRejectReasonIn) {
        if (mode == MODE_VALID)
            strRejectReason = strRejectReasonIn;
        mode = MODE_ERROR;
        return false;
    }
    bool IsValid() const {
        return mode == MODE_VALID;
    }
    bool IsInvalid() const {
        return mode == MODE_INVALID;
    }
    bool IsError() const {
        return mode == MODE_ERROR;
    }
    bool IsInvalid(int &nDoSOut) const {
        if (IsInvalid()) {
            nDoSOut = nDoS;
            return true;
        }
        return false;
    }
    bool CorruptionPossible() const {
        return corruptionPossible;
    }
    void SetCorruptionPossible() {
        corruptionPossible = true;
    }
    ValidationInvalidReason GetReason() const { return m_reason; }
    unsigned int GetRejectCode() const { return chRejectCode; }
    std::string GetRejectReason() const { return strRejectReason; }
    std::string GetDebugMessage() const { return strDebugMessage; }
};

// These implement the weight = (stripped_size * 4) + witness_size formula,
// using only serialization with and without witness data. As witness_size
// is equal to total_size - stripped_size, this formula is identical to:
// weight = (stripped_size * 3) + total_size.
static inline int64_t GetTransactionWeight(const CTransaction& tx)
{
    return ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
}
static inline int64_t GetBlockWeight(const CBlock& block, const Consensus::Params& params)
{
    int ser_flag = (block.nHeight < (uint32_t)params.BTGHeight) ? SERIALIZE_BLOCK_LEGACY : 0;
    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS | ser_flag) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | ser_flag);
}
static inline int64_t GetTransactionInputWeight(const CTxIn& txin)
{
    // scriptWitness size is added here because witnesses and txins are split up in segwit serialization.
    return ::GetSerializeSize(txin, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(txin, SER_NETWORK, PROTOCOL_VERSION) + ::GetSerializeSize(txin.scriptWitness.stack, SER_NETWORK, PROTOCOL_VERSION);
}

#endif // BITCOIN_CONSENSUS_VALIDATION_H
