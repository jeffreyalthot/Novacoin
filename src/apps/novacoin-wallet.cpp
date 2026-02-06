#include "blockchain.hpp"
#include "transaction.hpp"
#include "wallet/wallet.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

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
              << "  novacoin-wallet wallet-derive-range <wallet.dat> <start_index> <count> [passphrase]\n"
              << "  novacoin-wallet wallet-addresses <wallet.dat> <count> [passphrase]\n"
              << "  novacoin-wallet wallet-wif <wallet.dat> <index> [passphrase]\n"
              << "  novacoin-wallet wallet-wif-from-hex <wallet.dat> <private_key_hex> [passphrase]\n"
              << "  novacoin-wallet wallet-address <wallet.dat> <index> [passphrase]\n"
              << "  novacoin-wallet wallet-derive-address <wallet.dat> <index> [passphrase]\n"
              << "  novacoin-wallet wallet-hex-from-wif <wallet.dat> <wif> [passphrase]\n"
              << "  novacoin-wallet wallet-public-key <wallet.dat> <index> [passphrase]\n"
              << "  novacoin-wallet wallet-script <wallet.dat> <index> [passphrase]\n"
              << "  novacoin-wallet wallet-validate <wallet.dat> [passphrase]\n"
              << "  novacoin-wallet wallet-ckey <wallet.dat> [passphrase]\n"
              << "  novacoin-wallet wallet-incoming <wallet.dat> [passphrase]\n"
              << "  novacoin-wallet wallet-from-wif <wif>\n"
              << "  novacoin-wallet <address>\n";
}

std::string requireArg(const char* arg, const std::string& message) {
    if (!arg || std::string(arg).empty()) {
        throw std::invalid_argument(message);
    }
    return arg;
}

std::string bytesToHex(const std::vector<std::uint8_t>& bytes) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (auto value : bytes) {
        out.push_back(kHexDigits[(value >> 4) & 0x0F]);
        out.push_back(kHexDigits[value & 0x0F]);
    }
    return out;
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

            if (command == "wallet-derive-address") {
                if (argc < 4 || argc > 5) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                std::size_t index = parseSize(requireArg(argv[3], "Index manquant."), "index");
                std::string passphrase = argc == 5 ? argv[4] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                std::cout << "address=" << walletStore.deriveAddress(static_cast<std::uint32_t>(index), passphrase)
                          << "\n";
                return 0;
            }

            if (command == "wallet-public-key" || command == "wallet-script") {
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
                const std::string publicKey = walletStore.privateKeyHexToPublicKey(privateKeyHex);
                if (command == "wallet-public-key") {
                    std::cout << "public_key_hex=" << publicKey << "\n";
                } else {
                    std::cout << "public_key_script=" << walletStore.publicKeyToPublicKeyScript(publicKey) << "\n";
                }
                return 0;
            }

            if (command == "wallet-validate") {
                if (argc < 3 || argc > 4) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                std::string passphrase = argc == 4 ? argv[3] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                std::cout << "wallet_ok=true\n"
                          << "ckey_len=" << walletStore.ckey().size() << "\n"
                          << "ckey_ts=" << walletStore.ckeyTimestamp() << "\n";
                return 0;
            }

            if (command == "wallet-ckey") {
                if (argc < 3 || argc > 4) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                std::string passphrase = argc == 4 ? argv[3] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                const auto& ckey = walletStore.ckey();
                std::cout << "ckey_hex=" << bytesToHex(ckey) << "\n"
                          << "ckey_len=" << ckey.size() << "\n"
                          << "ckey_ts=" << walletStore.ckeyTimestamp() << "\n";
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

            if (command == "wallet-derive-range") {
                if (argc < 5 || argc > 6) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                std::size_t startIndex = parseSize(requireArg(argv[3], "Index manquant."), "start_index");
                std::size_t count = parseSize(requireArg(argv[4], "Count manquant."), "count");
                std::string passphrase = argc == 6 ? argv[5] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                std::cout << "addresses start=" << startIndex << " count=" << count << "\n";
                for (std::size_t offset = 0; offset < count; ++offset) {
                    const auto index = static_cast<std::uint32_t>(startIndex + offset);
                    std::cout << "  [" << index << "] " << walletStore.deriveAddress(index, passphrase) << "\n";
                }
                return 0;
            }

            if (command == "wallet-addresses") {
                if (argc < 4 || argc > 5) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                std::size_t count = parseSize(requireArg(argv[3], "Count manquant."), "count");
                std::string passphrase = argc == 5 ? argv[4] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                std::cout << "addresses count=" << count << "\n";
                for (std::size_t index = 0; index < count; ++index) {
                    std::cout << "  [" << index << "] "
                              << walletStore.deriveAddress(static_cast<std::uint32_t>(index), passphrase) << "\n";
                }
                return 0;
            }

            if (command == "wallet-incoming") {
                if (argc < 3 || argc > 4) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                std::string passphrase = argc == 4 ? argv[3] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                const auto& incoming = walletStore.incomingTransactions();
                std::cout << "incoming_transactions=" << incoming.size() << "\n";
                for (std::size_t i = 0; i < incoming.size(); ++i) {
                    const auto& tx = incoming[i];
                    std::cout << "  #" << (i + 1) << " id=" << tx.id() << " from=" << tx.from
                              << " to=" << tx.to << " amount=" << std::fixed << std::setprecision(8)
                              << Transaction::toNOVA(tx.amount) << " NOVA fee=" << Transaction::toNOVA(tx.fee)
                              << " NOVA ts=" << tx.timestamp << "\n";
                }
                return 0;
            }

            if (command == "wallet-wif-from-hex") {
                if (argc < 4 || argc > 5) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                const std::string privateKeyHex = requireArg(argv[3], "Cle privee manquante.");
                std::string passphrase = argc == 5 ? argv[4] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                std::cout << "wif=" << walletStore.privateKeyHexToWif(privateKeyHex) << "\n";
                return 0;
            }

            if (command == "wallet-hex-from-wif") {
                if (argc < 4 || argc > 5) {
                    printUsage();
                    return 1;
                }
                const std::string path = requireArg(argv[2], "Chemin wallet.dat manquant.");
                const std::string wif = requireArg(argv[3], "WIF manquant.");
                std::string passphrase = argc == 5 ? argv[4] : "";
                auto walletStore = wallet::WalletStore::load(path, passphrase);
                std::cout << "private_key_hex=" << walletStore.privateKeyHexFromWif(wif) << "\n";
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
