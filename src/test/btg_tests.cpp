// Copyright (c) 2017 The Bitcoin Gold developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "serialize.h"
#include "streams.h"
#include "version.h"
#include "primitives/block.h"
#include "test/btg_cltv_multisig_data.h"
#include "test/test_bitcoin.h"

#include <stdint.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(btg_tests, BasicTestingSetup)

// Taken from Zcash project to test the compability.
class ZcashBlockHeader
{
public:
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint256 hashReserved;
    uint32_t nTime;
    uint32_t nBits;
    uint256 nNonce;
    std::vector<unsigned char> nSolution;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(hashReserved);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
        READWRITE(nSolution);
    }
};

BOOST_AUTO_TEST_CASE(zcash_header_compatible)
{
    CBlockHeader btg_header;
    ZcashBlockHeader zcash_header;
    CDataStream ss(SER_DISK, PROTOCOL_VERSION);

    // `CBlockHeader.nHeight` should be placed in the first 4 bytes of `ZcashBlockHeader.hashReserved`.
    btg_header.nHeight = 10000000;
    // `CBlockHeader.nReserved[5:7]` should be placed in the last 8 bytes of `ZcashBlockHeader.hashReserved`.
    btg_header.nReserved[5] = 1234;
    btg_header.nReserved[6] = 0;
    btg_header.nSolution = std::vector<unsigned char> {
        0x11, 0x22, 0x33
    };

    ss << btg_header;
    ss >> zcash_header;

    BOOST_CHECK_EQUAL(btg_header.nHeight, (uint32_t)zcash_header.hashReserved.GetUint64(0));
    BOOST_CHECK_EQUAL(btg_header.nReserved[5], (uint32_t)zcash_header.hashReserved.GetUint64(3));
    BOOST_CHECK_EQUAL(btg_header.nSolution[2], zcash_header.nSolution[2]);
}

// For address conversion.
class FakeChainParam : public CChainParams
{
public:
    FakeChainParam()
    {
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
    }
};
static const FakeChainParam fakeLegacyChainParam;

std::string ConvertToNewAddress(const std::string& old_address)
{
    CBitcoinAddress addr(old_address);
    BOOST_CHECK(addr.IsValid(fakeLegacyChainParam));
    CBitcoinAddress new_addr(addr.Get(fakeLegacyChainParam));
    return new_addr.ToString();
}

std::string ConvertToOldAddress(const std::string& new_address)
{
    CBitcoinAddress addr(new_address);
    BOOST_CHECK(addr.IsValid());
    CBitcoinAddress old_addr(addr.Get(), fakeLegacyChainParam);
    return old_addr.ToString();
}

BOOST_AUTO_TEST_CASE(address_compatible)
{
    std::string p2pkh_addr = "16FoV6CpUT5YPHL8rcY7SQoHvnyXY4yRsD";
    BOOST_CHECK_EQUAL(ConvertToOldAddress(ConvertToNewAddress(p2pkh_addr)), p2pkh_addr);
    std::string p2sh_addr = "3CjLoKt9KYcfjf4MWAbGz8xnpfJvzvMxMi";
    BOOST_CHECK_EQUAL(ConvertToOldAddress(ConvertToNewAddress(p2sh_addr)), p2sh_addr);
}

BOOST_AUTO_TEST_CASE(cltv_multisig_whitelist)
{
    std::vector<CltvMultiSigTestData> data = GetCltvMultiSigTestData();
    const CChainParams& params = Params();
    for (const CltvMultiSigTestData& test_case : data) {
        std::vector<unsigned char> redeem_script_data = ParseHex(test_case.redeem_script);
        CScript redeem_script(redeem_script_data.begin(), redeem_script_data.end());
        CScript p2sh_script = GetScriptForDestination(CScriptID(redeem_script));
        BOOST_CHECK(params.IsPremineAddressScript(p2sh_script, test_case.height));
    }
}

BOOST_AUTO_TEST_SUITE_END()
