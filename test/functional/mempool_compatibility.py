#!/usr/bin/env python3
# Copyright (c) 2017-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that mempool.dat is both backward and forward compatible between versions

NOTE: The test is designed to prevent cases when compatibility is broken accidentally.
In case we need to break mempool compatibility we can continue to use the test by just bumping the version number.

Download node binaries:
test/get_previous_releases.py -b v0.19.1 v0.18.1 v0.17.2 v0.16.3 v0.15.2

Only v0.15.2 is required by this test. The rest is used in other backwards compatibility tests.
"""

import os

from test_framework.test_framework import BitcoinTestFramework


class MempoolCompatibilityTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.wallet_names = [None, self.default_wallet_name]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()
        self.skip_if_no_previous_releases()

    def setup_network(self):
        self.add_nodes(self.num_nodes, versions=[
            190100,  # oldest version with getmempoolinfo.loaded (used to avoid intermittent issues)
            None,
        ])
        self.start_nodes()
        self.import_deterministic_coinbase_privkeys()

    def run_test(self):
        self.log.info("Test that mempool.dat is compatible between versions")

        old_node = self.nodes[0]
        new_node = self.nodes[1]
        recipient = old_node.getnewaddress()
        self.stop_node(1)

        self.log.info("Add a transaction to mempool on old node and shutdown")
        old_tx_hash = old_node.sendtoaddress(recipient, 0.0001)
        assert old_tx_hash in old_node.getrawmempool()
        self.stop_node(0)

        self.log.info("Move mempool.dat from old to new node")
        old_node_mempool = os.path.join(old_node.datadir, self.chain, 'mempool.dat')
        new_node_mempool = os.path.join(new_node.datadir, self.chain, 'mempool.dat')
        os.rename(old_node_mempool, new_node_mempool)

        self.log.info("Start new node and verify mempool contains the tx")
        self.start_node(1)
        assert old_tx_hash in new_node.getrawmempool()

        self.log.info("Add unbroadcasted tx to mempool on new node and shutdown")
        unbroadcasted_tx_hash = new_node.sendtoaddress(recipient, 0.0001)
        assert unbroadcasted_tx_hash in new_node.getrawmempool()
        mempool = new_node.getrawmempool(True)
        assert mempool[unbroadcasted_tx_hash]['unbroadcast']
        self.stop_node(1)

        self.log.info("Move mempool.dat from new to old node")
        os.rename(new_node_mempool, old_node_mempool)

        self.log.info("Start old node again and verify mempool contains both txs")
        self.start_node(0, ['-nowallet'])
        assert old_tx_hash in old_node.getrawmempool()
        assert unbroadcasted_tx_hash in old_node.getrawmempool()


if __name__ == "__main__":
    MempoolCompatibilityTest().main()
