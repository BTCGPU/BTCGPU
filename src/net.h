// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_H
#define BITCOIN_NET_H

#include <addrdb.h>
#include <addrman.h>
#include <amount.h>
#include <bloom.h>
#include <chainparams.h>
#include <compat.h>
#include <crypto/siphash.h>
#include <hash.h>
#include <net_permissions.h>
#include <netaddress.h>
#include <optional.h>
#include <policy/feerate.h>
#include <protocol.h>
#include <random.h>
#include <streams.h>
#include <sync.h>
#include <threadinterrupt.h>
#include <uint256.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <thread>
#include <memory>
#include <condition_variable>

#ifndef WIN32
#include <arpa/inet.h>
#endif


class CScheduler;
class CNode;
class BanMan;
struct bilingual_str;

/** Default for -whitelistrelay. */
static const bool DEFAULT_WHITELISTRELAY = true;
/** Default for -whitelistforcerelay. */
static const bool DEFAULT_WHITELISTFORCERELAY = false;

/** Time after which to disconnect, after waiting for a ping response (or inactivity). */
static const int TIMEOUT_INTERVAL = 20 * 60;
/** Run the feeler connection loop once every 2 minutes or 120 seconds. **/
static const int FEELER_INTERVAL = 120;
/** The maximum number of addresses from our addrman to return in response to a getaddr message. */
static constexpr size_t MAX_ADDR_TO_SEND = 1000;
/** Maximum length of incoming protocol messages (no message over 4 MB is currently acceptable). */
static const unsigned int MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000;
/** Maximum length of the user agent string in `version` message */
static const unsigned int MAX_SUBVERSION_LENGTH = 256;
/** Maximum number of automatic outgoing nodes over which we'll relay everything (blocks, tx, addrs, etc) */
static const int MAX_OUTBOUND_FULL_RELAY_CONNECTIONS = 8;
/** Maximum number of addnode outgoing nodes */
static const int MAX_ADDNODE_CONNECTIONS = 8;
/** Maximum number of block-relay-only outgoing connections */
static const int MAX_BLOCK_RELAY_ONLY_CONNECTIONS = 2;
/** Maximum number of feeler connections */
static const int MAX_FEELER_CONNECTIONS = 1;
/** -listen default */
static const bool DEFAULT_LISTEN = true;
/** -upnp default */
#ifdef USE_UPNP
static const bool DEFAULT_UPNP = USE_UPNP;
#else
static const bool DEFAULT_UPNP = false;
#endif
/** The maximum number of peer connections to maintain. */
static const unsigned int DEFAULT_MAX_PEER_CONNECTIONS = 125;
/** The default for -maxuploadtarget. 0 = Unlimited */
static const uint64_t DEFAULT_MAX_UPLOAD_TARGET = 0;
/** The default timeframe for -maxuploadtarget. 1 day. */
static const uint64_t MAX_UPLOAD_TIMEFRAME = 60 * 60 * 24;
/** Default for blocks only*/
static const bool DEFAULT_BLOCKSONLY = false;
/** -peertimeout default */
static const int64_t DEFAULT_PEER_CONNECT_TIMEOUT = 60;

static const bool DEFAULT_FORCEDNSSEED = false;
static const size_t DEFAULT_MAXRECEIVEBUFFER = 5 * 1000;
static const size_t DEFAULT_MAXSENDBUFFER    = 1 * 1000;

typedef int64_t NodeId;

struct AddedNodeInfo
{
    std::string strAddedNode;
    CService resolvedAddress;
    bool fConnected;
    bool fInbound;
};

class CNodeStats;
class CClientUIInterface;

struct CSerializedNetMsg
{
    CSerializedNetMsg() = default;
    CSerializedNetMsg(CSerializedNetMsg&&) = default;
    CSerializedNetMsg& operator=(CSerializedNetMsg&&) = default;
    // No copying, only moves.
    CSerializedNetMsg(const CSerializedNetMsg& msg) = delete;
    CSerializedNetMsg& operator=(const CSerializedNetMsg&) = delete;

    std::vector<unsigned char> data;
    std::string m_type;
};

/** Different types of connections to a peer. This enum encapsulates the
 * information we have available at the time of opening or accepting the
 * connection. Aside from INBOUND, all types are initiated by us.
 *
 * If adding or removing types, please update CONNECTION_TYPE_DOC in
 * src/rpc/net.cpp. */
enum class ConnectionType {
    /**
     * Inbound connections are those initiated by a peer. This is the only
     * property we know at the time of connection, until P2P messages are
     * exchanged.
     */
    INBOUND,

    /**
     * These are the default connections that we use to connect with the
     * network. There is no restriction on what is relayed- by default we relay
     * blocks, addresses & transactions. We automatically attempt to open
     * MAX_OUTBOUND_FULL_RELAY_CONNECTIONS using addresses from our AddrMan.
     */
    OUTBOUND_FULL_RELAY,


    /**
     * We open manual connections to addresses that users explicitly inputted
     * via the addnode RPC, or the -connect command line argument. Even if a
     * manual connection is misbehaving, we do not automatically disconnect or
     * add it to our discouragement filter.
     */
    MANUAL,

    /**
     * Feeler connections are short-lived connections made to check that a node
     * is alive. They can be useful for:
     * - test-before-evict: if one of the peers is considered for eviction from
     *   our AddrMan because another peer is mapped to the same slot in the tried table,
     *   evict only if this longer-known peer is offline.
     * - move node addresses from New to Tried table, so that we have more
     *   connectable addresses in our AddrMan.
     * Note that in the literature ("Eclipse Attacks on Bitcoin’s Peer-to-Peer Network")
     * only the latter feature is referred to as "feeler connections",
     * although in our codebase feeler connections encompass test-before-evict as well.
     * We make these connections approximately every FEELER_INTERVAL:
     * first we resolve previously found collisions if they exist (test-before-evict),
     * otherwise connect to a node from the new table.
     */
    FEELER,

    /**
     * We use block-relay-only connections to help prevent against partition
     * attacks. By not relaying transactions or addresses, these connections
     * are harder to detect by a third party, thus helping obfuscate the
     * network topology. We automatically attempt to open
     * MAX_BLOCK_RELAY_ONLY_ANCHORS using addresses from our anchors.dat. Then
     * addresses from our AddrMan if MAX_BLOCK_RELAY_ONLY_CONNECTIONS
     * isn't reached yet.
     */
    BLOCK_RELAY,

    /**
     * AddrFetch connections are short lived connections used to solicit
     * addresses from peers. These are initiated to addresses submitted via the
     * -seednode command line argument, or under certain conditions when the
     * AddrMan is empty.
     */
    ADDR_FETCH,
};

class NetEventsInterface;
class CConnman
{
public:

    enum NumConnections {
        CONNECTIONS_NONE = 0,
        CONNECTIONS_IN = (1U << 0),
        CONNECTIONS_OUT = (1U << 1),
        CONNECTIONS_ALL = (CONNECTIONS_IN | CONNECTIONS_OUT),
    };

    struct Options
    {
        ServiceFlags nLocalServices = NODE_NONE;
        int nMaxConnections = 0;
        int m_max_outbound_full_relay = 0;
        int m_max_outbound_block_relay = 0;
        int nMaxAddnode = 0;
        int nMaxFeeler = 0;
        int nBestHeight = 0;
        CClientUIInterface* uiInterface = nullptr;
        NetEventsInterface* m_msgproc = nullptr;
        BanMan* m_banman = nullptr;
        unsigned int nSendBufferMaxSize = 0;
        unsigned int nReceiveFloodSize = 0;
        uint64_t nMaxOutboundTimeframe = 0;
        uint64_t nMaxOutboundLimit = 0;
        int64_t m_peer_connect_timeout = DEFAULT_PEER_CONNECT_TIMEOUT;
        std::vector<std::string> vSeedNodes;
        std::vector<NetWhitelistPermissions> vWhitelistedRange;
        std::vector<NetWhitebindPermissions> vWhiteBinds;
        std::vector<CService> vBinds;
        std::vector<CService> onion_binds;
        bool m_use_addrman_outgoing = true;
        std::vector<std::string> m_specified_outgoing;
        std::vector<std::string> m_added_nodes;
        std::vector<bool> m_asmap;
    };

    void Init(const Options& connOptions) {
        nLocalServices = connOptions.nLocalServices;
        nMaxConnections = connOptions.nMaxConnections;
        m_max_outbound_full_relay = std::min(connOptions.m_max_outbound_full_relay, connOptions.nMaxConnections);
        m_max_outbound_block_relay = connOptions.m_max_outbound_block_relay;
        m_use_addrman_outgoing = connOptions.m_use_addrman_outgoing;
        nMaxAddnode = connOptions.nMaxAddnode;
        nMaxFeeler = connOptions.nMaxFeeler;
        m_max_outbound = m_max_outbound_full_relay + m_max_outbound_block_relay + nMaxFeeler;
        nBestHeight = connOptions.nBestHeight;
        clientInterface = connOptions.uiInterface;
        m_banman = connOptions.m_banman;
        m_msgproc = connOptions.m_msgproc;
        nSendBufferMaxSize = connOptions.nSendBufferMaxSize;
        nReceiveFloodSize = connOptions.nReceiveFloodSize;
        m_peer_connect_timeout = connOptions.m_peer_connect_timeout;
        {
            LOCK(cs_totalBytesSent);
            nMaxOutboundTimeframe = connOptions.nMaxOutboundTimeframe;
            nMaxOutboundLimit = connOptions.nMaxOutboundLimit;
        }
        vWhitelistedRange = connOptions.vWhitelistedRange;
        {
            LOCK(cs_vAddedNodes);
            vAddedNodes = connOptions.m_added_nodes;
        }
        m_onion_binds = connOptions.onion_binds;
    }

    CConnman(uint64_t seed0, uint64_t seed1, bool network_active = true);
    ~CConnman();
    bool Start(CScheduler& scheduler, const Options& options);

    void StopThreads();
    void StopNodes();
    void Stop()
    {
        StopThreads();
        StopNodes();
    };

    void Interrupt();
    bool GetNetworkActive() const { return fNetworkActive; };
    bool GetUseAddrmanOutgoing() const { return m_use_addrman_outgoing; };
    void SetNetworkActive(bool active);
    void OpenNetworkConnection(const CAddress& addrConnect, bool fCountFailure, CSemaphoreGrant* grantOutbound, const char* strDest, ConnectionType conn_type);
    bool CheckIncomingNonce(uint64_t nonce);

    bool ForNode(NodeId id, std::function<bool(CNode* pnode)> func);

    void PushMessage(CNode* pnode, CSerializedNetMsg&& msg);

    using NodeFn = std::function<void(CNode*)>;
    void ForEachNode(const NodeFn& func)
    {
        LOCK(cs_vNodes);
        for (auto&& node : vNodes) {
            if (NodeFullyConnected(node))
                func(node);
        }
    };

    void ForEachNode(const NodeFn& func) const
    {
        LOCK(cs_vNodes);
        for (auto&& node : vNodes) {
            if (NodeFullyConnected(node))
                func(node);
        }
    };

    template<typename Callable, typename CallableAfter>
    void ForEachNodeThen(Callable&& pre, CallableAfter&& post)
    {
        LOCK(cs_vNodes);
        for (auto&& node : vNodes) {
            if (NodeFullyConnected(node))
                pre(node);
        }
        post();
    };

    template<typename Callable, typename CallableAfter>
    void ForEachNodeThen(Callable&& pre, CallableAfter&& post) const
    {
        LOCK(cs_vNodes);
        for (auto&& node : vNodes) {
            if (NodeFullyConnected(node))
                pre(node);
        }
        post();
    };

    // Addrman functions
    void SetServices(const CService &addr, ServiceFlags nServices);
    void MarkAddressGood(const CAddress& addr);
    bool AddNewAddresses(const std::vector<CAddress>& vAddr, const CAddress& addrFrom, int64_t nTimePenalty = 0);
    std::vector<CAddress> GetAddresses(size_t max_addresses, size_t max_pct);
    /**
     * Cache is used to minimize topology leaks, so it should
     * be used for all non-trusted calls, for example, p2p.
     * A non-malicious call (from RPC or a peer with addr permission) should
     * call the function without a parameter to avoid using the cache.
     */
    std::vector<CAddress> GetAddresses(CNode& requestor, size_t max_addresses, size_t max_pct);

    // This allows temporarily exceeding m_max_outbound_full_relay, with the goal of finding
    // a peer that is better than all our current peers.
    void SetTryNewOutboundPeer(bool flag);
    bool GetTryNewOutboundPeer();

    // Return the number of outbound peers we have in excess of our target (eg,
    // if we previously called SetTryNewOutboundPeer(true), and have since set
    // to false, we may have extra peers that we wish to disconnect). This may
    // return a value less than (num_outbound_connections - num_outbound_slots)
    // in cases where some outbound connections are not yet fully connected, or
    // not yet fully disconnected.
    int GetExtraOutboundCount();

    bool AddNode(const std::string& node);
    bool RemoveAddedNode(const std::string& node);
    std::vector<AddedNodeInfo> GetAddedNodeInfo();

    size_t GetNodeCount(NumConnections num);
    void GetNodeStats(std::vector<CNodeStats>& vstats);
    bool DisconnectNode(const std::string& node);
    bool DisconnectNode(const CSubNet& subnet);
    bool DisconnectNode(const CNetAddr& addr);
    bool DisconnectNode(NodeId id);

    //! Used to convey which local services we are offering peers during node
    //! connection.
    //!
    //! The data returned by this is used in CNode construction,
    //! which is used to advertise which services we are offering
    //! that peer during `net_processing.cpp:PushNodeVersion()`.
    ServiceFlags GetLocalServices() const;

    //!set the max outbound target in bytes
    void SetMaxOutboundTarget(uint64_t limit);
    uint64_t GetMaxOutboundTarget();

    //!set the timeframe for the max outbound target
    void SetMaxOutboundTimeframe(uint64_t timeframe);
    uint64_t GetMaxOutboundTimeframe();

    //! check if the outbound target is reached
    //! if param historicalBlockServingLimit is set true, the function will
    //! response true if the limit for serving historical blocks has been reached
    bool OutboundTargetReached(bool historicalBlockServingLimit);

    //! response the bytes left in the current max outbound cycle
    //! in case of no limit, it will always response 0
    uint64_t GetOutboundTargetBytesLeft();

    //! response the time in second left in the current max outbound cycle
    //! in case of no limit, it will always response 0
    uint64_t GetMaxOutboundTimeLeftInCycle();

    uint64_t GetTotalBytesRecv();
    uint64_t GetTotalBytesSent();

    void SetBestHeight(int height);
    int GetBestHeight() const;

    /** Get a unique deterministic randomizer. */
    CSipHasher GetDeterministicRandomizer(uint64_t id) const;

    unsigned int GetReceiveFloodSize() const;

    void WakeMessageHandler();

    /** Attempts to obfuscate tx time through exponentially distributed emitting.
        Works assuming that a single interval is used.
        Variable intervals will result in privacy decrease.
    */
    int64_t PoissonNextSendInbound(int64_t now, int average_interval_seconds);

    void SetAsmap(std::vector<bool> asmap) { addrman.m_asmap = std::move(asmap); }

private:
    struct ListenSocket {
    public:
        SOCKET socket;
        inline void AddSocketPermissionFlags(NetPermissionFlags& flags) const { NetPermissions::AddFlag(flags, m_permissions); }
        ListenSocket(SOCKET socket_, NetPermissionFlags permissions_) : socket(socket_), m_permissions(permissions_) {}
    private:
        NetPermissionFlags m_permissions;
    };

    bool BindListenPort(const CService& bindAddr, bilingual_str& strError, NetPermissionFlags permissions);
    bool Bind(const CService& addr, unsigned int flags, NetPermissionFlags permissions);
    bool InitBinds(
        const std::vector<CService>& binds,
        const std::vector<NetWhitebindPermissions>& whiteBinds,
        const std::vector<CService>& onion_binds);

    void ThreadOpenAddedConnections();
    void AddAddrFetch(const std::string& strDest);
    void ProcessAddrFetch();
    void ThreadOpenConnections(std::vector<std::string> connect);
    void ThreadMessageHandler();
    void AcceptConnection(const ListenSocket& hListenSocket);
    void DisconnectNodes();
    void NotifyNumConnectionsChanged();
    void InactivityCheck(CNode *pnode);
    bool GenerateSelectSet(std::set<SOCKET> &recv_set, std::set<SOCKET> &send_set, std::set<SOCKET> &error_set);
    void SocketEvents(std::set<SOCKET> &recv_set, std::set<SOCKET> &send_set, std::set<SOCKET> &error_set);
    void SocketHandler();
    void ThreadSocketHandler();
    void ThreadDNSAddressSeed();

    uint64_t CalculateKeyedNetGroup(const CAddress& ad) const;

    CNode* FindNode(const CNetAddr& ip);
    CNode* FindNode(const CSubNet& subNet);
    CNode* FindNode(const std::string& addrName);
    CNode* FindNode(const CService& addr);

    /**
     * Determine whether we're already connected to a given address, in order to
     * avoid initiating duplicate connections.
     */
    bool AlreadyConnectedToAddress(const CAddress& addr);

    bool AttemptToEvictConnection();
    CNode* ConnectNode(CAddress addrConnect, const char *pszDest, bool fCountFailure, ConnectionType conn_type);
    void AddWhitelistPermissionFlags(NetPermissionFlags& flags, const CNetAddr &addr) const;

    void DeleteNode(CNode* pnode);

    NodeId GetNewNodeId();

    size_t SocketSendData(CNode *pnode) const;
    void DumpAddresses();

    // Network stats
    void RecordBytesRecv(uint64_t bytes);
    void RecordBytesSent(uint64_t bytes);

    /**
     * Return vector of current BLOCK_RELAY peers.
     */
    std::vector<CAddress> GetCurrentBlockRelayOnlyConns() const;

    // Whether the node should be passed out in ForEach* callbacks
    static bool NodeFullyConnected(const CNode* pnode);

    // Network usage totals
    RecursiveMutex cs_totalBytesRecv;
    RecursiveMutex cs_totalBytesSent;
    uint64_t nTotalBytesRecv GUARDED_BY(cs_totalBytesRecv) {0};
    uint64_t nTotalBytesSent GUARDED_BY(cs_totalBytesSent) {0};

    // outbound limit & stats
    uint64_t nMaxOutboundTotalBytesSentInCycle GUARDED_BY(cs_totalBytesSent);
    uint64_t nMaxOutboundCycleStartTime GUARDED_BY(cs_totalBytesSent);
    uint64_t nMaxOutboundLimit GUARDED_BY(cs_totalBytesSent);
    uint64_t nMaxOutboundTimeframe GUARDED_BY(cs_totalBytesSent);

    // P2P timeout in seconds
    int64_t m_peer_connect_timeout;

    // Whitelisted ranges. Any node connecting from these is automatically
    // whitelisted (as well as those connecting to whitelisted binds).
    std::vector<NetWhitelistPermissions> vWhitelistedRange;

    unsigned int nSendBufferMaxSize{0};
    unsigned int nReceiveFloodSize{0};

    std::vector<ListenSocket> vhListenSocket;
    std::atomic<bool> fNetworkActive{true};
    bool fAddressesInitialized{false};
    CAddrMan addrman;
    std::deque<std::string> m_addr_fetches GUARDED_BY(m_addr_fetches_mutex);
    RecursiveMutex m_addr_fetches_mutex;
    std::vector<std::string> vAddedNodes GUARDED_BY(cs_vAddedNodes);
    RecursiveMutex cs_vAddedNodes;
    std::vector<CNode*> vNodes GUARDED_BY(cs_vNodes);
    std::list<CNode*> vNodesDisconnected;
    mutable RecursiveMutex cs_vNodes;
    std::atomic<NodeId> nLastNodeId{0};
    unsigned int nPrevNodeCount{0};

    /**
     * Cache responses to addr requests to minimize privacy leak.
     * Attack example: scraping addrs in real-time may allow an attacker
     * to infer new connections of the victim by detecting new records
     * with fresh timestamps (per self-announcement).
     */
    struct CachedAddrResponse {
        std::vector<CAddress> m_addrs_response_cache;
        std::chrono::microseconds m_cache_entry_expiration{0};
    };

    /**
     * Addr responses stored in different caches
     * per (network, local socket) prevent cross-network node identification.
     * If a node for example is multi-homed under Tor and IPv6,
     * a single cache (or no cache at all) would let an attacker
     * to easily detect that it is the same node by comparing responses.
     * Indexing by local socket prevents leakage when a node has multiple
     * listening addresses on the same network.
     *
     * The used memory equals to 1000 CAddress records (or around 40 bytes) per
     * distinct Network (up to 5) we have/had an inbound peer from,
     * resulting in at most ~196 KB. Every separate local socket may
     * add up to ~196 KB extra.
     */
    std::map<uint64_t, CachedAddrResponse> m_addr_response_caches;

    /**
     * Services this instance offers.
     *
     * This data is replicated in each CNode instance we create during peer
     * connection (in ConnectNode()) under a member also called
     * nLocalServices.
     *
     * This data is not marked const, but after being set it should not
     * change. See the note in CNode::nLocalServices documentation.
     *
     * \sa CNode::nLocalServices
     */
    ServiceFlags nLocalServices;

    std::unique_ptr<CSemaphore> semOutbound;
    std::unique_ptr<CSemaphore> semAddnode;
    int nMaxConnections;

    // How many full-relay (tx, block, addr) outbound peers we want
    int m_max_outbound_full_relay;

    // How many block-relay only outbound peers we want
    // We do not relay tx or addr messages with these peers
    int m_max_outbound_block_relay;

    int nMaxAddnode;
    int nMaxFeeler;
    int m_max_outbound;
    bool m_use_addrman_outgoing;
    std::atomic<int> nBestHeight;
    CClientUIInterface* clientInterface;
    NetEventsInterface* m_msgproc;
    /** Pointer to this node's banman. May be nullptr - check existence before dereferencing. */
    BanMan* m_banman;

    /**
     * Addresses that were saved during the previous clean shutdown. We'll
     * attempt to make block-relay-only connections to them.
     */
    std::vector<CAddress> m_anchors;

    /** SipHasher seeds for deterministic randomness */
    const uint64_t nSeed0, nSeed1;

    /** flag for waking the message processor. */
    bool fMsgProcWake GUARDED_BY(mutexMsgProc);

    std::condition_variable condMsgProc;
    Mutex mutexMsgProc;
    std::atomic<bool> flagInterruptMsgProc{false};

    CThreadInterrupt interruptNet;

    std::thread threadDNSAddressSeed;
    std::thread threadSocketHandler;
    std::thread threadOpenAddedConnections;
    std::thread threadOpenConnections;
    std::thread threadMessageHandler;

    /** flag for deciding to connect to an extra outbound peer,
     *  in excess of m_max_outbound_full_relay
     *  This takes the place of a feeler connection */
    std::atomic_bool m_try_another_outbound_peer;

    std::atomic<int64_t> m_next_send_inv_to_incoming{0};

    /**
     * A vector of -bind=<address>:<port>=onion arguments each of which is
     * an address and port that are designated for incoming Tor connections.
     */
    std::vector<CService> m_onion_binds;

    friend struct CConnmanTest;
    friend struct ConnmanTestMsg;
};
void Discover();
void StartMapPort();
void InterruptMapPort();
void StopMapPort();
uint16_t GetListenPort();

/**
 * Interface for message handling
 */
class NetEventsInterface
{
public:
    virtual bool ProcessMessages(CNode* pnode, std::atomic<bool>& interrupt) = 0;
    virtual bool SendMessages(CNode* pnode) = 0;
    virtual void InitializeNode(CNode* pnode) = 0;
    virtual void FinalizeNode(const CNode& node, bool& update_connection_time) = 0;

protected:
    /**
     * Protected destructor so that instances can only be deleted by derived classes.
     * If that restriction is no longer desired, this should be made public and virtual.
     */
    ~NetEventsInterface() = default;
};

enum
{
    LOCAL_NONE,   // unknown
    LOCAL_IF,     // address a local interface listens on
    LOCAL_BIND,   // address explicit bound to
    LOCAL_UPNP,   // address reported by UPnP
    LOCAL_MANUAL, // address explicitly specified (-externalip=)

    LOCAL_MAX
};

bool IsPeerAddrLocalGood(CNode *pnode);
void AdvertiseLocal(CNode *pnode);

/**
 * Mark a network as reachable or unreachable (no automatic connects to it)
 * @note Networks are reachable by default
 */
void SetReachable(enum Network net, bool reachable);
/** @returns true if the network is reachable, false otherwise */
bool IsReachable(enum Network net);
/** @returns true if the address is in a reachable network, false otherwise */
bool IsReachable(const CNetAddr& addr);

bool AddLocal(const CService& addr, int nScore = LOCAL_NONE);
bool AddLocal(const CNetAddr& addr, int nScore = LOCAL_NONE);
void RemoveLocal(const CService& addr);
bool SeenLocal(const CService& addr);
bool IsLocal(const CService& addr);
bool GetLocal(CService &addr, const CNetAddr *paddrPeer = nullptr);
CAddress GetLocalAddress(const CNetAddr *paddrPeer, ServiceFlags nLocalServices);


extern bool fDiscover;
extern bool fListen;
extern bool g_relay_txes;

/** Subversion as sent to the P2P network in `version` messages */
extern std::string strSubVersion;

struct LocalServiceInfo {
    int nScore;
    int nPort;
};

extern RecursiveMutex cs_mapLocalHost;
extern std::map<CNetAddr, LocalServiceInfo> mapLocalHost GUARDED_BY(cs_mapLocalHost);

extern const std::string NET_MESSAGE_COMMAND_OTHER;
typedef std::map<std::string, uint64_t> mapMsgCmdSize; //command, total bytes

class CNodeStats
{
public:
    NodeId nodeid;
    ServiceFlags nServices;
    bool fRelayTxes;
    int64_t nLastSend;
    int64_t nLastRecv;
    int64_t nLastTXTime;
    int64_t nLastBlockTime;
    int64_t nTimeConnected;
    int64_t nTimeOffset;
    std::string addrName;
    int nVersion;
    std::string cleanSubVer;
    bool fInbound;
    bool m_manual_connection;
    int nStartingHeight;
    uint64_t nSendBytes;
    mapMsgCmdSize mapSendBytesPerMsgCmd;
    uint64_t nRecvBytes;
    mapMsgCmdSize mapRecvBytesPerMsgCmd;
    NetPermissionFlags m_permissionFlags;
    bool m_legacyWhitelisted;
    int64_t m_ping_usec;
    int64_t m_ping_wait_usec;
    int64_t m_min_ping_usec;
    CAmount minFeeFilter;
    // Our address, as reported by the peer
    std::string addrLocal;
    // Address of this peer
    CAddress addr;
    // Bind address of our side of the connection
    CAddress addrBind;
    // Name of the network the peer connected through
    std::string m_network;
    uint32_t m_mapped_as;
    std::string m_conn_type_string;
};



/** Transport protocol agnostic message container.
 * Ideally it should only contain receive time, payload,
 * command and size.
 */
class CNetMessage {
public:
    CDataStream m_recv;                  //!< received message data
    std::chrono::microseconds m_time{0}; //!< time of message receipt
    uint32_t m_message_size{0};          //!< size of the payload
    uint32_t m_raw_message_size{0};      //!< used wire size of the message (including header/checksum)
    std::string m_command;

    CNetMessage(CDataStream&& recv_in) : m_recv(std::move(recv_in)) {}

    void SetVersion(int nVersionIn)
    {
        m_recv.SetVersion(nVersionIn);
    }
};

/** The TransportDeserializer takes care of holding and deserializing the
 * network receive buffer. It can deserialize the network buffer into a
 * transport protocol agnostic CNetMessage (command & payload)
 */
class TransportDeserializer {
public:
    // returns true if the current deserialization is complete
    virtual bool Complete() const = 0;
    // set the serialization context version
    virtual void SetVersion(int version) = 0;
    // read and deserialize data
    virtual int Read(const char *data, unsigned int bytes) = 0;
    // decomposes a message from the context
    virtual Optional<CNetMessage> GetMessage(std::chrono::microseconds time, uint32_t& out_err) = 0;
    virtual ~TransportDeserializer() {}
};

class V1TransportDeserializer final : public TransportDeserializer
{
private:
    const CChainParams& m_chain_params;
    const NodeId m_node_id; // Only for logging
    mutable CHash256 hasher;
    mutable uint256 data_hash;
    bool in_data;                   // parsing header (false) or data (true)
    CDataStream hdrbuf;             // partially received header
    CMessageHeader hdr;             // complete header
    CDataStream vRecv;              // received message data
    unsigned int nHdrPos;
    unsigned int nDataPos;

    const uint256& GetMessageHash() const;
    int readHeader(const char *pch, unsigned int nBytes);
    int readData(const char *pch, unsigned int nBytes);

    void Reset() {
        vRecv.clear();
        hdrbuf.clear();
        hdrbuf.resize(24);
        in_data = false;
        nHdrPos = 0;
        nDataPos = 0;
        data_hash.SetNull();
        hasher.Reset();
    }

public:
    V1TransportDeserializer(const CChainParams& chain_params, const NodeId node_id, int nTypeIn, int nVersionIn)
        : m_chain_params(chain_params),
          m_node_id(node_id),
          hdrbuf(nTypeIn, nVersionIn),
          vRecv(nTypeIn, nVersionIn)
    {
        Reset();
    }

    bool Complete() const override
    {
        if (!in_data)
            return false;
        return (hdr.nMessageSize == nDataPos);
    }
    void SetVersion(int nVersionIn) override
    {
        hdrbuf.SetVersion(nVersionIn);
        vRecv.SetVersion(nVersionIn);
    }
    int Read(const char *pch, unsigned int nBytes) override {
        int ret = in_data ? readData(pch, nBytes) : readHeader(pch, nBytes);
        if (ret < 0) Reset();
        return ret;
    }
    Optional<CNetMessage> GetMessage(std::chrono::microseconds time, uint32_t& out_err_raw_size) override;
};

/** The TransportSerializer prepares messages for the network transport
 */
class TransportSerializer {
public:
    // prepare message for transport (header construction, error-correction computation, payload encryption, etc.)
    virtual void prepareForTransport(CSerializedNetMsg& msg, std::vector<unsigned char>& header) = 0;
    virtual ~TransportSerializer() {}
};

class V1TransportSerializer  : public TransportSerializer {
public:
    void prepareForTransport(CSerializedNetMsg& msg, std::vector<unsigned char>& header) override;
};

/** Information about a peer */
class CNode
{
    friend class CConnman;
    friend struct ConnmanTestMsg;

public:
    std::unique_ptr<TransportDeserializer> m_deserializer;
    std::unique_ptr<TransportSerializer> m_serializer;

    // socket
    std::atomic<ServiceFlags> nServices{NODE_NONE};
    SOCKET hSocket GUARDED_BY(cs_hSocket);
    size_t nSendSize{0}; // total size of all vSendMsg entries
    size_t nSendOffset{0}; // offset inside the first vSendMsg already sent
    uint64_t nSendBytes GUARDED_BY(cs_vSend){0};
    std::deque<std::vector<unsigned char>> vSendMsg GUARDED_BY(cs_vSend);
    RecursiveMutex cs_vSend;
    RecursiveMutex cs_hSocket;
    RecursiveMutex cs_vRecv;

    RecursiveMutex cs_vProcessMsg;
    std::list<CNetMessage> vProcessMsg GUARDED_BY(cs_vProcessMsg);
    size_t nProcessQueueSize{0};

    RecursiveMutex cs_sendProcessing;

    uint64_t nRecvBytes GUARDED_BY(cs_vRecv){0};

    std::atomic<int64_t> nLastSend{0};
    std::atomic<int64_t> nLastRecv{0};
    const int64_t nTimeConnected;
    std::atomic<int64_t> nTimeOffset{0};
    // Address of this peer
    const CAddress addr;
    // Bind address of our side of the connection
    const CAddress addrBind;
    std::atomic<int> nVersion{0};
    RecursiveMutex cs_SubVer;
    /**
     * cleanSubVer is a sanitized string of the user agent byte array we read
     * from the wire. This cleaned string can safely be logged or displayed.
     */
    std::string cleanSubVer GUARDED_BY(cs_SubVer){};
    bool m_prefer_evict{false}; // This peer is preferred for eviction.
    bool HasPermission(NetPermissionFlags permission) const {
        return NetPermissions::HasFlag(m_permissionFlags, permission);
    }
    // This boolean is unusued in actual processing, only present for backward compatibility at RPC/QT level
    bool m_legacyWhitelisted{false};
    bool fClient{false}; // set by version message
    bool m_limited_node{false}; //after BIP159, set by version message
    /**
     * Whether the peer has signaled support for receiving ADDRv2 (BIP155)
     * messages, implying a preference to receive ADDRv2 instead of ADDR ones.
     */
    std::atomic_bool m_wants_addrv2{false};
    std::atomic_bool fSuccessfullyConnected{false};
    // Setting fDisconnect to true will cause the node to be disconnected the
    // next time DisconnectNodes() runs
    std::atomic_bool fDisconnect{false};
    bool fSentAddr{false};
    CSemaphoreGrant grantOutbound;
    std::atomic<int> nRefCount{0};

    const uint64_t nKeyedNetGroup;
    std::atomic_bool fPauseRecv{false};
    std::atomic_bool fPauseSend{false};

    bool IsOutboundOrBlockRelayConn() const {
        switch (m_conn_type) {
            case ConnectionType::OUTBOUND_FULL_RELAY:
            case ConnectionType::BLOCK_RELAY:
                return true;
            case ConnectionType::INBOUND:
            case ConnectionType::MANUAL:
            case ConnectionType::ADDR_FETCH:
            case ConnectionType::FEELER:
                return false;
        } // no default case, so the compiler can warn about missing cases

        assert(false);
    }

    bool IsFullOutboundConn() const {
        return m_conn_type == ConnectionType::OUTBOUND_FULL_RELAY;
    }

    bool IsManualConn() const {
        return m_conn_type == ConnectionType::MANUAL;
    }

    bool IsBlockOnlyConn() const {
        return m_conn_type == ConnectionType::BLOCK_RELAY;
    }

    bool IsFeelerConn() const {
        return m_conn_type == ConnectionType::FEELER;
    }

    bool IsAddrFetchConn() const {
        return m_conn_type == ConnectionType::ADDR_FETCH;
    }

    bool IsInboundConn() const {
        return m_conn_type == ConnectionType::INBOUND;
    }

    /* Whether we send addr messages over this connection */
    bool RelayAddrsWithConn() const
    {
        return m_conn_type != ConnectionType::BLOCK_RELAY;
    }

    bool ExpectServicesFromConn() const {
        switch (m_conn_type) {
            case ConnectionType::INBOUND:
            case ConnectionType::MANUAL:
            case ConnectionType::FEELER:
                return false;
            case ConnectionType::OUTBOUND_FULL_RELAY:
            case ConnectionType::BLOCK_RELAY:
            case ConnectionType::ADDR_FETCH:
                return true;
        } // no default case, so the compiler can warn about missing cases

        assert(false);
    }

    /**
     * Get network the peer connected through.
     *
     * Returns Network::NET_ONION for *inbound* onion connections,
     * and CNetAddr::GetNetClass() otherwise. The latter cannot be used directly
     * because it doesn't detect the former, and it's not the responsibility of
     * the CNetAddr class to know the actual network a peer is connected through.
     *
     * @return network the peer connected through.
     */
    Network ConnectedThroughNetwork() const;

protected:
    mapMsgCmdSize mapSendBytesPerMsgCmd;
    mapMsgCmdSize mapRecvBytesPerMsgCmd GUARDED_BY(cs_vRecv);

public:
    uint256 hashContinue;
    std::atomic<int> nStartingHeight{-1};

    // flood relay
    std::vector<CAddress> vAddrToSend;
    std::unique_ptr<CRollingBloomFilter> m_addr_known{nullptr};
    bool fGetAddr{false};
    std::chrono::microseconds m_next_addr_send GUARDED_BY(cs_sendProcessing){0};
    std::chrono::microseconds m_next_local_addr_send GUARDED_BY(cs_sendProcessing){0};

    // List of block ids we still have announce.
    // There is no final sorting before sending, as they are always sent immediately
    // and in the order requested.
    std::vector<uint256> vInventoryBlockToSend GUARDED_BY(cs_inventory);
    Mutex cs_inventory;

    struct TxRelay {
        mutable RecursiveMutex cs_filter;
        // We use fRelayTxes for two purposes -
        // a) it allows us to not relay tx invs before receiving the peer's version message
        // b) the peer may tell us in its version message that we should not relay tx invs
        //    unless it loads a bloom filter.
        bool fRelayTxes GUARDED_BY(cs_filter){false};
        std::unique_ptr<CBloomFilter> pfilter PT_GUARDED_BY(cs_filter) GUARDED_BY(cs_filter){nullptr};

        mutable RecursiveMutex cs_tx_inventory;
        CRollingBloomFilter filterInventoryKnown GUARDED_BY(cs_tx_inventory){50000, 0.000001};
        // Set of transaction ids we still have to announce.
        // They are sorted by the mempool before relay, so the order is not important.
        std::set<uint256> setInventoryTxToSend;
        // Used for BIP35 mempool sending
        bool fSendMempool GUARDED_BY(cs_tx_inventory){false};
        // Last time a "MEMPOOL" request was serviced.
        std::atomic<std::chrono::seconds> m_last_mempool_req{std::chrono::seconds{0}};
        std::chrono::microseconds nNextInvSend{0};

        RecursiveMutex cs_feeFilter;
        // Minimum fee rate with which to filter inv's to this node
        CAmount minFeeFilter GUARDED_BY(cs_feeFilter){0};
        CAmount lastSentFeeFilter{0};
        int64_t nextSendTimeFeeFilter{0};
    };

    // m_tx_relay == nullptr if we're not relaying transactions with this peer
    std::unique_ptr<TxRelay> m_tx_relay;

    // Used for headers announcements - unfiltered blocks to relay
    std::vector<uint256> vBlockHashesToAnnounce GUARDED_BY(cs_inventory);

    /** UNIX epoch time of the last block received from this peer that we had
     * not yet seen (e.g. not already received from another peer), that passed
     * preliminary validity checks and was saved to disk, even if we don't
     * connect the block or it eventually fails connection. Used as an inbound
     * peer eviction criterium in CConnman::AttemptToEvictConnection. */
    std::atomic<int64_t> nLastBlockTime{0};

    /** UNIX epoch time of the last transaction received from this peer that we
     * had not yet seen (e.g. not already received from another peer) and that
     * was accepted into our mempool. Used as an inbound peer eviction criterium
     * in CConnman::AttemptToEvictConnection. */
    std::atomic<int64_t> nLastTXTime{0};

    // Ping time measurement:
    // The pong reply we're expecting, or 0 if no pong expected.
    std::atomic<uint64_t> nPingNonceSent{0};
    /** When the last ping was sent, or 0 if no ping was ever sent */
    std::atomic<std::chrono::microseconds> m_ping_start{std::chrono::microseconds{0}};
    // Last measured round-trip time.
    std::atomic<int64_t> nPingUsecTime{0};
    // Best measured round-trip time.
    std::atomic<int64_t> nMinPingUsecTime{std::numeric_limits<int64_t>::max()};
    // Whether a ping is requested.
    std::atomic<bool> fPingQueued{false};

    CNode(NodeId id, ServiceFlags nLocalServicesIn, int nMyStartingHeightIn, SOCKET hSocketIn, const CAddress &addrIn, uint64_t nKeyedNetGroupIn, uint64_t nLocalHostNonceIn, const CAddress &addrBindIn, const std::string &addrNameIn, ConnectionType conn_type_in, bool inbound_onion = false);
    ~CNode();
    CNode(const CNode&) = delete;
    CNode& operator=(const CNode&) = delete;

private:
    const NodeId id;
    const uint64_t nLocalHostNonce;
    const ConnectionType m_conn_type;
    std::atomic<int> m_greatest_common_version{INIT_PROTO_VERSION};

    //! Services offered to this peer.
    //!
    //! This is supplied by the parent CConnman during peer connection
    //! (CConnman::ConnectNode()) from its attribute of the same name.
    //!
    //! This is const because there is no protocol defined for renegotiating
    //! services initially offered to a peer. The set of local services we
    //! offer should not change after initialization.
    //!
    //! An interesting example of this is NODE_NETWORK and initial block
    //! download: a node which starts up from scratch doesn't have any blocks
    //! to serve, but still advertises NODE_NETWORK because it will eventually
    //! fulfill this role after IBD completes. P2P code is written in such a
    //! way that it can gracefully handle peers who don't make good on their
    //! service advertisements.
    const ServiceFlags nLocalServices;

    const int nMyStartingHeight;
    NetPermissionFlags m_permissionFlags{ PF_NONE };
    std::list<CNetMessage> vRecvMsg;  // Used only by SocketHandler thread

    mutable RecursiveMutex cs_addrName;
    std::string addrName GUARDED_BY(cs_addrName);

    // Our address, as reported by the peer
    CService addrLocal GUARDED_BY(cs_addrLocal);
    mutable RecursiveMutex cs_addrLocal;

    //! Whether this peer connected via our Tor onion service.
    const bool m_inbound_onion{false};

public:

    NodeId GetId() const {
        return id;
    }

    uint64_t GetLocalNonce() const {
        return nLocalHostNonce;
    }

    int GetMyStartingHeight() const {
        return nMyStartingHeight;
    }

    int GetRefCount() const
    {
        assert(nRefCount >= 0);
        return nRefCount;
    }

    bool ReceiveMsgBytes(const char *pch, unsigned int nBytes, bool& complete);

    void SetCommonVersion(int greatest_common_version)
    {
        m_greatest_common_version = greatest_common_version;
    }
    int GetCommonVersion() const
    {
        return m_greatest_common_version;
    }

    CService GetAddrLocal() const;
    //! May not be called more than once
    void SetAddrLocal(const CService& addrLocalIn);

    CNode* AddRef()
    {
        nRefCount++;
        return this;
    }

    void Release()
    {
        nRefCount--;
    }



    void AddAddressKnown(const CAddress& _addr)
    {
        assert(m_addr_known);
        m_addr_known->insert(_addr.GetKey());
    }

    void PushAddress(const CAddress& _addr, FastRandomContext &insecure_rand)
    {
        // Whether the peer supports the address in `_addr`. For example,
        // nodes that do not implement BIP155 cannot receive Tor v3 addresses
        // because they require ADDRv2 (BIP155) encoding.
        const bool addr_format_supported = m_wants_addrv2 || _addr.IsAddrV1Compatible();

        // Known checking here is only to save space from duplicates.
        // SendMessages will filter it again for knowns that were added
        // after addresses were pushed.
        assert(m_addr_known);
        if (_addr.IsValid() && !m_addr_known->contains(_addr.GetKey()) && addr_format_supported) {
            if (vAddrToSend.size() >= MAX_ADDR_TO_SEND) {
                vAddrToSend[insecure_rand.randrange(vAddrToSend.size())] = _addr;
            } else {
                vAddrToSend.push_back(_addr);
            }
        }
    }


    void AddKnownTx(const uint256& hash)
    {
        if (m_tx_relay != nullptr) {
            LOCK(m_tx_relay->cs_tx_inventory);
            m_tx_relay->filterInventoryKnown.insert(hash);
        }
    }

    void PushTxInventory(const uint256& hash)
    {
        if (m_tx_relay == nullptr) return;
        LOCK(m_tx_relay->cs_tx_inventory);
        if (!m_tx_relay->filterInventoryKnown.contains(hash)) {
            m_tx_relay->setInventoryTxToSend.insert(hash);
        }
    }

    void CloseSocketDisconnect();

    void copyStats(CNodeStats &stats, const std::vector<bool> &m_asmap);

    ServiceFlags GetLocalServices() const
    {
        return nLocalServices;
    }

    std::string GetAddrName() const;
    //! Sets the addrName only if it was not previously set
    void MaybeSetAddrName(const std::string& addrNameIn);

    std::string ConnectionTypeAsString() const;
};

/** Return a timestamp in the future (in microseconds) for exponentially distributed events. */
int64_t PoissonNextSend(int64_t now, int average_interval_seconds);

/** Wrapper to return mockable type */
inline std::chrono::microseconds PoissonNextSend(std::chrono::microseconds now, std::chrono::seconds average_interval)
{
    return std::chrono::microseconds{PoissonNextSend(now.count(), average_interval.count())};
}

#endif // BITCOIN_NET_H
