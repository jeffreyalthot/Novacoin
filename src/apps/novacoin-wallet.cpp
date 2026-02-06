#include "blockchain.hpp"
#include "transaction.hpp"
#include "wallet/wallet.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
void printUsage() {
    std::cout << "Usage:\n"
              << "  novacoin-wallet balance <address>\n"
              << "  novacoin-wallet history <address> [limit] [--confirmed-only]\n"
              << "  novacoin-wallet stats <address>\n"
              << "  novacoin-wallet summary\n"
              << "  novacoin-wallet wallet-create <wallet.dat> [--encrypt] [passphrase]\n"
              << "  novacoin-wallet wallet-restore <wallet.dat> <wif> [--encrypt] [passphrase]\n"
              << "  novacoin-wallet wallet-info <wallet.dat> [passphrase]\n"
              << "  novacoin-wallet wallet-derive <wallet.dat> <index> [passphrase]\n"
              << "  novacoin-wallet wallet-wif <wallet.dat> <index> [passphrase]\n"
              << "  novacoin-wallet wallet-address <wallet.dat> <index> [passphrase]\n"
              << "  novacoin-wallet wallet-from-wif <wif>\n"
              << "  novacoin-wallet <address>\n";
}

std::string requireArg(const char* arg, const std::string& message) {
    if (!arg || std::string(arg).empty()) {
        throw std::invalid_argument(message);
    }
    return arg;
}

void printBalance(const Blockchain& chain, const std::string& address) {
    std::cout << "Wallet: " << address << "\n"
              << "  confirmed: " << std::fixed << std::setprecision(8) << Transaction::toNOVA(chain.getBalance(address))
              << " NOVA\n"
              << "  available: " << std::fixed << std::setprecision(8)
              << Transaction::toNOVA(chain.getAvailableBalance(address)) << " NOVA\n";
}

std::size_t parseSize(const std::string& raw, const std::string& field) {
    std::size_t consumed = 0;
    const auto value = std::stoull(raw, &consumed);
    if (consumed != raw.size()) {
        throw std::invalid_argument("Valeur invalide pour " + field + ": " + raw);
    }
    return static_cast<std::size_t>(value);
}

void printHistory(const Blockchain& chain, const std::string& address, std::size_t limit, bool confirmedOnly) {
    const auto history = chain.getTransactionHistoryDetailed(address, limit, !confirmedOnly);
    std::cout << "History: " << address << "\n";
    if (history.empty()) {
        std::cout << "  (aucune transaction)\n";
        return;
    }
    for (std::size_t i = 0; i < history.size(); ++i) {
        const auto& entry = history[i];
        std::cout << "  #" << (i + 1) << " id=" << entry.tx.id() << "\n"
                  << "    from=" << entry.tx.from << " to=" << entry.tx.to << "\n"
                  << "    amount=" << std::fixed << std::setprecision(8)
                  << Transaction::toNOVA(entry.tx.amount) << " NOVA\n"
                  << "    fee=" << Transaction::toNOVA(entry.tx.fee) << " NOVA\n"
                  << "    confirmed=" << std::boolalpha << entry.isConfirmed
                  << " confirmations=" << entry.confirmations << "\n";
    }
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            printUsage();
            return 1;
        }

        const std::string command = argv[1];
        if (command.rfind("wallet-", 0) == 0) {
            if (command == "wallet-create") {
                if (argc < 3 || argc > 5) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                bool encrypt = false;
                std::string passphrase;
                if (argc >= 4) {
                    if (std::string(argv[3]) == "--encrypt") {
                        encrypt = true;
                        if (argc == 5) {
                            passphrase = argv[4];
                        }
                    } else {
                        printUsage();
                        return 1;
                    }
                }
                auto walletStore = wallet::WalletStore::createNew(encrypt, passphrase);
                walletStore.save(path);
                std::cout << "Wallet cree: " << path << "\n";
                return 0;
            }

            if (command == "wallet-restore") {
                if (argc < 4 || argc > 6) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                const std::string wif = requireArg(argv[3], "WIF manquant.");
                bool encrypt = false;
                std::string passphrase;
                if (argc >= 5) {
                    if (std::string(argv[4]) == "--encrypt") {
                        encrypt = true;
                        if (argc == 6) {
                            passphrase = argv[5];
                        }
                    } else {
                        printUsage();
                        return 1;
                    }
                }
                auto walletStore = wallet::WalletStore::restoreFromWif(wif, encrypt, passphrase);
                walletStore.save(path);
                std::cout << "Wallet restaure: " << path << "\n";
                return 0;
            }

            if (command == "wallet-info") {
                if (argc < 3 || argc > 4) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                std::string passphrase = argc == 4 ? argv[3] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                std::cout << "master_key_hex=" << walletStore.decryptMasterKeyHex(passphrase) << "\n";
                return 0;
            }

            if (command == "wallet-derive" || command == "wallet-wif" || command == "wallet-address") {
                if (argc < 4 || argc > 5) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                std::size_t index = parseSize(requireArg(argv[3], "Index manquant."), "index");
                std::string passphrase = argc == 5 ? argv[4] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                const std::string privateKeyHex = walletStore.derivePrivateKeyHex(static_cast<std::uint32_t>(index),
                                                                                   passphrase);
                if (command == "wallet-derive") {
                    std::cout << "private_key_hex=" << privateKeyHex << "\n";
                } else if (command == "wallet-wif") {
                    std::cout << "wif=" << walletStore.privateKeyHexToWif(privateKeyHex) << "\n";
                } else {
                    const std::string publicKey = walletStore.privateKeyHexToPublicKey(privateKeyHex);
                    const std::string address = walletStore.publicKeyToAddress(publicKey);
                    std::cout << "public_key_hex=" << publicKey << "\n"
                              << "p2pkh_address=" << address << "\n"
                              << "public_key_script=" << walletStore.publicKeyToPublicKeyScript(publicKey) << "\n";
                }
                return 0;
            }

            if (command == "wallet-from-wif") {
                if (argc != 3) {
                    printUsage();
                    return 1;
                }
                const std::string wif = requireArg(argv[2], "WIF manquant.");
                auto walletStore = wallet::WalletStore::restoreFromWif(wif, false, "");
                std::cout << "private_key_hex=" << walletStore.decryptMasterKeyHex() << "\n";
                return 0;
            }
        }

        Blockchain chain{2, Transaction::fromNOVA(50.0), 10};
        chain.minePendingTransactions("miner");
        chain.createTransaction(Transaction{"miner", "alice", Transaction::fromNOVA(3.5), 1, Transaction::fromNOVA(0.1)});
        chain.createTransaction(Transaction{"miner", "bob", Transaction::fromNOVA(1.25), 2, Transaction::fromNOVA(0.05)});
        chain.minePendingTransactions("miner");

        if (command == "summary") {
            std::cout << chain.getChainSummary();
            return 0;
        }

        if (command == "balance") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const std::string address = argv[2];
            if (address.empty()) {
                throw std::invalid_argument("L'adresse wallet ne peut pas etre vide.");
            }
            printBalance(chain, address);
            std::cout << "  tx_count: " << chain.getTransactionHistory(address).size() << "\n";
            return 0;
        }

        if (command == "history") {
            if (argc < 3 || argc > 5) {
                printUsage();
                return 1;
            }
            const std::string address = argv[2];
            if (address.empty()) {
                throw std::invalid_argument("L'adresse wallet ne peut pas etre vide.");
            }
            std::size_t limit = 0;
            bool confirmedOnly = false;
            for (int i = 3; i < argc; ++i) {
                const std::string arg = argv[i];
                if (arg == "--confirmed-only") {
                    confirmedOnly = true;
                } else if (limit == 0) {
                    limit = parseSize(arg, "limit");
                } else {
                    printUsage();
                    return 1;
                }
            }
            printHistory(chain, address, limit, confirmedOnly);
            return 0;
        }

        if (command == "stats") {
            if (argc != 3) {
                printUsage();
                return 1;
            }
            const std::string address = argv[2];
            if (address.empty()) {
                throw std::invalid_argument("L'adresse wallet ne peut pas etre vide.");
            }
            const auto stats = chain.getAddressStats(address);
            std::cout << "Stats: " << address << "\n"
                      << "  total_received=" << std::fixed << std::setprecision(8)
                      << Transaction::toNOVA(stats.totalReceived) << " NOVA\n"
                      << "  total_sent=" << Transaction::toNOVA(stats.totalSent) << " NOVA\n"
                      << "  fees_paid=" << Transaction::toNOVA(stats.feesPaid) << " NOVA\n"
                      << "  mined_rewards=" << Transaction::toNOVA(stats.minedRewards) << " NOVA\n"
                      << "  pending_outgoing=" << Transaction::toNOVA(stats.pendingOutgoing) << " NOVA\n"
                      << "  outgoing_tx=" << stats.outgoingTransactionCount << "\n"
                      << "  incoming_tx=" << stats.incomingTransactionCount << "\n"
                      << "  mined_blocks=" << stats.minedBlockCount << "\n";
            return 0;
        }

        if (argc != 2) {
            printUsage();
            return 1;
        }

        const std::string address = argv[1];
        if (address.empty()) {
            throw std::invalid_argument("L'adresse wallet ne peut pas etre vide.");
        }

        printBalance(chain, address);
        std::cout << "  tx_count: " << chain.getTransactionHistory(address).size() << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Erreur: " << ex.what() << "\n";
        return 1;
    }
}
