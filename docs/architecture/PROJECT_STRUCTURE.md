# Structure du projet Novacoin

Pour faciliter l'extension du développement, le projet est organisé en dossiers dédiés :

- `src/core/` : composants du noyau partagés (informations build, utilitaires transverses).
- `src/apps/` : points d'entrée applicatifs (CLI, daemon, wallet, regtest).
- `include/core/` : en-têtes publics du noyau transversal.
- `cmake/` : fichiers CMake dédiés à la déclaration des cibles et listes de sources.
- `docs/architecture/` : documentation de structure et conventions d'évolution.

## Convention d'extension

1. Ajouter un nouveau module dans `src/<module>/` et `include/<module>/`.
2. Déclarer ses sources dans `cmake/NovacoinTargets.cmake`.
3. Garder les exécutables dans `src/apps/` pour une séparation claire entre core et apps.

Cette organisation évite la concentration de tout le code au même niveau et simplifie la maintenance.
