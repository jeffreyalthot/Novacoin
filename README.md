# Novacoin

Implémentation cohérente d'une blockchain simplifiée en C++ pour le projet Novacoin.

## Fonctionnalités

- Blocs chaînés avec `previousHash`.
- Minage par preuve de travail (difficulté configurable).
- Transactions signées par des adresses (source / destination / montant / timestamp).
- Gestion d'un mempool (`pendingTransactions`) et récompense de minage.
- Vérification d'intégrité complète de la chaîne (`isValid`).

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
- `include/blockchain.hpp` : logique de chaîne, minage, validation, solde.
- `src/main.cpp` : scénario de démonstration complet.
