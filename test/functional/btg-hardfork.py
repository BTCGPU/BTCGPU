#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test cases for Bitcoin Gold fork """

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *


class BTGForkTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.setup_clean_chain = False

    def run_test(self):
        node = self.nodes[0]

        # Basic block generation test.
        # Block #2999.
        self.log.info("Generating 2799 blocks.")
        node.generate(2799)
        tmpl = node.getblocktemplate()
        assert_equal(tmpl['height'], 3000)

        # Block #3000, Equihash enabled.
        node.generate(1)
        tmpl = node.getblocktemplate()
        assert_equal(tmpl['height'], 3001)


if __name__ == '__main__':
    BTGForkTest().main()
