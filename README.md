# Novacoin

Implémentation cohérente d'une blockchain simplifiée en C++ pour le projet Novacoin.

## Fonctionnalités

- Blocs chaînés avec `previousHash`.
- Minage par preuve de travail (difficulté configurable).
- Transactions signées par des adresses (source / destination / montant / frais / timestamp).
- Gestion d'un mempool (`pendingTransactions`) et récompense de minage majorée par les frais collectés.
- Capacité maximale configurable de transactions par bloc (minage fractionné du mempool).
- Vérification d'intégrité complète de la chaîne (`isValid`).
- Vérification des fonds disponibles avant d'ajouter une transaction.
- Historique des transactions par adresse (incluant les transactions en attente).
- Estimation de la prochaine récompense de minage (récompense de base + frais en mempool).
- Statistiques réseau: nombre de blocs et masse monétaire totale émise.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Exécution

```bash
./build/novacoin
```

## Structure

- `include/transaction.hpp` : modèle de transaction.
- `include/block.hpp` : bloc minable + hash de contenu.
- `include/blockchain.hpp` : logique de chaîne, minage, validation, solde et historique.
- `src/main.cpp` : scénario de démonstration complet.
