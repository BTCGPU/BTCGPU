// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2016-2017 The Zcash developers
// Copyright (c) 2018 The Bitcoin Private developers
// Copyright (c) 2017-2018 The Bitcoin Gold developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <uint256.h>
#include <arith_uint256.h>
#include <base58.h>
#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>
#include <pubkey.h>
#include <script/standard.h>

// For equihash_parameters_acceptable.
#include <crypto/equihash.h>
const unsigned int MAX_HEADERS_RESULTS = 2000;
const unsigned int MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000;
#define equihash_parameters_acceptable(N, K) \
    ((CBlockHeader::HEADER_SIZE + equihash_solution_size(N, K))*MAX_HEADERS_RESULTS < \
     MAX_PROTOCOL_MESSAGE_LENGTH-1000)


#include <assert.h>
#include <limits>

#include <chainparamsseeds.h>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = ArithToUint256(arith_uint256(nNonce));
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.nHeight  = 0;
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);

    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

const arith_uint256 maxUint = UintToArith256(uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));


class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Exception = uint256S("0x00000000000002dc756eebf4f49723ed8d30cc28a5f108eb94b1ba88ac4f9c22");
        consensus.BIP34Height = 227931;
        consensus.BIP34Hash = uint256S("0x000000000000024b89b42a942fe0d9fea3bb44ab7bd1b19115dd6a759c0808b8");
        consensus.BIP65Height = 388381; // 000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0
        consensus.BIP66Height = 363725; // 00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931
        consensus.BTGHeight = 491407; // Around 10/25/2017 12:00 UTC
        consensus.BTGPremineWindow = 8000;
        consensus.BTGZawyLWMAHeight = 536200; // Around 07/01/2018
        consensus.BTGEquihashForkHeight = 536200; // Around 07/01/2018
        consensus.BTGPremineEnforceWhitelist = true;
        consensus.powLimit = uint256S("0007ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimitStart = uint256S("0000000fffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimitLegacy = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

        consensus.nDigishieldAveragingWindow = 30;
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nDigishieldAveragingWindow);
        consensus.nDigishieldMaxAdjustDown = 32;
        consensus.nDigishieldMaxAdjustUp = 16;

        consensus.nZawyLwmaAveragingWindow = 45;
        consensus.nZawyLwmaAdjustedWeightLegacy = 13772;
        consensus.nZawyLwmaAdjustedWeight = 13772;
        consensus.nZawyLwmaMinDenominatorLegacy = 10;
        consensus.nZawyLwmaMinDenominator = 10;
        consensus.bZawyLwmaSolvetimeLimitation = true;
        consensus.BTGMaxFutureBlockTime = 12 * 10 * 60; // 120 mins
        
        consensus.nPowTargetTimespanLegacy = 14 * 24 * 60 * 60; // 10 minutes
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespanLegacy / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800; // May 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1479168000; // November 15th, 2016.
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1510704000; // November 15th, 2017.

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000a21782bdfbc18847452a68");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00000001ddb50bf6de8012bf8cd15bebc278c7e75ce5f77409ab78a291bd7e36"); //566000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xe1;
        pchMessageStart[1] = 0x47;
        pchMessageStart[2] = 0x6d;
        pchMessageStart[3] = 0x44;
        nDefaultPort = 8338; // different port than Bitcoin
        nPruneAfterHeight = 100000;
        const size_t N = 200, K = 9;
        const size_t N2 = 144, K2 = 5;
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N, K));
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N2, K2));
        nEquihashN = N;
        nEquihashK = K;
        nEquihashNnew = N2;
        nEquihashKnew = K2;

        genesis = CreateGenesisBlock(1231006505, 2083236893, 0x1d00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash(consensus);
        assert(consensus.hashGenesisBlock == uint256S("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.emplace_back("eu-dnsseed.bitcoingold-official.org", true);
        vSeeds.emplace_back("dnsseed.bitcoingold.org", true);
        vSeeds.emplace_back("dnsseed.bitcoingold.dev", true);

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,38);  // prefix: G
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,23);  // prefix: A
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "btg";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                { 11111, uint256S("0x0000000069e244f73d78e8fd29ba2fd2ed618bd6fa2ee92559f542fdb26e7c1d")},
                { 33333, uint256S("0x000000002dd5588a74784eaa7ab0507a18ad16a236e7b1ce69f00d7ddfb5d0a6")},
                { 74000, uint256S("0x0000000000573993a3c9e41ce34471c079dcf5f52a0e824a81e7f953b8661a20")},
                {105000, uint256S("0x00000000000291ce28027faea320c8d2b054b2e0fe44a773f3eefb151d6bdc97")},
                {134444, uint256S("0x00000000000005b12ffd4cd315cd34ffd4a594f430ac814c91184a0d42d2b0fe")},
                {168000, uint256S("0x000000000000099e61ea72015e79632f216fe6cb33d7899acb35b75c8303b763")},
                {193000, uint256S("0x000000000000059f452a5f7340de6682a977387c17010ff6e6c3bd83ca8b1317")},
                {210000, uint256S("0x000000000000048b95347e83192f69cf0366076336c639f9b7228e9ba171342e")},
                {216116, uint256S("0x00000000000001b4f4b433e81ee46494af945cf96014816a4e2370f11b23df4e")},
                {225430, uint256S("0x00000000000001c108384350f74090433e7fcf79a606b8e797f065b130575932")},
                {250000, uint256S("0x000000000000003887df1f29024b06fc2200b55f8af8f35453d7be294df2d214")},
                {279000, uint256S("0x0000000000000001ae8c72a0b0c301f67e3afca10e819efa9041e458e9bd7e40")},
                {295000, uint256S("0x00000000000000004d9b4ef50f0f9d686fd69db2e03af35a100370c64632a983")},
                {640650, uint256S("0x000000059ec8884fa4fbbdbe46c09cfb4ecba281dfa2351a05084e817c1200ae")},
            }
        };

        chainTxData = ChainTxData{
            // Data from rpc: getchaintxstats 4096 000000000c5dddd832b53e78d72f9f0d656d006bdf71a3de396e946e0799f197
            /* nTime    */ 1593660032,
            /* nTxCount */ 266775998,
            /* dTxRate  */ 0.0072359
        };

        vPreminePubkeys = {
            { "02babc391be409351aa993deb10083a0a852b79b36df80954341c74522fb298c21", "0267c59df7a3653d9cef3819cfb5a2a35d3f6217f522c307f05ab5714c1c738c44", "02532558057b56b76349b6a3a8c9d39547e482869fcd9556767c83523aa02fb358", "0214a27d6aa62e4aa14ebaf4682cd90e629bcb4ab18eccfed150dbed0b447bbafa", "038f4a7a5524929642e5688c5cecac4b5c7806a6520316f53ba71dbbdb3e181037", "02bb4feeb207779054aacb49b61d325f46e0b98afb301a66dc32ca3e7484f419ef" },
            { "0395a0b7b7fed3a09ad9a526f58955b2e6c6349678fc319fad79822d7b5727a189", "03b2c38ed954facb5adce1a8621ffb559601490125ba09e65d5e3e667a2adc6fdc", "03fd383f5b8ccb9aaedd2b5924f7198dacd2f6d38b50460b03194204344c5e8f2c", "02aa0a0aa08142f9bd3c2e794a292c68df730173bce2e6a69c2810e445df7ed363", "0291a043b4375bbde4e5e3738957ae5e83c8eecab70b90f55b3dc0b547a1ac19cf", "031f4d880c835238b97625d20579940e965a833c30fb8f643bd5e6a43ef37e0ee7" },
            { "0328571bff52ab95267ca51d7fe2689599cb73300a3847d6440ecd9882166e1ed7", "0301411004164e5798db00227c1a8e87c4e0a0e3425057b1c650a5e88252b08035", "0399e7477f01d40af05a417cfbf98c179e0c92778fb731e7f6422036c4918cb0d1", "02f136e4181d63ec1d3f587513a21427c6ec5d5c36364f20abdf4f751ce21e485f", "02fd5c856002b77384599ea9cd6ceae515223809e6f1b63d45be5456b409d2be8e", "02bf748f7e7291e9061f32bc72ea52a325154dadddb98348307838565fc8855f4c" },
            { "03d1198ed4659a53bbc5fd945893545ee5efda9c20d014b87c138f658ed61d9cd2", "03d558f9dd313bf6a4ebbc4f3c9209f758e21e99c1d3fb3a2fc40517f4de01d55c", "03e185dd9289d6f72ed579b2db9474e033361902cb5c60817f757652fd86910677", "023c4975dbb840e91a0047496412a8f69eaf61571d24a552713d585337bed26101", "032c8735d320b6219cb398999345fea9e6b234e5f7d9f96c6a2758658d261acd6d", "029860998228d746ec5ccdc47b451b3143c05f9e26b7b1a491d64429dcac3feb0e" },
        };
        /* disable fallback fee on mainnet */
        m_fallback_fee_enabled = false;
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = -1;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = -1;
        consensus.BIP66Height = -1;
        consensus.BTGHeight = 1;
        consensus.BTGZawyLWMAHeight = -1; // Activated on testnet
        consensus.BTGEquihashForkHeight = 14300;
        consensus.BTGPremineWindow = 50;
        consensus.BTGPremineEnforceWhitelist = false;
        consensus.powLimit = uint256S("0007ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimitStart = uint256S("0007ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimitLegacy = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

        consensus.nDigishieldAveragingWindow = 30;
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nDigishieldAveragingWindow);
        consensus.nDigishieldMaxAdjustDown = 32;
        consensus.nDigishieldMaxAdjustUp = 16;

        consensus.nZawyLwmaAveragingWindow = 45;
        consensus.nZawyLwmaAdjustedWeightLegacy = 13632;
        consensus.nZawyLwmaAdjustedWeight = 13772;
        consensus.nZawyLwmaMinDenominatorLegacy = 3;
        consensus.nZawyLwmaMinDenominator = 10;
        consensus.bZawyLwmaSolvetimeLimitation = false;
        consensus.BTGMaxFutureBlockTime = 7 * 10 * 60; // 70 mins
        
        consensus.nPowTargetTimespanLegacy = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespanLegacy / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1514764800; // January 1st, 2018
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1546300800; // January 1st, 2019

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1514764800; // January 1st 2018
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1546300800; // January 1st 2019

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000000000018469bad0");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x000042e99d907f6a06dfc3e09499d6abe7cec3f665bffb2e297f5242ff948e60");  //38500

        pchMessageStart[0] = 0xe2;
        pchMessageStart[1] = 0x48;
        pchMessageStart[2] = 0x6e;
        pchMessageStart[3] = 0x45;
        nDefaultPort = 18338;
        nPruneAfterHeight = 1000;
        const size_t N = 200, K = 9;
        const size_t N2 = 144, K2 = 5;
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N, K));
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N2, K2));
        nEquihashN = N;
        nEquihashK = K;
        nEquihashNnew = N2;
        nEquihashKnew = K2;

        genesis = CreateGenesisBlock(1516123516, 0x56bd5142, 0x1d00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash(consensus);
        assert(consensus.hashGenesisBlock == uint256S("0x00000000e0781ebe24b91eedc293adfea2f557b53ec379e78959de3853e6f9f6"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));
        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top

        vSeeds.emplace_back("eu-test-dnsseed.bitcoingold-official.org", true);
        vSeeds.emplace_back("test-dnsseed.bitcoingold.org", true);
        vSeeds.emplace_back("test-dnsseed.bitcoingold.dev", true);

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tbtg";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;


        checkpointData = {
            {
                {0, uint256S("0x00000000e0781ebe24b91eedc293adfea2f557b53ec379e78959de3853e6f9f6")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        /* enable fallback fee on testnet */
        m_fallback_fee_enabled = true;
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP16Exception = uint256();
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.BTGHeight = 2000;
        consensus.BTGZawyLWMAHeight = -1; // Activated on regtest
        consensus.BTGEquihashForkHeight = 2001;
        consensus.BTGPremineWindow = 10;
        consensus.BTGPremineEnforceWhitelist = false;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimitStart = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimitLegacy = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

        consensus.nDigishieldAveragingWindow = 30;
        consensus.nDigishieldMaxAdjustDown = 32;
        consensus.nDigishieldMaxAdjustUp = 16;

        consensus.nZawyLwmaAveragingWindow = 45;
        consensus.nZawyLwmaAdjustedWeightLegacy = 13772;
        consensus.nZawyLwmaAdjustedWeight = 13772;
        consensus.nZawyLwmaMinDenominatorLegacy = 10;
        consensus.nZawyLwmaMinDenominator = 10;
        consensus.bZawyLwmaSolvetimeLimitation = true;
        consensus.BTGMaxFutureBlockTime = 7 * 10 * 60; // 70 mins

        consensus.nPowTargetTimespanLegacy = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");
        
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;

        nDefaultPort = 18444;
        nPruneAfterHeight = 1000;
        const size_t N = 48, K = 5;
        const size_t N2 = 96, K2 = 5;
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N, K));
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N2, K2));
        nEquihashN = N;
        nEquihashK = K;
        nEquihashNnew = N2;
        nEquihashKnew = K2;

        genesis = CreateGenesisBlock(1296688602, 2, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash(consensus);
        assert(consensus.hashGenesisBlock == uint256S("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            {
                {0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "btgrt";

        /* enable fallback fee on regtest */
        m_fallback_fee_enabled = true;
    }
};

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}


static CScript CltvMultiSigScript(const std::vector<std::string>& pubkeys, uint32_t lock_time) {
    assert(pubkeys.size() == 6);
    CScript redeem_script;
    if (lock_time > 0) {
        redeem_script << lock_time << OP_CHECKLOCKTIMEVERIFY << OP_DROP;
    }
    redeem_script << 4;
    for (const std::string& pubkey : pubkeys) {
        redeem_script << ToByteVector(ParseHex(pubkey));
    }
    redeem_script << 6 << OP_CHECKMULTISIG;
    return redeem_script;
}

unsigned int CChainParams::EquihashSolutionWidth(int height) const
{
    return EhSolutionWidth(EquihashN(height), EquihashK(height));
}

bool CChainParams::IsPremineAddressScript(const CScript& scriptPubKey, uint32_t height) const {
    static const int LOCK_TIME = 3 * 365 * 24 * 3600;  // 3 years
    static const int LOCK_STAGES = 3 * 12;  // Every month for 3 years
    assert((uint32_t)consensus.BTGHeight <= height &&
           height < (uint32_t)(consensus.BTGHeight + consensus.BTGPremineWindow));
    int block = height - consensus.BTGHeight;
    int num_unlocked = consensus.BTGPremineWindow * 40 / 100;  // 40% unlocked.
    int num_locked = consensus.BTGPremineWindow - num_unlocked;  // 60% time-locked.
    int stage_lock_time = LOCK_TIME / LOCK_STAGES / consensus.nPowTargetSpacing;
    int stage_block_height = num_locked / LOCK_STAGES;
    const std::vector<std::string> pubkeys = vPreminePubkeys[block % vPreminePubkeys.size()];  // Round robin.
    CScript redeem_script;
    if (block < num_unlocked) {
        redeem_script = CltvMultiSigScript(pubkeys, 0);
    } else {
        int locked_block = block - num_unlocked;
        int stage = locked_block / stage_block_height;
        int lock_time = consensus.BTGHeight + stage_lock_time * (1 + stage);
        redeem_script = CltvMultiSigScript(pubkeys, lock_time);
    }
    CScriptID redeem_script_id(redeem_script);
    CScript target_scriptPubkey = GetScriptForDestination(redeem_script_id);
    return scriptPubKey == target_scriptPubkey;
}
