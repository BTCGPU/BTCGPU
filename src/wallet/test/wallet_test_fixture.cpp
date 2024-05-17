// Copyright (c) 2016-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/test/wallet_test_fixture.h>

WalletTestingSetup::WalletTestingSetup(const std::string& chainName)
    : TestingSetup(chainName),
      m_wallet(m_chain.get(), "", CreateMockWalletDatabase())
{
    bool fFirstRun;
    m_wallet.LoadWallet(fFirstRun);
    m_chain_notifications_handler = m_chain->handleNotifications({ &m_wallet, [](CWallet*) {} });
    m_wallet_client->registerRpcs();
}
