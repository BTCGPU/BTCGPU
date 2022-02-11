#!/usr/bin/env python3
# Copyright (c) 2012-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
convert btc address to btg address.

'''
# 2012 Wladimir J. van der Laan
# Released under MIT License
import os
from base58 import b58encode_chk, b58decode_chk


if __name__ == '__main__':

    addr_list = ["3B5fQsEXEaV8v6U3ejYc8XaKXAkyQj2MjV", 
                 "1KqbBpLy5FARmTPD4VZnDDpYjkUvkr82Pm",
                 "15CRxFdyRpGZLW9w8HnHvVduizdL5jKNbs",
                 "11canuhp9X2NocwCq7xNrQYTmUgZAnLK3"]

    for addr in addr_list:
        print("BTC:", addr)
        raw = b58decode_chk(addr)
        raw_array = bytearray(raw)
        if (raw_array[0] == 5):
            raw_array[0] = 23
        elif (raw_array[0] == 0):
            raw_array[0] = 38

        res = b58encode_chk(raw_array)
        print("---BTG:", res)
