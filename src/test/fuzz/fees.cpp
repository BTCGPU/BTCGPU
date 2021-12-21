// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <policy/fees.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <util/fees.h>

#include <cstdint>
#include <string>
#include <vector>

void test_one_input(const std::vector<uint8_t>& buffer)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());
    const CFeeRate minimal_incremental_fee{ConsumeMoney(fuzzed_data_provider)};
    FeeFilterRounder fee_filter_rounder{minimal_incremental_fee};
    while (fuzzed_data_provider.ConsumeBool()) {
        const CAmount current_minimum_fee = ConsumeMoney(fuzzed_data_provider);
        const CAmount rounded_fee = fee_filter_rounder.round(current_minimum_fee);
        assert(MoneyRange(rounded_fee));
    }
    const FeeReason fee_reason = fuzzed_data_provider.PickValueInArray({FeeReason::NONE, FeeReason::HALF_ESTIMATE, FeeReason::FULL_ESTIMATE, FeeReason::DOUBLE_ESTIMATE, FeeReason::CONSERVATIVE, FeeReason::MEMPOOL_MIN, FeeReason::PAYTXFEE, FeeReason::FALLBACK, FeeReason::REQUIRED});
    (void)StringForFeeReason(fee_reason);
}
