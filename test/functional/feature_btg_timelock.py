#!/usr/bin/env python3
# Copyright (c) 2017 The Bitcoin Gold developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test cases for Bitcoin Gold CLTV wallet."""

import io
from decimal import Decimal

from test_framework.address import script_to_p2sh
from test_framework.key import ECKey, ECPubKey
from test_framework.messages import CTransaction
from test_framework.script import (
    CScript,
    CScriptNum,
    OP_0,
    OP_4,
    OP_6,
    OP_CHECKLOCKTIMEVERIFY,
    OP_CHECKMULTISIG,
    OP_DROP,
    SIGHASH_ALL,
    LegacySignatureHash,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, hex_str_to_bytes


EPS = 1e-6

def sig(x):
    if x < -EPS:
        return -1
    elif x > EPS:
        return 1
    else:
        return 0


class BTGTimeLockTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = False
        self.extra_args = [[]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def sign_tx(self, tx, spend_tx, spend_n, redeem_script, in_n, keys):
        """Sign a P2SH transaction by privkeys."""
        sighash, _ = LegacySignatureHash(redeem_script, tx, in_n, SIGHASH_ALL)
        sigs = [key.sign_ecdsa(sighash) + bytes(bytearray([SIGHASH_ALL])) for key in keys]
        tx.vin[0].scriptSig = CScript([OP_0] + sigs + [redeem_script])
        tx.rehash()

    def run_test(self):
        node = self.nodes[0]

        # Generate 6 keys.
        rawkeys = []
        pubkeys = []
        for i in range(6):
            raw_key = ECKey()
            raw_key.generate()
            rawkeys.append(raw_key)
        pubkeys = [key.get_pubkey().get_bytes() for key in rawkeys]

        # Create a 4-of-6 multi-sig wallet with CLTV.
        height = 210
        redeem_script = CScript(
            [CScriptNum(height), OP_CHECKLOCKTIMEVERIFY, OP_DROP]  # CLTV (lock_time >= 210)
            + [OP_4] + pubkeys +  [OP_6, OP_CHECKMULTISIG])  # multi-sig
        hex_redeem_script = redeem_script.hex()
        p2sh_address = script_to_p2sh(redeem_script, main=False)

        # Send 1 coin to the mult-sig wallet.
        txid = node.sendtoaddress(p2sh_address, 1.0)
        raw_tx = node.getrawtransaction(txid, True)
        try:
            node.importaddress(hex_redeem_script, 'cltv', True, True)
        except Exception:
            pass
        assert_equal(sig(node.getreceivedbyaddress(p2sh_address, 0) - Decimal(1.0)), 0)

        # Try to spend the coin.
        addr_to = node.getnewaddress('')

        # (1) Find the UTXO
        for vout in raw_tx['vout']:
            if vout['scriptPubKey']['addresses'] == [p2sh_address]:
                vout_n = vout['n']
        hex_script_pubkey = raw_tx['vout'][vout_n]['scriptPubKey']['hex']
        value = raw_tx['vout'][vout_n]['value']

        # (2) Create a tx
        inputs = [{
            "txid": txid,
            "vout": vout_n,
            "scriptPubKey": hex_script_pubkey,
            "redeemScript": hex_redeem_script,
            "amount": value,
        }]
        outputs = {addr_to: 0.999}
        lock_time = height
        hex_spend_raw_tx = node.createrawtransaction(inputs, outputs, lock_time)
        hex_funding_raw_tx = node.getrawtransaction(txid, False)

        # (3) Try to sign the spending tx.
        tx0 = CTransaction()
        tx0.deserialize(io.BytesIO(hex_str_to_bytes(hex_funding_raw_tx)))
        tx1 = CTransaction()
        tx1.deserialize(io.BytesIO(hex_str_to_bytes(hex_spend_raw_tx)))
        self.sign_tx(tx1, tx0, vout_n, redeem_script, 0, rawkeys[:4])  # Sign with key[0:4]

        # Mine some blocks to pass the lock time.
        node.generate(10)

        # Spend the CLTV multi-sig coins.
        raw_tx1 = tx1.serialize()
        hex_raw_tx1 = raw_tx1.hex()
        node.sendrawtransaction(hex_raw_tx1)

        # Check the tx is accepted by mempool but not confirmed.
        assert_equal(sig(node.getreceivedbyaddress(addr_to, 0) - Decimal(0.999)), 0)
        assert_equal(sig(node.getreceivedbyaddress(addr_to, 1)), 0)

        # Mine a block to confirm the tx.
        node.generate(1)
        assert_equal(sig(node.getreceivedbyaddress(addr_to, 1) - Decimal(0.999)), 0)


if __name__ == '__main__':
    BTGTimeLockTest().main()
