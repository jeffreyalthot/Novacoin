# Structure du projet Novacoin

Pour faciliter l'extension du développement, le projet est organisé en dossiers dédiés :

- `src/core/` : composants du noyau partagés (informations build, utilitaires transverses).
- `src/consensus/` : paramètres de consensus partageables (timings, règles réseau).
- `src/mempool/` : politique mempool et règles d’acceptation transactionnelle.
- `src/validation/` : validateurs spécialisés (ex: forme basique des blocs).
- `src/rpc/` : structures de contexte et couches de service RPC.
- `src/wallet/` : profilage wallet et métadonnées utilisateur.
- `src/network/` : primitives de gestion de pairs, seeds de bootstrap et orchestration réseau.
- `src/storage/` : snapshots et briques liées à la persistance.
- `src/utils/` : outils transverses (journalisation, helpers techniques).
- `src/apps/` : points d'entrée applicatifs (CLI, daemon, wallet, regtest).
- `include/core/` : en-têtes publics du noyau transversal.
- `include/consensus/` : interfaces publiques des paramètres de consensus.
- `include/mempool/` : interfaces publiques de politique mempool.
- `include/validation/` : interfaces publiques des validateurs dédiés.
- `include/rpc/` : interfaces publiques de contexte RPC.
- `include/wallet/` : interfaces publiques liées aux profils wallet.
- `include/network/` : interfaces publiques du module réseau.
- `include/storage/` : interfaces publiques du module stockage.
- `include/utils/` : interfaces publiques des utilitaires.
- `cmake/` : fichiers CMake dédiés à la déclaration des cibles et listes de sources.
- `docs/architecture/` : documentation de structure et conventions d'évolution.

## Convention d'extension

1. Ajouter un nouveau module dans `src/<module>/` et `include/<module>/`.
2. Déclarer ses sources dans `cmake/NovacoinTargets.cmake`.
3. Garder les exécutables dans `src/apps/` pour une séparation claire entre core et apps.

Cette organisation évite la concentration de tout le code au même niveau et simplifie la maintenance.

## Extensions de structure ajoutées

Pour rendre l'arborescence immédiatement exploitable, les dossiers historiquement vides disposent désormais d'un `README.md` local qui documente leur rôle :

- `src/test/` pour les tests C++ internes au noyau,
- `test/functional/`, `test/fuzz/`, `test/util/` pour la hiérarchie de tests inspirée de Bitcoin Core,
- `doc/release-notes/` pour les notes de version,
- `share/examples/` et `share/rpcauth/` pour les ressources opérateur/RPC,
- `m4/` pour la compatibilité de structure avec des workflows Autotools.

Cette approche permet de "continuer à étendre les dossiers et fichiers" sans attendre l'implémentation complète de chaque module, tout en gardant des conventions explicites.
