// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key.h>
#include <keystore.h>
#include <script/ismine.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/standard.h>
#include <test/test_bitcoin.h>

#include <boost/test/unit_test.hpp>


BOOST_FIXTURE_TEST_SUITE(script_standard_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(script_standard_Solver_success)
{
    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript s;
    txnouttype whichType;
    std::vector<std::vector<unsigned char> > solutions;

    // TX_PUBKEY
    s.clear();
    s << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    BOOST_CHECK(Solver(s, whichType, solutions));
    BOOST_CHECK_EQUAL(whichType, TX_PUBKEY);
    BOOST_CHECK_EQUAL(solutions.size(), 1);
    BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0]));

    // TX_PUBKEYHASH
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(Solver(s, whichType, solutions));
    BOOST_CHECK_EQUAL(whichType, TX_PUBKEYHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1);
    BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0].GetID()));

    // TX_SCRIPTHASH
    CScript redeemScript(s); // initialize with leftover P2PKH script
    s.clear();
    s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    BOOST_CHECK(Solver(s, whichType, solutions));
    BOOST_CHECK_EQUAL(whichType, TX_SCRIPTHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1);
    BOOST_CHECK(solutions[0] == ToByteVector(CScriptID(redeemScript)));

    // TX_MULTISIG
    s.clear();
    s << OP_1 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(Solver(s, whichType, solutions));
    BOOST_CHECK_EQUAL(whichType, TX_MULTISIG);
    BOOST_CHECK_EQUAL(solutions.size(), 4);
    BOOST_CHECK(solutions[0] == std::vector<unsigned char>({1}));
    BOOST_CHECK(solutions[1] == ToByteVector(pubkeys[0]));
    BOOST_CHECK(solutions[2] == ToByteVector(pubkeys[1]));
    BOOST_CHECK(solutions[3] == std::vector<unsigned char>({2}));

    s.clear();
    s << OP_2 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        ToByteVector(pubkeys[2]) <<
        OP_3 << OP_CHECKMULTISIG;
    BOOST_CHECK(Solver(s, whichType, solutions));
    BOOST_CHECK_EQUAL(whichType, TX_MULTISIG);
    BOOST_CHECK_EQUAL(solutions.size(), 5);
    BOOST_CHECK(solutions[0] == std::vector<unsigned char>({2}));
    BOOST_CHECK(solutions[1] == ToByteVector(pubkeys[0]));
    BOOST_CHECK(solutions[2] == ToByteVector(pubkeys[1]));
    BOOST_CHECK(solutions[3] == ToByteVector(pubkeys[2]));
    BOOST_CHECK(solutions[4] == std::vector<unsigned char>({3}));

    // TX_NULL_DATA
    s.clear();
    s << OP_RETURN <<
        std::vector<unsigned char>({0}) <<
        std::vector<unsigned char>({75}) <<
        std::vector<unsigned char>({255});
    BOOST_CHECK(Solver(s, whichType, solutions));
    BOOST_CHECK_EQUAL(whichType, TX_NULL_DATA);
    BOOST_CHECK_EQUAL(solutions.size(), 0);

    // TX_WITNESS_V0_KEYHASH
    s.clear();
    s << OP_0 << ToByteVector(pubkeys[0].GetID());
    BOOST_CHECK(Solver(s, whichType, solutions));
    BOOST_CHECK_EQUAL(whichType, TX_WITNESS_V0_KEYHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1);
    BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0].GetID()));

    // TX_WITNESS_V0_SCRIPTHASH
    uint256 scriptHash;
    CSHA256().Write(&redeemScript[0], redeemScript.size())
        .Finalize(scriptHash.begin());

    s.clear();
    s << OP_0 << ToByteVector(scriptHash);
    BOOST_CHECK(Solver(s, whichType, solutions));
    BOOST_CHECK_EQUAL(whichType, TX_WITNESS_V0_SCRIPTHASH);
    BOOST_CHECK_EQUAL(solutions.size(), 1);
    BOOST_CHECK(solutions[0] == ToByteVector(scriptHash));

    // TX_NONSTANDARD
    s.clear();
    s << OP_9 << OP_ADD << OP_11 << OP_EQUAL;
    BOOST_CHECK(!Solver(s, whichType, solutions));
    BOOST_CHECK_EQUAL(whichType, TX_NONSTANDARD);
}

BOOST_AUTO_TEST_CASE(script_standard_Solver_failure)
{
    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    CScript s;
    txnouttype whichType;
    std::vector<std::vector<unsigned char> > solutions;

    // TX_PUBKEY with incorrectly sized pubkey
    s.clear();
    s << std::vector<unsigned char>(30, 0x01) << OP_CHECKSIG;
    BOOST_CHECK(!Solver(s, whichType, solutions));

    // TX_PUBKEYHASH with incorrectly sized key hash
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkey) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(!Solver(s, whichType, solutions));

    // TX_SCRIPTHASH with incorrectly sized script hash
    s.clear();
    s << OP_HASH160 << std::vector<unsigned char>(21, 0x01) << OP_EQUAL;
    BOOST_CHECK(!Solver(s, whichType, solutions));

    // TX_MULTISIG 0/2
    s.clear();
    s << OP_0 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK(!Solver(s, whichType, solutions));

    // TX_MULTISIG 2/1
    s.clear();
    s << OP_2 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK(!Solver(s, whichType, solutions));

    // TX_MULTISIG n = 2 with 1 pubkey
    s.clear();
    s << OP_1 << ToByteVector(pubkey) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(!Solver(s, whichType, solutions));

    // TX_MULTISIG n = 1 with 0 pubkeys
    s.clear();
    s << OP_1 << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK(!Solver(s, whichType, solutions));

    // TX_NULL_DATA with other opcodes
    s.clear();
    s << OP_RETURN << std::vector<unsigned char>({75}) << OP_ADD;
    BOOST_CHECK(!Solver(s, whichType, solutions));

    // TX_WITNESS with incorrect program size
    s.clear();
    s << OP_0 << std::vector<unsigned char>(19, 0x01);
    BOOST_CHECK(!Solver(s, whichType, solutions));
}

BOOST_AUTO_TEST_CASE(script_standard_ExtractDestination)
{
    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    CScript s;
    CTxDestination address;

    // TX_PUBKEY
    s.clear();
    s << ToByteVector(pubkey) << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(boost::get<CKeyID>(&address) &&
                *boost::get<CKeyID>(&address) == pubkey.GetID());

    // TX_PUBKEYHASH
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(boost::get<CKeyID>(&address) &&
                *boost::get<CKeyID>(&address) == pubkey.GetID());

    // TX_SCRIPTHASH
    CScript redeemScript(s); // initialize with leftover P2PKH script
    s.clear();
    s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(boost::get<CScriptID>(&address) &&
                *boost::get<CScriptID>(&address) == CScriptID(redeemScript));

    // TX_MULTISIG
    s.clear();
    s << OP_1 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK(!ExtractDestination(s, address));

    // TX_NULL_DATA
    s.clear();
    s << OP_RETURN << std::vector<unsigned char>({75});
    BOOST_CHECK(!ExtractDestination(s, address));

    // TX_WITNESS_V0_KEYHASH
    s.clear();
    s << OP_0 << ToByteVector(pubkey.GetID());
    BOOST_CHECK(ExtractDestination(s, address));
    WitnessV0KeyHash keyhash;
    CHash160().Write(pubkey.begin(), pubkey.size()).Finalize(keyhash.begin());
    BOOST_CHECK(boost::get<WitnessV0KeyHash>(&address) && *boost::get<WitnessV0KeyHash>(&address) == keyhash);

    // TX_WITNESS_V0_SCRIPTHASH
    s.clear();
    WitnessV0ScriptHash scripthash;
    CSHA256().Write(redeemScript.data(), redeemScript.size()).Finalize(scripthash.begin());
    s << OP_0 << ToByteVector(scripthash);
    BOOST_CHECK(ExtractDestination(s, address));
    BOOST_CHECK(boost::get<WitnessV0ScriptHash>(&address) && *boost::get<WitnessV0ScriptHash>(&address) == scripthash);

    // TX_WITNESS with unknown version
    s.clear();
    s << OP_1 << ToByteVector(pubkey);
    BOOST_CHECK(ExtractDestination(s, address));
    WitnessUnknown unk;
    unk.length = 33;
    unk.version = 1;
    std::copy(pubkey.begin(), pubkey.end(), unk.program);
    BOOST_CHECK(boost::get<WitnessUnknown>(&address) && *boost::get<WitnessUnknown>(&address) == unk);
}

BOOST_AUTO_TEST_CASE(script_standard_ExtractDestinations)
{
    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript s;
    txnouttype whichType;
    std::vector<CTxDestination> addresses;
    int nRequired;

    // TX_PUBKEY
    s.clear();
    s << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired));
    BOOST_CHECK_EQUAL(whichType, TX_PUBKEY);
    BOOST_CHECK_EQUAL(addresses.size(), 1);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<CKeyID>(&addresses[0]) &&
                *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());

    // TX_PUBKEYHASH
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired));
    BOOST_CHECK_EQUAL(whichType, TX_PUBKEYHASH);
    BOOST_CHECK_EQUAL(addresses.size(), 1);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<CKeyID>(&addresses[0]) &&
                *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());

    // TX_SCRIPTHASH
    CScript redeemScript(s); // initialize with leftover P2PKH script
    s.clear();
    s << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired));
    BOOST_CHECK_EQUAL(whichType, TX_SCRIPTHASH);
    BOOST_CHECK_EQUAL(addresses.size(), 1);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<CScriptID>(&addresses[0]) &&
                *boost::get<CScriptID>(&addresses[0]) == CScriptID(redeemScript));

    // TX_MULTISIG
    s.clear();
    s << OP_2 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired));
    BOOST_CHECK_EQUAL(whichType, TX_MULTISIG);
    BOOST_CHECK_EQUAL(addresses.size(), 2);
    BOOST_CHECK_EQUAL(nRequired, 2);
    BOOST_CHECK(boost::get<CKeyID>(&addresses[0]) &&
                *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());
    BOOST_CHECK(boost::get<CKeyID>(&addresses[1]) &&
                *boost::get<CKeyID>(&addresses[1]) == pubkeys[1].GetID());

    // TX_NULL_DATA
    s.clear();
    s << OP_RETURN << std::vector<unsigned char>({75});
    BOOST_CHECK(!ExtractDestinations(s, whichType, addresses, nRequired));
}

BOOST_AUTO_TEST_CASE(script_standard_GetScriptFor_)
{
    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript expected, result;

    // CKeyID
    expected.clear();
    expected << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    result = GetScriptForDestination(pubkeys[0].GetID());
    BOOST_CHECK(result == expected);

    // CScriptID
    CScript redeemScript(result);
    expected.clear();
    expected << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;
    result = GetScriptForDestination(CScriptID(redeemScript));
    BOOST_CHECK(result == expected);

    // CNoDestination
    expected.clear();
    result = GetScriptForDestination(CNoDestination());
    BOOST_CHECK(result == expected);

    // GetScriptForRawPubKey
    expected.clear();
    expected << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    result = GetScriptForRawPubKey(pubkeys[0]);
    BOOST_CHECK(result == expected);

    // GetScriptForMultisig
    expected.clear();
    expected << OP_2 <<
        ToByteVector(pubkeys[0]) <<
        ToByteVector(pubkeys[1]) <<
        ToByteVector(pubkeys[2]) <<
        OP_3 << OP_CHECKMULTISIG;
    result = GetScriptForMultisig(2, std::vector<CPubKey>(pubkeys, pubkeys + 3));
    BOOST_CHECK(result == expected);

    // GetScriptForWitness
    CScript witnessScript;

    witnessScript << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    expected.clear();
    expected << OP_0 << ToByteVector(pubkeys[0].GetID());
    result = GetScriptForWitness(witnessScript);
    BOOST_CHECK(result == expected);

    witnessScript.clear();
    witnessScript << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;
    result = GetScriptForWitness(witnessScript);
    BOOST_CHECK(result == expected);

    witnessScript.clear();
    witnessScript << OP_1 << ToByteVector(pubkeys[0]) << OP_1 << OP_CHECKMULTISIG;

    uint256 scriptHash;
    CSHA256().Write(&witnessScript[0], witnessScript.size())
        .Finalize(scriptHash.begin());

    expected.clear();
    expected << OP_0 << ToByteVector(scriptHash);
    result = GetScriptForWitness(witnessScript);
    BOOST_CHECK(result == expected);
}

BOOST_AUTO_TEST_CASE(script_standard_IsMine)
{
    CKey keys[2];
    CPubKey pubkeys[2];
    for (int i = 0; i < 2; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CKey uncompressedKey;
    uncompressedKey.MakeNewKey(false);
    CPubKey uncompressedPubkey = uncompressedKey.GetPubKey();

    CScript scriptPubKey;
    isminetype result;
    bool isInvalid;

    // P2PK compressed
    {
        CBasicKeyStore keystore;
        scriptPubKey.clear();
        scriptPubKey << ToByteVector(pubkeys[0]) << OP_CHECKSIG;

        // Keystore does not have key
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has key
        keystore.AddKey(keys[0]);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // P2PK uncompressed
    {
        CBasicKeyStore keystore;
        scriptPubKey.clear();
        scriptPubKey << ToByteVector(uncompressedPubkey) << OP_CHECKSIG;

        // Keystore does not have key
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has key
        keystore.AddKey(uncompressedKey);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // P2PKH compressed
    {
        CBasicKeyStore keystore;
        scriptPubKey.clear();
        scriptPubKey << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        // Keystore does not have key
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has key
        keystore.AddKey(keys[0]);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // P2PKH uncompressed
    {
        CBasicKeyStore keystore;
        scriptPubKey.clear();
        scriptPubKey << OP_DUP << OP_HASH160 << ToByteVector(uncompressedPubkey.GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        // Keystore does not have key
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has key
        keystore.AddKey(uncompressedKey);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // P2SH
    {
        CBasicKeyStore keystore;

        CScript redeemScript;
        redeemScript << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID()) << OP_EQUALVERIFY << OP_CHECKSIG;

        scriptPubKey.clear();
        scriptPubKey << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;

        // Keystore does not have redeemScript or key
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has redeemScript but no key
        keystore.AddCScript(redeemScript);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has redeemScript and key
        keystore.AddKey(keys[0]);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // P2WPKH compressed
    {
        CBasicKeyStore keystore;
        keystore.AddKey(keys[0]);

        scriptPubKey.clear();
        scriptPubKey << OP_0 << ToByteVector(pubkeys[0].GetID());

        // Keystore implicitly has key and P2SH redeemScript
        keystore.AddCScript(scriptPubKey);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // P2WPKH uncompressed
    {
        CBasicKeyStore keystore;
        keystore.AddKey(uncompressedKey);

        scriptPubKey.clear();
        scriptPubKey << OP_0 << ToByteVector(uncompressedPubkey.GetID());

        // Keystore has key, but no P2SH redeemScript
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has key and P2SH redeemScript
        keystore.AddCScript(scriptPubKey);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(isInvalid);
    }

    // scriptPubKey multisig
    {
        CBasicKeyStore keystore;

        scriptPubKey.clear();
        scriptPubKey << OP_2 <<
            ToByteVector(uncompressedPubkey) <<
            ToByteVector(pubkeys[1]) <<
            OP_2 << OP_CHECKMULTISIG;

        // Keystore does not have any keys
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has 1/2 keys
        keystore.AddKey(uncompressedKey);

        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has 2/2 keys
        keystore.AddKey(keys[1]);

        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // P2SH multisig
    {
        CBasicKeyStore keystore;
        keystore.AddKey(uncompressedKey);
        keystore.AddKey(keys[1]);

        CScript redeemScript;
        redeemScript << OP_2 <<
            ToByteVector(uncompressedPubkey) <<
            ToByteVector(pubkeys[1]) <<
            OP_2 << OP_CHECKMULTISIG;

        scriptPubKey.clear();
        scriptPubKey << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;

        // Keystore has no redeemScript
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has redeemScript
        keystore.AddCScript(redeemScript);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // P2WSH multisig with compressed keys
    {
        CBasicKeyStore keystore;
        keystore.AddKey(keys[0]);
        keystore.AddKey(keys[1]);

        CScript witnessScript;
        witnessScript << OP_2 <<
            ToByteVector(pubkeys[0]) <<
            ToByteVector(pubkeys[1]) <<
            OP_2 << OP_CHECKMULTISIG;

        uint256 scriptHash;
        CSHA256().Write(&witnessScript[0], witnessScript.size())
            .Finalize(scriptHash.begin());

        scriptPubKey.clear();
        scriptPubKey << OP_0 << ToByteVector(scriptHash);

        // Keystore has keys, but no witnessScript or P2SH redeemScript
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has keys and witnessScript, but no P2SH redeemScript
        keystore.AddCScript(witnessScript);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has keys, witnessScript, P2SH redeemScript
        keystore.AddCScript(scriptPubKey);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // P2WSH multisig with uncompressed key
    {
        CBasicKeyStore keystore;
        keystore.AddKey(uncompressedKey);
        keystore.AddKey(keys[1]);

        CScript witnessScript;
        witnessScript << OP_2 <<
            ToByteVector(uncompressedPubkey) <<
            ToByteVector(pubkeys[1]) <<
            OP_2 << OP_CHECKMULTISIG;

        uint256 scriptHash;
        CSHA256().Write(&witnessScript[0], witnessScript.size())
            .Finalize(scriptHash.begin());

        scriptPubKey.clear();
        scriptPubKey << OP_0 << ToByteVector(scriptHash);

        // Keystore has keys, but no witnessScript or P2SH redeemScript
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has keys and witnessScript, but no P2SH redeemScript
        keystore.AddCScript(witnessScript);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has keys, witnessScript, P2SH redeemScript
        keystore.AddCScript(scriptPubKey);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(isInvalid);
    }

    // P2WSH multisig wrapped in P2SH
    {
        CBasicKeyStore keystore;

        CScript witnessScript;
        witnessScript << OP_2 <<
            ToByteVector(pubkeys[0]) <<
            ToByteVector(pubkeys[1]) <<
            OP_2 << OP_CHECKMULTISIG;

        uint256 scriptHash;
        CSHA256().Write(&witnessScript[0], witnessScript.size())
            .Finalize(scriptHash.begin());

        CScript redeemScript;
        redeemScript << OP_0 << ToByteVector(scriptHash);

        scriptPubKey.clear();
        scriptPubKey << OP_HASH160 << ToByteVector(CScriptID(redeemScript)) << OP_EQUAL;

        // Keystore has no witnessScript, P2SH redeemScript, or keys
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has witnessScript and P2SH redeemScript, but no keys
        keystore.AddCScript(redeemScript);
        keystore.AddCScript(witnessScript);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);

        // Keystore has keys, witnessScript, P2SH redeemScript
        keystore.AddKey(keys[0]);
        keystore.AddKey(keys[1]);
        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        BOOST_CHECK(!isInvalid);
    }

    // OP_RETURN
    {
        CBasicKeyStore keystore;
        keystore.AddKey(keys[0]);

        scriptPubKey.clear();
        scriptPubKey << OP_RETURN << ToByteVector(pubkeys[0]);

        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);
    }

    // Nonstandard
    {
        CBasicKeyStore keystore;
        keystore.AddKey(keys[0]);

        scriptPubKey.clear();
        scriptPubKey << OP_9 << OP_ADD << OP_11 << OP_EQUAL;

        result = IsMine(keystore, scriptPubKey, isInvalid);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
        BOOST_CHECK(!isInvalid);
    }
}

BOOST_AUTO_TEST_SUITE_END()
