// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATION_H
#define BITCOIN_VALIDATION_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <amount.h>
#include <coins.h>
#include <crypto/common.h> // for ReadLE64
#include <fs.h>
#include <optional.h>
#include <policy/feerate.h>
#include <protocol.h> // For CMessageHeader::MessageStartChars
#include <script/script_error.h>
#include <sync.h>
#include <txmempool.h> // For CTxMemPool::cs
#include <txdb.h>
#include <versionbits.h>
#include <serialize.h>

#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

class CChainState;
class BlockValidationState;
class CBlockIndex;
class CBlockTreeDB;
class CBlockUndo;
class CChainParams;
class CInv;
class CConnman;
class CScriptCheck;
class CBlockPolicyEstimator;
class CTxMemPool;
class ChainstateManager;
class TxValidationState;
struct ChainTxData;

struct DisconnectedBlockTransactions;
struct PrecomputedTransactionData;
struct LockPoints;

/** Default for -minrelaytxfee, minimum relay fee for transactions */
static const unsigned int DEFAULT_MIN_RELAY_TX_FEE = 1000;
/** Default for -limitancestorcount, max number of in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_LIMIT = 25;
/** Default for -limitancestorsize, maximum kilobytes of tx + all in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_SIZE_LIMIT = 101;
/** Default for -limitdescendantcount, max number of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_LIMIT = 25;
/** Default for -limitdescendantsize, maximum kilobytes of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_SIZE_LIMIT = 101;
/** Default for -mempoolexpiry, expiration time for mempool transactions in hours */
static const unsigned int DEFAULT_MEMPOOL_EXPIRY = 336;
/** The maximum size of a blk?????.dat file (since 0.8) */
static const unsigned int MAX_BLOCKFILE_SIZE = 0x8000000; // 128 MiB
/** Maximum number of dedicated script-checking threads allowed */
static const int MAX_SCRIPTCHECK_THREADS = 15;
/** -par default (number of script-checking threads, 0 = auto) */
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;
static const int64_t DEFAULT_MAX_TIP_AGE = 24 * 60 * 60;
static const bool DEFAULT_CHECKPOINTS_ENABLED = true;
static const bool DEFAULT_TXINDEX = false;
static const char* const DEFAULT_BLOCKFILTERINDEX = "0";
/** Default for -persistmempool */
static const bool DEFAULT_PERSIST_MEMPOOL = true;
/** Default for using fee filter */
static const bool DEFAULT_FEEFILTER = true;
/** Default for -stopatheight */
static const int DEFAULT_STOPATHEIGHT = 0;
/** Block files containing a block-height within MIN_BLOCKS_TO_KEEP of ::ChainActive().Tip() will not be pruned. */
static const unsigned int MIN_BLOCKS_TO_KEEP = 288;
static const signed int DEFAULT_CHECKBLOCKS = 6;
static const unsigned int DEFAULT_CHECKLEVEL = 3;
// Require that user allocate at least 550 MiB for block & undo files (blk???.dat and rev???.dat)
// At 1MB per block, 288 blocks = 288MB.
// Add 15% for Undo data = 331MB
// Add 20% for Orphan block rate = 397MB
// We want the low water mark after pruning to be at least 397 MB and since we prune in
// full block file chunks, we need the high water mark which triggers the prune to be
// one 128MB block file + added 15% undo data = 147MB greater for a total of 545MB
// Setting the target to >= 550 MiB will make it likely we can respect the target.
static const uint64_t MIN_DISK_SPACE_FOR_BLOCK_FILES = 550 * 1024 * 1024;

/** Default for -minfinalizationdepth */
static const int DEFAULT_MAX_REORG_DEPTH = 9;

/* This is the minimum time between a block header reception and the block
 * finalization.
 * This value should be >> block propagation and validation time
 */
static const int64_t DEFAULT_MIN_FINALIZATION_DELAY = 80 * 60;

/**
 * Reject codes greater or equal to this can be returned by AcceptToMemPool or
 * AcceptBlock for blocks/transactions, to signal internal conditions. They
 * cannot and should not be sent over the P2P network.
 */
/** Block conflicts with a transaction already known */
static const unsigned int REJECT_AGAINST_FINALIZED = 0x103;

struct BlockHasher
{
    // this used to call `GetCheapHash()` in uint256, which was later moved; the
    // cheap hash function simply calls ReadLE64() however, so the end result is
    // identical
    size_t operator()(const uint256& hash) const { return ReadLE64(hash.begin()); }
};

/** Current sync state passed to tip changed callbacks. */
enum class SynchronizationState {
    INIT_REINDEX,
    INIT_DOWNLOAD,
    POST_INIT
};

extern RecursiveMutex cs_main;
extern CBlockPolicyEstimator feeEstimator;
typedef std::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;
extern Mutex g_best_block_mutex;
extern std::condition_variable g_best_block_cv;
extern uint256 g_best_block;
extern std::atomic_bool fImporting;
extern std::atomic_bool fReindex;
/** Whether there are dedicated script-checking threads running.
 * False indicates all script checking is done on the main threadMessageHandler thread.
 */
extern bool g_parallel_script_checks;
extern bool fRequireStandard;
extern bool fCheckBlockIndex;
extern bool fCheckpointsEnabled;
/** A fee rate smaller than this is considered zero fee (for relaying, mining and transaction creation) */
extern CFeeRate minRelayTxFee;
/** If the tip is older than this (in seconds), the node is considered to be in initial block download. */
extern int64_t nMaxTipAge;

/** Block hash whose ancestors we will assume to have valid scripts without checking them. */
extern uint256 hashAssumeValid;

/** Minimum work we will assume exists on some valid chain. */
extern arith_uint256 nMinimumChainWork;

/** Best header we've seen so far (used for getheaders queries' starting points). */
extern CBlockIndex *pindexBestHeader;

/** Pruning-related variables and constants */
/** True if any block files have ever been pruned. */
extern bool fHavePruned;
/** True if we're running in -prune mode. */
extern bool fPruneMode;
/** Number of MiB of block files that we're trying to stay below. */
extern uint64_t nPruneTarget;
/** Documentation for argument 'checklevel'. */
extern const std::vector<std::string> CHECKLEVEL_DOC;

/** Open a block file (blk?????.dat) */
FILE* OpenBlockFile(const FlatFilePos &pos, bool fReadOnly = false);
/** Translation to a filesystem path */
fs::path GetBlockPosFilename(const FlatFilePos &pos);
/** Import blocks from an external file */
void LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, FlatFilePos* dbp = nullptr);
/** Ensures we have a genesis block in the block tree, possibly writing one to disk. */
bool LoadGenesisBlock(const CChainParams& chainparams);
/** Unload database information */
void UnloadBlockIndex(CTxMemPool* mempool, ChainstateManager& chainman);
/** Run an instance of the script checking thread */
void ThreadScriptCheck(int worker_num);
/**
 * Return transaction from the block at block_index.
 * If block_index is not provided, fall back to mempool.
 * If mempool is not provided or the tx couldn't be found in mempool, fall back to g_txindex.
 *
 * @param[in]  block_index     The block to read from disk, or nullptr
 * @param[in]  mempool         If block_index is not provided, look in the mempool, if provided
 * @param[in]  hash            The txid
 * @param[in]  consensusParams The params
 * @param[out] hashBlock       The hash of block_index, if the tx was found via block_index
 * @returns                    The tx if found, otherwise nullptr
 */
CTransactionRef GetTransaction(const CBlockIndex* const block_index, const CTxMemPool* const mempool, const uint256& hash, const Consensus::Params& consensusParams, uint256& hashBlock);
/**
 * Find the best known block, and make it the tip of the block chain
 *
 * May not be called with cs_main held. May not be called in a
 * validationinterface callback.
 */
bool ActivateBestChain(BlockValidationState& state, const CChainParams& chainparams, std::shared_ptr<const CBlock> pblock = std::shared_ptr<const CBlock>());
CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams);

/** Guess verification progress (as a fraction between 0.0=genesis and 1.0=current tip). */
double GuessVerificationProgress(const ChainTxData& data, const CBlockIndex* pindex);

/** Calculate the amount of disk space the block & undo files currently use */
uint64_t CalculateCurrentUsage();

/**
 *  Actually unlink the specified files
 */
void UnlinkPrunedFiles(const std::set<int>& setFilesToPrune);

/** Prune block files up to a given height */
void PruneBlockFilesManual(int nManualPruneHeight);

/** (try to) add transaction to memory pool
 * plTxnReplaced will be appended to with all transactions replaced from mempool
 * @param[out] fee_out optional argument to return tx fee to the caller **/
bool AcceptToMemoryPool(CTxMemPool& pool, TxValidationState &state, const CTransactionRef &tx,
                        std::list<CTransactionRef>* plTxnReplaced,
                        bool bypass_limits, bool test_accept=false, CAmount* fee_out=nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Get the BIP9 state for a given deployment at the current tip. */
ThresholdState VersionBitsTipState(const Consensus::Params& params, Consensus::DeploymentPos pos);

/** Get the numerical statistics for the BIP9 state for a given deployment at the current tip. */
BIP9Stats VersionBitsTipStatistics(const Consensus::Params& params, Consensus::DeploymentPos pos);

/** Get the block height at which the BIP9 deployment switched into the state for the block building on the current tip. */
int VersionBitsTipStateSinceHeight(const Consensus::Params& params, Consensus::DeploymentPos pos);


/** Apply the effects of this transaction on the UTXO set represented by view */
void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight);

/** Transaction validation functions */

/**
 * Check if transaction will be final in the next block to be created.
 *
 * Calls IsFinalTx() with current block height and appropriate block time.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckFinalTx(const CTransaction &tx, int flags = -1) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Test whether the LockPoints height and time are still valid on the current chain
 */
bool TestLockPointValidity(const LockPoints* lp) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Check if transaction will be BIP 68 final in the next block to be created.
 *
 * Simulates calling SequenceLocks() with data from the tip of the current active chain.
 * Optionally stores in LockPoints the resulting height and time calculated and the hash
 * of the block needed for calculation or skips the calculation and uses the LockPoints
 * passed in for evaluation.
 * The LockPoints should not be considered valid if CheckSequenceLocks returns false.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckSequenceLocks(const CTxMemPool& pool, const CTransaction& tx, int flags, LockPoints* lp = nullptr, bool useExistingLockPoints = false) EXCLUSIVE_LOCKS_REQUIRED(::cs_main, pool.cs);

/**
 * Closure representing one script verification
 * Note that this stores references to the spending transaction
 */
class CScriptCheck
{
private:
    CTxOut m_tx_out;
    const CTransaction *ptxTo;
    unsigned int nIn;
    unsigned int nFlags;
    bool cacheStore;
    ScriptError error;
    PrecomputedTransactionData *txdata;

public:
    CScriptCheck(): ptxTo(nullptr), nIn(0), nFlags(0), cacheStore(false), error(SCRIPT_ERR_UNKNOWN_ERROR) {}
    CScriptCheck(const CTxOut& outIn, const CTransaction& txToIn, unsigned int nInIn, unsigned int nFlagsIn, bool cacheIn, PrecomputedTransactionData* txdataIn) :
        m_tx_out(outIn), ptxTo(&txToIn), nIn(nInIn), nFlags(nFlagsIn), cacheStore(cacheIn), error(SCRIPT_ERR_UNKNOWN_ERROR), txdata(txdataIn) { }

    bool operator()();

    void swap(CScriptCheck &check) {
        std::swap(ptxTo, check.ptxTo);
        std::swap(m_tx_out, check.m_tx_out);
        std::swap(nIn, check.nIn);
        std::swap(nFlags, check.nFlags);
        std::swap(cacheStore, check.cacheStore);
        std::swap(error, check.error);
        std::swap(txdata, check.txdata);
    }

    ScriptError GetScriptError() const { return error; }
};

/** Initializes the script-execution cache */
void InitScriptExecutionCache();


/** Functions for disk access for blocks */
bool ReadBlockFromDisk(CBlock& block, const FlatFilePos& pos, const Consensus::Params& consensusParams);
bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& consensusParams);
bool ReadRawBlockFromDisk(std::vector<uint8_t>& block, const FlatFilePos& pos, const CMessageHeader::MessageStartChars& message_start);
bool ReadRawBlockFromDisk(std::vector<uint8_t>& block, const CBlockIndex* pindex, const CMessageHeader::MessageStartChars& message_start);

bool UndoReadFromDisk(CBlockUndo& blockundo, const CBlockIndex* pindex);

/** Functions for validating blocks and updating the block tree */

/** Context-independent validity checks */
bool CheckBlock(const CBlock& block, BlockValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW = true, bool fCheckMerkleRoot = true);

/** Check a block is completely valid from start to finish (only works on top of our current best block) */
bool TestBlockValidity(BlockValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* pindexPrev, bool fCheckPOW = true, bool fCheckMerkleRoot = true) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Check whether witness commitments are required for a block, and whether to enforce NULLDUMMY (BIP 147) rules.
 *  Note that transaction witness validation rules are always enforced when P2SH is enforced. */
bool IsWitnessEnabled(const CBlockIndex* pindexPrev, const Consensus::Params& params);

/** Update uncommitted block structures (currently: only the witness reserved value). This is safe for submitted blocks. */
void UpdateUncommittedBlockStructures(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams);

/** Produce the necessary coinbase commitment for a block (modifies the hash, don't call for mined blocks). */
std::vector<unsigned char> GenerateCoinbaseCommitment(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams);

/** RAII wrapper for VerifyDB: Verify consistency of the block and coin databases */
class CVerifyDB {
public:
    CVerifyDB();
    ~CVerifyDB();
    bool VerifyDB(const CChainParams& chainparams, CCoinsView *coinsview, int nCheckLevel, int nCheckDepth);
};

CBlockIndex* LookupBlockIndex(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/** Find the last common block between the parameter chain and a locator. */
CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

enum DisconnectResult
{
    DISCONNECT_OK,      // All good.
    DISCONNECT_UNCLEAN, // Rolled back, but UTXO set was inconsistent with block.
    DISCONNECT_FAILED   // Something else went wrong.
};

class ConnectTrace;

/** @see CChainState::FlushStateToDisk */
enum class FlushStateMode {
    NONE,
    IF_NEEDED,
    PERIODIC,
    ALWAYS
};

struct CBlockIndexWorkComparator
{
    bool operator()(const CBlockIndex *pa, const CBlockIndex *pb) const;
};

/**
 * Maintains a tree of blocks (stored in `m_block_index`) which is consulted
 * to determine where the most-work tip is.
 *
 * This data is used mostly in `CChainState` - information about, e.g.,
 * candidate tips is not maintained here.
 */
class BlockManager
{
    friend CChainState;

private:
    /* Calculate the block/rev files to delete based on height specified by user with RPC command pruneblockchain */
    void FindFilesToPruneManual(std::set<int>& setFilesToPrune, int nManualPruneHeight, int chain_tip_height);

    /**
     * Prune block and undo files (blk???.dat and undo???.dat) so that the disk space used is less than a user-defined target.
     * The user sets the target (in MB) on the command line or in config file.  This will be run on startup and whenever new
     * space is allocated in a block or undo file, staying below the target. Changing back to unpruned requires a reindex
     * (which in this case means the blockchain must be re-downloaded.)
     *
     * Pruning functions are called from FlushStateToDisk when the global fCheckForPruning flag has been set.
     * Block and undo files are deleted in lock-step (when blk00003.dat is deleted, so is rev00003.dat.)
     * Pruning cannot take place until the longest chain is at least a certain length (100000 on mainnet, 1000 on testnet, 1000 on regtest).
     * Pruning will never delete a block within a defined distance (currently 288) from the active chain's tip.
     * The block index is updated by unsetting HAVE_DATA and HAVE_UNDO for any blocks that were stored in the deleted files.
     * A db flag records the fact that at least some block files have been pruned.
     *
     * @param[out]   setFilesToPrune   The set of file indices that can be unlinked will be returned
     */
    void FindFilesToPrune(std::set<int>& setFilesToPrune, uint64_t nPruneAfterHeight, int chain_tip_height, bool is_ibd);

public:
    BlockMap m_block_index GUARDED_BY(cs_main);

    /** In order to efficiently track invalidity of headers, we keep the set of
      * blocks which we tried to connect and found to be invalid here (ie which
      * were set to BLOCK_FAILED_VALID since the last restart). We can then
      * walk this set and check if a new header is a descendant of something in
      * this set, preventing us from having to walk m_block_index when we try
      * to connect a bad block and fail.
      *
      * While this is more complicated than marking everything which descends
      * from an invalid block as invalid at the time we discover it to be
      * invalid, doing so would require walking all of m_block_index to find all
      * descendants. Since this case should be very rare, keeping track of all
      * BLOCK_FAILED_VALID blocks in a set should be just fine and work just as
      * well.
      *
      * Because we already walk m_block_index in height-order at startup, we go
      * ahead and mark descendants of invalid blocks as FAILED_CHILD at that time,
      * instead of putting things in this set.
      */
    std::set<CBlockIndex*> m_failed_blocks;

    /**
     * All pairs A->B, where A (or one of its ancestors) misses transactions, but B has transactions.
     * Pruned nodes may have entries where B is missing data.
     */
    std::multimap<CBlockIndex*, CBlockIndex*> m_blocks_unlinked;

    /**
     * Load the blocktree off disk and into memory. Populate certain metadata
     * per index entry (nStatus, nChainWork, nTimeMax, etc.) as well as peripheral
     * collections like setDirtyBlockIndex.
     *
     * @param[out] block_index_candidates  Fill this set with any valid blocks for
     *                                     which we've downloaded all transactions.
     */
    bool LoadBlockIndex(
        const Consensus::Params& consensus_params,
        CBlockTreeDB& blocktree,
        std::set<CBlockIndex*, CBlockIndexWorkComparator>& block_index_candidates)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    /** Clear all data members. */
    void Unload() EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    CBlockIndex* AddToBlockIndex(const CBlockHeader& block) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    /** Create a new block index entry for a given block hash */
    CBlockIndex* InsertBlockIndex(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Mark one block file as pruned (modify associated database entries)
    void PruneOneBlockFile(const int fileNumber) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    /**
     * If a block header hasn't already been seen, call CheckBlockHeader on it, ensure
     * that it doesn't descend from an invalid block, and then add it to m_block_index.
     */
    bool AcceptBlockHeader(
        const CBlockHeader& block,
        BlockValidationState& state,
        const CChainParams& chainparams,
        CBlockIndex** ppindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    ~BlockManager() {
        Unload();
    }
};

/**
 * A convenience class for constructing the CCoinsView* hierarchy used
 * to facilitate access to the UTXO set.
 *
 * This class consists of an arrangement of layered CCoinsView objects,
 * preferring to store and retrieve coins in memory via `m_cacheview` but
 * ultimately falling back on cache misses to the canonical store of UTXOs on
 * disk, `m_dbview`.
 */
class CoinsViews {

public:
    //! The lowest level of the CoinsViews cache hierarchy sits in a leveldb database on disk.
    //! All unspent coins reside in this store.
    CCoinsViewDB m_dbview GUARDED_BY(cs_main);

    //! This view wraps access to the leveldb instance and handles read errors gracefully.
    CCoinsViewErrorCatcher m_catcherview GUARDED_BY(cs_main);

    //! This is the top layer of the cache hierarchy - it keeps as many coins in memory as
    //! can fit per the dbcache setting.
    std::unique_ptr<CCoinsViewCache> m_cacheview GUARDED_BY(cs_main);

    //! This constructor initializes CCoinsViewDB and CCoinsViewErrorCatcher instances, but it
    //! *does not* create a CCoinsViewCache instance by default. This is done separately because the
    //! presence of the cache has implications on whether or not we're allowed to flush the cache's
    //! state to disk, which should not be done until the health of the database is verified.
    //!
    //! All arguments forwarded onto CCoinsViewDB.
    CoinsViews(std::string ldb_name, size_t cache_size_bytes, bool in_memory, bool should_wipe);

    //! Initialize the CCoinsViewCache member.
    void InitCache() EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
};

enum class CoinsCacheSizeState
{
    //! The coins cache is in immediate need of a flush.
    CRITICAL = 2,
    //! The cache is at >= 90% capacity.
    LARGE = 1,
    OK = 0
};

/**
 * CChainState stores and provides an API to update our local knowledge of the
 * current best chain.
 *
 * Eventually, the API here is targeted at being exposed externally as a
 * consumable libconsensus library, so any functions added must only call
 * other class member functions, pure functions in other parts of the consensus
 * library, callbacks via the validation interface, or read/write-to-disk
 * functions (eventually this will also be via callbacks).
 *
 * Anything that is contingent on the current tip of the chain is stored here,
 * whereas block information and metadata independent of the current tip is
 * kept in `BlockMetadataManager`.
 */
class CChainState
{
protected:
    /**
     * Every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
    RecursiveMutex cs_nBlockSequenceId;
    /** Blocks loaded from disk are assigned id 0, so start the counter at 1. */
    int32_t nBlockSequenceId = 1;
    /** Decreasing counter (used by subsequent preciousblock calls). */
    int32_t nBlockReverseSequenceId = -1;
    /** chainwork for the last block that preciousblock has been applied to. */
    arith_uint256 nLastPreciousChainwork = 0;

    /**
     * the ChainState CriticalSection
     * A lock that must be held when modifying this ChainState - held in ActivateBestChain()
     */
    RecursiveMutex m_cs_chainstate;

    /**
     * Whether this chainstate is undergoing initial block download.
     *
     * Mutable because we need to be able to mark IsInitialBlockDownload()
     * const, which latches this for caching purposes.
     */
    mutable std::atomic<bool> m_cached_finished_ibd{false};

    //! Reference to a BlockManager instance which itself is shared across all
    //! CChainState instances. Keeping a local reference allows us to test more
    //! easily as opposed to referencing a global.
    BlockManager& m_blockman;

    //! mempool that is kept in sync with the chain
    CTxMemPool& m_mempool;

    //! Manages the UTXO set, which is a reflection of the contents of `m_chain`.
    std::unique_ptr<CoinsViews> m_coins_views;

public:
    explicit CChainState(CTxMemPool& mempool, BlockManager& blockman, uint256 from_snapshot_blockhash = uint256());

    /**
     * Initialize the CoinsViews UTXO set database management data structures. The in-memory
     * cache is initialized separately.
     *
     * All parameters forwarded to CoinsViews.
     */
    void InitCoinsDB(
        size_t cache_size_bytes,
        bool in_memory,
        bool should_wipe,
        std::string leveldb_name = "chainstate");

    //! Initialize the in-memory coins cache (to be done after the health of the on-disk database
    //! is verified).
    void InitCoinsCache(size_t cache_size_bytes) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

    //! @returns whether or not the CoinsViews object has been fully initialized and we can
    //!          safely flush this object to disk.
    bool CanFlushToDisk() EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
        return m_coins_views && m_coins_views->m_cacheview;
    }

    //! The current chain of blockheaders we consult and build on.
    //! @see CChain, CBlockIndex.
    CChain m_chain;

    /**
     * The blockhash which is the base of the snapshot this chainstate was created from.
     *
     * IsNull() if this chainstate was not created from a snapshot.
     */
    const uint256 m_from_snapshot_blockhash{};

    /**
     * The set of all CBlockIndex entries with BLOCK_VALID_TRANSACTIONS (for itself and all ancestors) and
     * as good as our current tip or better. Entries may be failed, though, and pruning nodes may be
     * missing the data for the block.
     */
    std::set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;

    //! @returns A reference to the in-memory cache of the UTXO set.
    CCoinsViewCache& CoinsTip() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        assert(m_coins_views->m_cacheview);
        return *m_coins_views->m_cacheview.get();
    }

    //! @returns A reference to the on-disk UTXO set database.
    CCoinsViewDB& CoinsDB() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        return m_coins_views->m_dbview;
    }

    //! @returns A reference to a wrapped view of the in-memory UTXO set that
    //!     handles disk read errors gracefully.
    CCoinsViewErrorCatcher& CoinsErrorCatcher() EXCLUSIVE_LOCKS_REQUIRED(cs_main)
    {
        return m_coins_views->m_catcherview;
    }

    //! Destructs all objects related to accessing the UTXO set.
    void ResetCoinsViews() { m_coins_views.reset(); }

    //! The cache size of the on-disk coins view.
    size_t m_coinsdb_cache_size_bytes{0};

    //! The cache size of the in-memory coins view.
    size_t m_coinstip_cache_size_bytes{0};

    //! Resize the CoinsViews caches dynamically and flush state to disk.
    //! @returns true unless an error occurred during the flush.
    bool ResizeCoinsCaches(size_t coinstip_size, size_t coinsdb_size)
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

    /**
     * Update the on-disk chain state.
     * The caches and indexes are flushed depending on the mode we're called with
     * if they're too large, if it's been a while since the last write,
     * or always and in all cases if we're in prune mode and are deleting files.
     *
     * If FlushStateMode::NONE is used, then FlushStateToDisk(...) won't do anything
     * besides checking if we need to prune.
     *
     * @returns true unless a system error occurred
     */
    bool FlushStateToDisk(
        const CChainParams& chainparams,
        BlockValidationState &state,
        FlushStateMode mode,
        int nManualPruneHeight = 0);

    //! Unconditionally flush all changes to disk.
    void ForceFlushStateToDisk();

    //! Prune blockfiles from the disk if necessary and then flush chainstate changes
    //! if we pruned.
    void PruneAndFlush();

    /**
     * Make the best chain active, in multiple steps. The result is either failure
     * or an activated best chain. pblock is either nullptr or a pointer to a block
     * that is already loaded (to avoid loading it again from disk).
     *
     * ActivateBestChain is split into steps (see ActivateBestChainStep) so that
     * we avoid holding cs_main for an extended period of time; the length of this
     * call may be quite long during reindexing or a substantial reorg.
     *
     * May not be called with cs_main held. May not be called in a
     * validationinterface callback.
     *
     * @returns true unless a system error occurred
     */
    bool ActivateBestChain(
        BlockValidationState& state,
        const CChainParams& chainparams,
        std::shared_ptr<const CBlock> pblock) LOCKS_EXCLUDED(cs_main);

    bool AcceptBlock(const std::shared_ptr<const CBlock>& pblock, BlockValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex, bool fRequested, const FlatFilePos* dbp, bool* fNewBlock) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    // Block (dis)connection on a given view:
    DisconnectResult DisconnectBlock(const CBlock& block, const CBlockIndex* pindex, CCoinsViewCache& view);
    bool ConnectBlock(const CBlock& block, BlockValidationState& state, CBlockIndex* pindex,
                      CCoinsViewCache& view, const CChainParams& chainparams, bool fJustCheck = false) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    // Apply the effects of a block disconnection on the UTXO set.
    bool DisconnectTip(BlockValidationState& state, const CChainParams& chainparams, DisconnectedBlockTransactions* disconnectpool) EXCLUSIVE_LOCKS_REQUIRED(cs_main, m_mempool.cs);

    // Manual block validity manipulation:
    bool PreciousBlock(BlockValidationState& state, const CChainParams& params, CBlockIndex* pindex) LOCKS_EXCLUDED(cs_main);
    bool InvalidateBlock(BlockValidationState& state, const CChainParams& chainparams, CBlockIndex* pindex) LOCKS_EXCLUDED(cs_main);
    void ResetBlockFailureFlags(CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    /** Replay blocks that aren't fully applied to the database. */
    bool ReplayBlocks(const CChainParams& params);
    bool RewindBlockIndex(const CChainParams& params) LOCKS_EXCLUDED(cs_main);
    bool LoadGenesisBlock(const CChainParams& chainparams);

    void PruneBlockIndexCandidates();

    void UnloadBlockIndex();

    /** Check whether we are doing an initial block download (synchronizing from disk or network) */
    bool IsInitialBlockDownload() const;

    /**
     * Make various assertions about the state of the block index.
     *
     * By default this only executes fully when using the Regtest chain; see: fCheckBlockIndex.
     */
    void CheckBlockIndex(const Consensus::Params& consensusParams);

    /** Load the persisted mempool from disk */
    void LoadMempool(const ArgsManager& args);

    /** Update the chain tip based on database information, i.e. CoinsTip()'s best block. */
    bool LoadChainTip(const CChainParams& chainparams) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Dictates whether we need to flush the cache to disk or not.
    //!
    //! @return the state of the size of the coins cache.
    CoinsCacheSizeState GetCoinsCacheSizeState(const CTxMemPool* tx_pool)
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

    CoinsCacheSizeState GetCoinsCacheSizeState(
        const CTxMemPool* tx_pool,
        size_t max_coins_cache_size_bytes,
        size_t max_mempool_size_bytes) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

    std::string ToString() EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

private:
    bool ActivateBestChainStep(BlockValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexMostWork, const std::shared_ptr<const CBlock>& pblock, bool& fInvalidFound, ConnectTrace& connectTrace) EXCLUSIVE_LOCKS_REQUIRED(cs_main, m_mempool.cs);
    bool ConnectTip(BlockValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexNew, const std::shared_ptr<const CBlock>& pblock, ConnectTrace& connectTrace, DisconnectedBlockTransactions& disconnectpool) EXCLUSIVE_LOCKS_REQUIRED(cs_main, m_mempool.cs);

    void InvalidBlockFound(CBlockIndex *pindex, const BlockValidationState &state) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    CBlockIndex* FindMostWorkChain() EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    void ReceivedBlockTransactions(const CBlock& block, CBlockIndex* pindexNew, const FlatFilePos& pos, const Consensus::Params& consensusParams) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    bool RollforwardBlock(const CBlockIndex* pindex, CCoinsViewCache& inputs, const CChainParams& params) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Mark a block as not having block data
    void EraseBlockData(CBlockIndex* index) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    friend ChainstateManager;
};

/** Mark a block as precious and reorganize.
 *
 * May not be called in a
 * validationinterface callback.
 */
bool PreciousBlock(BlockValidationState& state, const CChainParams& params, CBlockIndex *pindex) LOCKS_EXCLUDED(cs_main);

/** Mark a block as invalid. */
bool InvalidateBlock(BlockValidationState& state, const CChainParams& chainparams, CBlockIndex* pindex) LOCKS_EXCLUDED(cs_main);

/** Remove invalidity status from a block and its descendants. */
void ResetBlockFailureFlags(CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
/**
 * Mark a block as finalized.
 * A finalized block can not be reorged in any way.
 */
bool FinalizeBlockAndInvalidate(BlockValidationState &state, CBlockIndex *pindex) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Retrieve the topmost finalized block.
 */
const CBlockIndex *GetFinalizedBlock();  // EXCLUSIVE_LOCKS_REQUIRED(cs_main)

/**
 * Checks if a block is finalized.
 */
bool IsBlockFinalized(const CBlockIndex *pindex);  // EXCLUSIVE_LOCKS_REQUIRED(cs_main);

/**
 * Provides an interface for creating and interacting with one or two
 * chainstates: an IBD chainstate generated by downloading blocks, and
 * an optional snapshot chainstate loaded from a UTXO snapshot. Managed
 * chainstates can be maintained at different heights simultaneously.
 *
 * This class provides abstractions that allow the retrieval of the current
 * most-work chainstate ("Active") as well as chainstates which may be in
 * background use to validate UTXO snapshots.
 *
 * Definitions:
 *
 * *IBD chainstate*: a chainstate whose current state has been "fully"
 *   validated by the initial block download process.
 *
 * *Snapshot chainstate*: a chainstate populated by loading in an
 *    assumeutxo UTXO snapshot.
 *
 * *Active chainstate*: the chainstate containing the current most-work
 *    chain. Consulted by most parts of the system (net_processing,
 *    wallet) as a reflection of the current chain and UTXO set.
 *    This may either be an IBD chainstate or a snapshot chainstate.
 *
 * *Background IBD chainstate*: an IBD chainstate for which the
 *    IBD process is happening in the background while use of the
 *    active (snapshot) chainstate allows the rest of the system to function.
 *
 * *Validated chainstate*: the most-work chainstate which has been validated
 *   locally via initial block download. This will be the snapshot chainstate
 *   if a snapshot was loaded and all blocks up to the snapshot starting point
 *   have been downloaded and validated (via background validation), otherwise
 *   it will be the IBD chainstate.
 */
class ChainstateManager
{
private:
    //! The chainstate used under normal operation (i.e. "regular" IBD) or, if
    //! a snapshot is in use, for background validation.
    //!
    //! Its contents (including on-disk data) will be deleted *upon shutdown*
    //! after background validation of the snapshot has completed. We do not
    //! free the chainstate contents immediately after it finishes validation
    //! to cautiously avoid a case where some other part of the system is still
    //! using this pointer (e.g. net_processing).
    //!
    //! Once this pointer is set to a corresponding chainstate, it will not
    //! be reset until init.cpp:Shutdown(). This means it is safe to acquire
    //! the contents of this pointer with ::cs_main held, release the lock,
    //! and then use the reference without concern of it being deconstructed.
    //!
    //! This is especially important when, e.g., calling ActivateBestChain()
    //! on all chainstates because we are not able to hold ::cs_main going into
    //! that call.
    std::unique_ptr<CChainState> m_ibd_chainstate;

    //! A chainstate initialized on the basis of a UTXO snapshot. If this is
    //! non-null, it is always our active chainstate.
    //!
    //! Once this pointer is set to a corresponding chainstate, it will not
    //! be reset until init.cpp:Shutdown(). This means it is safe to acquire
    //! the contents of this pointer with ::cs_main held, release the lock,
    //! and then use the reference without concern of it being deconstructed.
    //!
    //! This is especially important when, e.g., calling ActivateBestChain()
    //! on all chainstates because we are not able to hold ::cs_main going into
    //! that call.
    std::unique_ptr<CChainState> m_snapshot_chainstate;

    //! Points to either the ibd or snapshot chainstate; indicates our
    //! most-work chain.
    //!
    //! Once this pointer is set to a corresponding chainstate, it will not
    //! be reset until init.cpp:Shutdown(). This means it is safe to acquire
    //! the contents of this pointer with ::cs_main held, release the lock,
    //! and then use the reference without concern of it being deconstructed.
    //!
    //! This is especially important when, e.g., calling ActivateBestChain()
    //! on all chainstates because we are not able to hold ::cs_main going into
    //! that call.
    CChainState* m_active_chainstate{nullptr};

    //! If true, the assumed-valid chainstate has been fully validated
    //! by the background validation chainstate.
    bool m_snapshot_validated{false};

    // For access to m_active_chainstate.
    friend CChainState& ChainstateActive();
    friend CChain& ChainActive();

public:
    //! A single BlockManager instance is shared across each constructed
    //! chainstate to avoid duplicating block metadata.
    BlockManager m_blockman GUARDED_BY(::cs_main);

    //! The total number of bytes available for us to use across all in-memory
    //! coins caches. This will be split somehow across chainstates.
    int64_t m_total_coinstip_cache{0};
    //
    //! The total number of bytes available for us to use across all leveldb
    //! coins databases. This will be split somehow across chainstates.
    int64_t m_total_coinsdb_cache{0};

    //! Instantiate a new chainstate and assign it based upon whether it is
    //! from a snapshot.
    //!
    //! @param[in] mempool              The mempool to pass to the chainstate
    //                                  constructor
    //! @param[in] snapshot_blockhash   If given, signify that this chainstate
    //!                                 is based on a snapshot.
    CChainState& InitializeChainstate(CTxMemPool& mempool, const uint256& snapshot_blockhash = uint256())
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

    //! Get all chainstates currently being used.
    std::vector<CChainState*> GetAll();

    //! The most-work chain.
    CChainState& ActiveChainstate() const;
    CChain& ActiveChain() const { return ActiveChainstate().m_chain; }
    int ActiveHeight() const { return ActiveChain().Height(); }
    CBlockIndex* ActiveTip() const { return ActiveChain().Tip(); }

    BlockMap& BlockIndex() EXCLUSIVE_LOCKS_REQUIRED(::cs_main)
    {
        return m_blockman.m_block_index;
    }

    bool IsSnapshotActive() const;

    Optional<uint256> SnapshotBlockhash() const;

    //! Is there a snapshot in use and has it been fully validated?
    bool IsSnapshotValidated() const { return m_snapshot_validated; }

    //! @returns true if this chainstate is being used to validate an active
    //!          snapshot in the background.
    bool IsBackgroundIBD(CChainState* chainstate) const;

    //! Return the most-work chainstate that has been fully validated.
    //!
    //! During background validation of a snapshot, this is the IBD chain. After
    //! background validation has completed, this is the snapshot chain.
    CChainState& ValidatedChainstate() const;

    CChain& ValidatedChain() const { return ValidatedChainstate().m_chain; }
    CBlockIndex* ValidatedTip() const { return ValidatedChain().Tip(); }

    /**
     * Process an incoming block. This only returns after the best known valid
     * block is made active. Note that it does not, however, guarantee that the
     * specific block passed to it has been checked for validity!
     *
     * If you want to *possibly* get feedback on whether pblock is valid, you must
     * install a CValidationInterface (see validationinterface.h) - this will have
     * its BlockChecked method called whenever *any* block completes validation.
     *
     * Note that we guarantee that either the proof-of-work is valid on pblock, or
     * (and possibly also) BlockChecked will have been called.
     *
     * May not be called in a
     * validationinterface callback.
     *
     * @param[in]   pblock  The block we want to process.
     * @param[in]   fForceProcessing Process this block even if unrequested; used for non-network block sources.
     * @param[out]  fNewBlock A boolean which is set to indicate if the block was first received via this call
     * @returns     If the block was processed, independently of block validity
     */
    bool ProcessNewBlock(const CChainParams& chainparams, const std::shared_ptr<const CBlock> pblock, bool fForceProcessing, bool* fNewBlock) LOCKS_EXCLUDED(cs_main);

    /**
     * Process incoming block headers.
     *
     * May not be called in a
     * validationinterface callback.
     *
     * @param[in]  block The block headers themselves
     * @param[out] state This may be set to an Error state if any error occurred processing them
     * @param[in]  chainparams The params for the chain we want to connect to
     * @param[out] ppindex If set, the pointer will be set to point to the last new block index object for the given headers
     */
    bool ProcessNewBlockHeaders(const std::vector<CBlockHeader>& block, BlockValidationState& state, const CChainParams& chainparams, const CBlockIndex** ppindex = nullptr) LOCKS_EXCLUDED(cs_main);

    //! Load the block tree and coins database from disk, initializing state if we're running with -reindex
    bool LoadBlockIndex(const CChainParams& chainparams) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    //! Unload block index and chain data before shutdown.
    void Unload() EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

    //! Clear (deconstruct) chainstate data.
    void Reset();

    //! Check to see if caches are out of balance and if so, call
    //! ResizeCoinsCaches() as needed.
    void MaybeRebalanceCaches() EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
};

/** DEPRECATED! Please use node.chainman instead. May only be used in validation.cpp internally */
extern ChainstateManager g_chainman GUARDED_BY(::cs_main);

/** Please prefer the identical ChainstateManager::ActiveChainstate */
CChainState& ChainstateActive();

/** Please prefer the identical ChainstateManager::ActiveChain */
CChain& ChainActive();

/** Global variable that points to the active block tree (protected by cs_main) */
extern std::unique_ptr<CBlockTreeDB> pblocktree;

/**
 * Return the spend height, which is one more than the inputs.GetBestBlock().
 * While checking, GetBestBlock() refers to the parent block. (protected by cs_main)
 * This is also true for mempool checks.
 */
int GetSpendHeight(const CCoinsViewCache& inputs);

extern VersionBitsCache versionbitscache;

/**
 * Determine what nVersion a new block should use.
 */
int32_t ComputeBlockVersion(const CBlockIndex* pindexPrev, const Consensus::Params& params);


/** Get block file info entry for one block file */
CBlockFileInfo* GetBlockFileInfo(size_t n);

/** Dump the mempool to disk. */
bool DumpMempool(const CTxMemPool& pool);

/** Load the mempool from disk. */
bool LoadMempool(CTxMemPool& pool);

//! Check whether the block associated with this index entry is pruned or not.
inline bool IsBlockPruned(const CBlockIndex* pblockindex)
{
    return (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0);
}

bool IsBTGHardForkEnabledForCurrentBlock(const Consensus::Params& params);

#endif // BITCOIN_VALIDATION_H
