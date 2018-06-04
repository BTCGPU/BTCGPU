#!/usr/bin/env python3
# Copyright (c) 2014-2016 The Bitcoin Core developers
# Copyright (c) 2017-2018 The Bitcoin Gold developers
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
        # Block #1999.
        self.log.info("Generating 1799 blocks.")
        node.generate(1799)
        tmpl = node.getblocktemplate()
        assert_equal(tmpl['height'], 2000)
        assert_equal(tmpl['equihashn'], 48)
        assert_equal(tmpl['equihashk'], 5)

        # Block #2000, Equihash<48,5> enabled.
        node.generate(1)
        tmpl = node.getblocktemplate()
        assert_equal(tmpl['height'], 2001)
        assert_equal(tmpl['equihashn'], 96)
        assert_equal(tmpl['equihashk'], 5)

        # Block #2001, Equihash<96,5> enabled.
        node.generate(1)
        tmpl = node.getblocktemplate()
        assert_equal(tmpl['height'], 2002)


if __name__ == '__main__':
    BTGForkTest().main()
