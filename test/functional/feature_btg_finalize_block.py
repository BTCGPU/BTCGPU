#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the finalizeblock RPC calls."""

import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    set_node_times,
    wait_until,
    connect_nodes_bi,
)

RPC_FINALIZE_INVALID_BLOCK_ERROR = 'finalize-invalid-block'
RPC_FORK_PRIOR_FINALIZED_ERROR = 'bad-fork-prior-finalized'
RPC_BLOCK_NOT_FOUND_ERROR = 'Block not found'


def print_blocks_to_finalized(logger, node):
    logger.info(f'print_block_to_finalized(node{node.index})')
    h_finalized = node.getfinalizedblockhash()
    h = node.getbestblockhash()
    while h != h_finalized:
        header = node.getblockheader(h)
        logger.info(f' {header["height"]} {h} {header["receivedTime"]}')
        h = header['previousblockhash']
    logger.info(f' * {h_finalized}')
    for _ in range(3):
        header = node.getblockheader(h)
        logger.info(f' {header["height"]} {h} {header["receivedTime"]}')
        h = header['previousblockhash']

class FinalizeBlockTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.extra_args = [["-maxreorgdepth=10", "-finalizationdelay=0", "-whitelist=127.0.0.1"],
                           ["-maxreorgdepth=10", "-finalizationdelay=0"],
                           ["-maxreorgdepth=10"],
                           ["-maxreorgdepth=10"]]
        self.finalization_delay = 2 * 60 * 60

    def run_test(self):
        node = self.nodes[0]

        self.mocktime = int(time.time())

        self.log.info("Test block finalization...")
        node.generatetoaddress(10, node.get_deterministic_priv_key()[0])
        tip = node.getbestblockhash()
        node.finalizeblock(tip)
        assert_equal(node.getbestblockhash(), tip)
        assert_equal(node.getfinalizedblockhash(), tip)

        def wait_for_tip(node, tip):
            def check_tip():
                return node.getbestblockhash() == tip
            wait_until(check_tip)

        alt_node = self.nodes[1]
        wait_for_tip(alt_node, tip)

        alt_node.invalidateblock(tip)
        # We will use this later
        fork_block = alt_node.getbestblockhash()

        # Node 0 should not accept the whole alt_node's chain due to tip being finalized,
        # even though it is longer.
        # Headers would not be accepted if previousblock is invalid:
        #    - First block from alt node has same height than node tip, but is on a minority chain. Its
        #    status is "valid-headers"
        #    - Second block from alt node has height > node tip height, will be marked as invalid because
        #    node tip is finalized
        #    - Later blocks from alt node will be rejected because their previous block are invalid
        #
        # Expected state:
        #
        # On alt_node:
        #                           >(210)->(211)-> // ->(218 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210 invalid)
        #
        # On node:
        #                           >(210 valid-headers)->(211 invalid)->(212 to 218 dropped)
        #                          /
        # (200)->(201)-> // ->(209)->(210 finalized, tip)

        def wait_for_block(node, block, status="invalid"):
            def check_block():
                for tip in node.getchaintips():
                    if tip["hash"] == block:
                        assert tip["status"] != "active"
                        return tip["status"] == status
                return False
            wait_until(check_block)

        # First block header is accepted as valid-header
        alt_node.generatetoaddress(
            1, alt_node.get_deterministic_priv_key()[0])
        wait_for_block(node, alt_node.getbestblockhash(), "valid-headers")

        # Second block header is accepted but set invalid
        alt_node.generatetoaddress(
            1, alt_node.get_deterministic_priv_key()[0])
        invalid_block = alt_node.getbestblockhash()
        wait_for_block(node, invalid_block)

        # Later block headers are rejected
        for i in range(2, 9):
            alt_node.generatetoaddress(
                1, alt_node.get_deterministic_priv_key()[0])
            assert_raises_rpc_error(-5, RPC_BLOCK_NOT_FOUND_ERROR,
                                    node.getblockheader, alt_node.getbestblockhash())

        assert_equal(node.getbestblockhash(), tip)
        assert_equal(node.getfinalizedblockhash(), tip)

        self.log.info("Test that an invalid block cannot be finalized...")
        assert_raises_rpc_error(-20, RPC_FINALIZE_INVALID_BLOCK_ERROR,
                                node.finalizeblock, invalid_block)

        self.log.info(
            "Test that invalidating a finalized block moves the finalization backward...")

        # Node's finalized block will be invalidated, which causes the finalized block to
        # move to the previous block.
        #
        # Expected state:
        #
        # On alt_node:
        #                                                 >(210)->(211)-> // ->(218 tip)
        #                                                /
        # (200)->(201)-> // ->(208 auto-finalized)->(209)->(210 invalid)
        #
        # On node:
        #                                     >(210 valid-headers)->(211 invalid)->(212 to 218 dropped)
        #                                    /
        # (200)->(201)-> // ->(209 finalized)->(210 tip)
        node.invalidateblock(tip)
        node.reconsiderblock(tip)

        assert_equal(node.getbestblockhash(), tip)
        assert_equal(node.getfinalizedblockhash(), fork_block)

        assert_equal(alt_node.getfinalizedblockhash(), node.getblockheader(
            node.getfinalizedblockhash())['previousblockhash'])

        # The node will now accept that chain as the finalized block moved back.
        # Generate a new block on alt_node to trigger getheader from node
        # Previous 212-218 height blocks have been droped because their previous was invalid
        #
        # Expected state:
        #
        # On alt_node:
        #                                          >(210)->(211)-> // ->(218)->(219 tip)
        #                                         /
        # (200)->(201)-> // ->(209 auto-finalized)->(210 invalid)
        #
        # On node:
        #                                     >(210)->(211)->(212)-> // ->(218)->(219 tip)
        #                                    /
        # (200)->(201)-> // ->(209 finalized)->(210)
        node.reconsiderblock(invalid_block)

        alt_node_tip = alt_node.generatetoaddress(
            1, alt_node.get_deterministic_priv_key()[0])[-1]
        wait_for_tip(node, alt_node_tip)

        assert_equal(node.getbestblockhash(), alt_node.getbestblockhash())
        assert_equal(node.getfinalizedblockhash(), fork_block)
        assert_equal(alt_node.getfinalizedblockhash(), fork_block)

        self.log.info("Trigger reorg via block finalization...")
        # Finalize node tip to reorg
        #
        # Expected state:
        #
        # On alt_node:
        #                                          >(210)->(211)-> // ->(218)->(219 tip)
        #                                         /
        # (200)->(201)-> // ->(209 auto-finalized)->(210 invalid)
        #
        # On node:
        #                           >(210 invalid)-> // ->(219 invalid)
        #                          /
        # (200)->(201)-> // ->(209)->(210 finalized, tip)
        node.finalizeblock(tip)
        assert_equal(node.getfinalizedblockhash(), tip)

        self.log.info("Try to finalize a block on a competiting fork...")
        assert_raises_rpc_error(-20, RPC_FINALIZE_INVALID_BLOCK_ERROR,
                                node.finalizeblock, alt_node.getbestblockhash())
        assert_equal(node.getfinalizedblockhash(), tip)

        self.log.info(
            "Check auto-finalization occurs as the tip move forward...")
        # Reconsider alt_node tip then generate some more blocks on alt_node.
        # Auto-finalization will occur on both chains.
        #
        # Expected state:
        #
        # On alt_node:
        #                           >(210)->(211)-> // ->(219 auto-finalized)-> // ->(229 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210 invalid)
        #
        # On node:
        #                           >(210)->(211)-> // ->(219 auto-finalized)-> // ->(229 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210 invalid)
        node.reconsiderblock(alt_node.getbestblockhash())
        block_to_autofinalize = alt_node.generatetoaddress(
            1, alt_node.get_deterministic_priv_key()[0])[-1]
        alt_node_new_tip = alt_node.generatetoaddress(
            9, alt_node.get_deterministic_priv_key()[0])[-1]
        wait_for_tip(node, alt_node_new_tip)

        assert_equal(node.getbestblockhash(), alt_node.getbestblockhash())
        assert_equal(node.getfinalizedblockhash(), alt_node_tip)
        assert_equal(alt_node.getfinalizedblockhash(), alt_node_tip)

        self.log.info(
            "Try to finalize a block on an already finalized chain...")
        # Finalizing a block of an already finalized chain should have no
        # effect
        block_218 = node.getblockheader(alt_node_tip)['previousblockhash']
        node.finalizeblock(block_218)
        assert_equal(node.getfinalizedblockhash(), alt_node_tip)

        self.log.info(
            "Make sure reconsidering block move the finalization point...")
        # Reconsidering the tip will move back the finalized block on node
        #
        # Expected state:
        #
        # On alt_node:
        #                           >(210)->(211)-> // ->(219 auto-finalized)-> // ->(229 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210 invalid)
        #
        # On node:
        #                                     >(210)->(211)-> // ->(219)-> // ->(229 tip)
        #                                    /
        # (200)->(201)-> // ->(209 finalized)->(210)
        node.reconsiderblock(tip)

        assert_equal(node.getbestblockhash(), alt_node_new_tip)
        assert_equal(node.getfinalizedblockhash(), fork_block)

        # TEST FINALIZATION DELAY

        self.log.info("Check that finalization delay prevents eclipse attacks")
        # Because there has been no delay since the beginning of this test,
        # there should have been no auto-finalization on delay_node.
        #
        # Expected state:
        #
        # On alt_node:
        #                           >(210)->(211)-> // ->(219 auto-finalized)-> // ->(229 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210 invalid)
        #
        # On delay_node:
        #                           >(210)->(211)-> // ->(219)-> // ->(229 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210)
        delay_node = self.nodes[2]
        alt_delay_node = self.nodes[3]

        wait_for_tip(delay_node, alt_node_new_tip)
        assert_equal(delay_node.getfinalizedblockhash(), str())

        self.log.info(
            "Check that finalization delay does not prevent auto-finalization")
        # Expire the delay, then generate 1 new block with alt_node to
        # update the tip on all chains.
        # Because the finalization delay is expired, auto-finalization
        # should occur.
        #
        # Expected state:
        #
        # On alt_node:
        #                           >(220 auto-finalized)-> // ->(230 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210 invalid)
        #
        # On delay_node:
        #                           >(220 auto-finalized)-> // ->(230 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210)
        self.mocktime += self.finalization_delay
        set_node_times([delay_node, alt_delay_node], self.mocktime)
        new_tip = alt_node.generatetoaddress(
            1, alt_node.get_deterministic_priv_key()[0])[-1]

        assert_equal(alt_node.getbestblockhash(), new_tip)
        assert_equal(alt_node.getfinalizedblockhash(), block_to_autofinalize)

        wait_for_tip(node, new_tip)
        assert_equal(node.getfinalizedblockhash(), block_to_autofinalize)

        wait_for_tip(delay_node, new_tip)
        self.log.info(
            "Check that finalization delay is effective on node boot")
        # Restart the new node, so the blocks have no header received time.
        self.restart_node(2)
        self.restart_node(3)
        # Connect the two delayed nodes
        connect_nodes_bi(self.nodes, 2, 3)

        # There should be no finalized block (getfinalizedblockhash returns an
        # empty string)
        assert_equal(delay_node.getfinalizedblockhash(), str())
        assert_equal(alt_delay_node.getfinalizedblockhash(), str())

        # Generate 20 blocks with no delay. This should not trigger auto-finalization.
        #
        # Expected state:
        #
        # On delay_node:
        #                           >(220)-> // ->(250 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210)
        blocks = delay_node.generatetoaddress(
            20, alt_node.get_deterministic_priv_key()[0])
        reboot_autofinalized_block = blocks[10]
        new_tip = blocks[-1]
        wait_for_tip(delay_node, new_tip)

        assert_equal(delay_node.getfinalizedblockhash(), str())

        # Now let the finalization delay to expire, then generate one more block.
        # This should resume auto-finalization.
        #
        # Expected state:
        #
        # On delay_node:
        #                           >(220)-> // ->(241 auto-finalized)-> // ->(251 tip)
        #                          /
        # (200)->(201)-> // ->(209)->(210)
        self.mocktime += self.finalization_delay
        set_node_times([delay_node, alt_delay_node], self.mocktime)
        new_tip = delay_node.generatetoaddress(
            1, delay_node.get_deterministic_priv_key()[0])[-1]
        wait_for_tip(delay_node, new_tip)

        assert_equal(delay_node.getfinalizedblockhash(),
                     reboot_autofinalized_block)

        self.log.info("Check block delay edge cases")
        wait_for_tip(alt_delay_node, new_tip)

        '''
        Test plan:
            n2 mine 1 block
                n2: 0 1
                n3: 0 1
            within 2 hours, n3 invalidate 1, mine 1 public block, and followed by 11 more private blocks (more work).
            the same time n2 mine 10 blocks
                n2: 0 1   2  .. 11
                n3: 0 1' (2' .. 11' 12')
            now the 2 hour delay has passed. imagine now the honest chain hasn't mine a new block to activate the the checkpoint at 1.
            n3 can broadcast the blockchain, create an reorg, and trigger n2 to mark 1' as the checkpoint, though 1 was mined even
            before 1', though 1 is already eligible to be checkpointed.
                      1 .. 11 (less work fork)
                     /
                n2: 0 [1'] .. 12'
                n3: 0 [1'] .. 12'
        '''

        # finalize the current tip
        delay_node.finalizeblock(new_tip)
        alt_delay_node.finalizeblock(new_tip)
        finalized_block0 = new_tip

        # n2 mine 1 block
        #     n2: 0 1
        #     n3: 0 1
        honest1 = delay_node.generatetoaddress(1, node.get_deterministic_priv_key()[0])[-1]
        wait_for_tip(alt_delay_node, honest1)

        # 1 min passed
        self.mocktime += 1 * 60
        set_node_times([delay_node, alt_delay_node], self.mocktime)

        # within 2 hours, n3 invalidate 1, mine 1 public block, and followed by 11 more private blocks (more work).
        # the same time n2 mine 10 blocks
        #     n2: 0 1   2  .. 10
        #     n3: 0 1' (2' .. 10' 11')
        alt_delay_node.invalidateblock(honest1)
        attack_block1 = alt_delay_node.generatetoaddress(1, node.get_deterministic_priv_key()[0])[-1]
        wait_for_block(delay_node, attack_block1, status="valid-headers")

        # n3 mines the private chain
        alt_delay_node.setnetworkactive(False)
        wait_until(lambda: alt_delay_node.getnetworkinfo()['connections'] == 0, timeout=3)
        attack_blocks = alt_delay_node.generatetoaddress(11, node.get_deterministic_priv_key()[0])
        # n2 mines the public chain
        honest_blocks = delay_node.generatetoaddress(10, node.get_deterministic_priv_key()[0])

        wait_for_tip(delay_node, honest_blocks[-1])
        wait_for_tip(alt_delay_node, attack_blocks[-1])

        # now both nodes still don't have any new block finalized
        assert_equal(finalized_block0, delay_node.getfinalizedblockhash())
        assert_equal(finalized_block0, alt_delay_node.getfinalizedblockhash())

        # now the 2 hour delay passed. imagine now the honest chain hasn't mine a new block to activate the the checkpoint at 1.
        # n3 broadcast the blockchain leading to a reorg, and trigger n2 to mark 1' as the checkpoint, though 1 was mined even
        # before 1', though 1 is already eligible to be checkpointed.
        self.mocktime += self.finalization_delay
        set_node_times([delay_node, alt_delay_node], self.mocktime)
        alt_delay_node.setnetworkactive(True)
        connect_nodes_bi(self.nodes, 2, 3)

        # we should see the attacking blocks take over the honest chain
        wait_for_tip(delay_node, attack_blocks[-1])
        # the attacking chain got finalized!
        self.log.info(f'attacking block1: {attack_block1}')
        print_blocks_to_finalized(self.log, delay_node)
        print_blocks_to_finalized(self.log, alt_delay_node)
        assert_equal(attack_block1, delay_node.getfinalizedblockhash())


if __name__ == '__main__':
    FinalizeBlockTest().main()
