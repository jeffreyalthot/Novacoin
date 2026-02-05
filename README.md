# Novacoin

Implémentation cohérente d'une blockchain simplifiée en C++ pour le projet Novacoin.

## Vision du projet

Construire **Novacoin** comme une blockchain **Proof of Work (PoW)** robuste, progressive et auditable, avec une politique monétaire stricte plafonnée à **29 000 000.00000000 NOVA**.

Ce README sert de **plan directeur complet** pour poser les bases techniques, économiques, de sécurité et d'exploitation du réseau.

---

## Principes fondateurs

- **Consensus** : Preuve de Travail (PoW) uniquement.
- **Offre maximale** : `29_000_000.00000000 NOVA` (hard cap non négociable).
- **Précision monétaire** : 8 décimales (unité minimale = `0.00000001 NOVA`).
- **Validation déterministe** : mêmes règles pour tous les nœuds.
- **Sécurité d'abord** : priorité à l'intégrité de la chaîne, à la résistance aux attaques et à la simplicité vérifiable.
- **Développement incrémental** : livrer par phases avec critères d'acceptation clairs.

---

## Plan complet de développement

## Phase 0 — Cadrage & Spécification (fondations)

### Objectifs

- Définir la spécification officielle du protocole Novacoin.
- Geler les règles de consensus de la v1.
- Poser les conventions de codage, de test et de publication.

### Livrables

- Document de spécification protocolaire (format bloc, transaction, validation, consensus).
- Règles de sérialisation canonique (endianness, encodages, versions).
- Stratégie de versioning (compatibilité réseau et hardfork planifiés).

### Critères d'acceptation

- Toute règle de validation est documentée de manière déterministe.
- Deux implémentations indépendantes produisent le même hash pour les mêmes données.

---

## Phase 1 — Noyau blockchain local (single-node)

### Objectifs

- Mettre en place une chaîne valide localement.
- Implémenter le minage PoW et la validation complète des blocs.

### Fonctionnalités clés

- Blocs chaînés (`previousHash`) avec horodatage.
- Transactions UTXO-like simplifiées ou account-based (à figer en Phase 0).
- Mempool local et inclusion des transactions en blocs.
- Vérification d'intégrité de bout en bout (`isValidChain`).
- Gestion des frais de transaction.

### Critères d'acceptation

- Minage d'un bloc valide avec cible de difficulté respectée.
- Rejet des blocs invalides (hash, signature, double dépense, montant négatif, etc.).

---

## Phase 2 — Économie NOVA & Hard Cap 29M

### Objectifs

- Garantir cryptographiquement que l'émission totale ne dépasse jamais `29_000_000.00000000 NOVA`.

### Règles monétaires à implémenter

- Récompense de bloc initiale définie (ex: `R0`).
- Mécanisme de réduction (halving ou courbe décroissante prédéfinie).
- Addition de la récompense + frais au mineur.
- Vérification à la validation de bloc :
  - `coinbase <= reward_height + total_fees_block`
  - `supply_total <= 29_000_000.00000000`

### Critères d'acceptation

- Tests de simulation longue (jusqu'à extinction des récompenses).
- Assertion stricte côté consensus : impossible de créer 1 satoshi NOVA au-delà du cap.

---

## Phase 3 — Consensus PoW production-ready

### Objectifs

- Rendre le consensus stable sur de longues périodes et sous variation du hashrate.

### Fonctionnalités clés

- Algorithme de difficulté dynamique (retarget périodique ou par bloc).
- Gestion des réorganisations de chaîne (fork choice : chaîne la plus difficile/cumulative work).
- Validation stricte des timestamps (bornes dérive temporelle).
- Protection contre blocs anormaux (taille, poids, sigops selon modèle choisi).

### Critères d'acceptation

- Le réseau se réajuste après simulation de chute/hausse brutale du hashrate.
- Les nœuds convergent vers la même chaîne canonique.

---

## Phase 4 — Réseau P2P (multi-nœuds)

### Objectifs

- Passer d'une chaîne locale à un réseau distribué de nœuds synchronisés.

### Fonctionnalités clés

- Découverte de pairs (seed nodes, DNS seeds éventuels).
- Messages P2P minimaux : `version`, `verack`, `inv`, `getdata`, `block`, `tx`.
- Synchronisation initiale de la chaîne (IBD simplifiée).
- Politique anti-spam et limitation de débit.

### Critères d'acceptation

- Plusieurs nœuds démarrés sur machines différentes se synchronisent.
- Propagation d'un nouveau bloc sur l'ensemble du réseau dans un délai mesurable.

---

## Phase 5 — Stockage, persistance & performances

### Objectifs

- Assurer la durabilité des données et de bonnes performances de validation.

### Fonctionnalités clés

- Persistance des blocs, index de transactions et état (UTXO/compte).
- Reprise après crash sans corruption logique.
- Cache mémoire pour accélérer validation et requêtes.
- Outils de reindexation et de vérification de base.

### Critères d'acceptation

- Redémarrage nœud avec restauration cohérente de l'état.
- Benchmarks documentés (temps de sync, validation par bloc).

---

## Phase 6 — Wallet, signatures & sécurité utilisateur

### Objectifs

- Permettre l'utilisation réelle de NOVA avec sécurité minimale robuste.

### Fonctionnalités clés

- Génération d'adresses et gestion de clés (format standardisé).
- Signature et vérification des transactions.
- Portefeuille local (solde, historique, envoi, frais).
- Export/import sécurisé des clés (chiffrement recommandé).

### Critères d'acceptation

- Transactions signées valides de bout en bout entre deux wallets.
- Tests contre signatures invalides, rejouées ou mal formées.

---

## Phase 7 — API, CLI & observabilité

### Objectifs

- Faciliter l'exploitation développeur, opérateur et intégrateur.

### Fonctionnalités clés

- CLI nœud (`start`, `status`, `mine`, `send`, `getblock`, `gettx`).
- API RPC/REST versionnée.
- Métriques (hashrate local, hauteur, peers, mempool, orphan rate).
- Logs structurés (niveaux, corrélation, événements consensus).

### Critères d'acceptation

- Opérations principales réalisables sans modifier le code.
- Monitoring basique possible via endpoint métriques.

---

## Phase 8 — Tests, qualité & audit sécurité

### Objectifs

- Atteindre un niveau de fiabilité compatible avec un réseau ouvert.

### Plan qualité

- Tests unitaires (hash, signatures, règles de validation).
- Tests d'intégration (mempool → bloc → propagation → réorg).
- Tests de propriété/fuzzing (parsers, sérialisation, transactions invalides).
- Tests de charge (mempool massif, sync longue, forks fréquents).
- Revue de sécurité externe (audit ciblé consensus + wallet).

### Critères d'acceptation

- Couverture pertinente sur composants critiques.
- Aucun bug consensus de sévérité critique ouvert avant testnet public.

---

## Phase 9 — Testnet, gouvernance technique & lancement mainnet

### Objectifs

- Lancer Novacoin de manière maîtrisée et transparente.

### Étapes

1. **Devnet interne** : validation continue des nouveautés.
2. **Testnet public** : stress réel, retours communauté.
3. **Freeze protocole v1** : verrouillage des paramètres consensus.
4. **Genesis mainnet** : lancement officiel et monitoring 24/7.
5. **Post-launch** : runbook incidents, politique de patchs, roadmap v2.

### Critères d'acceptation

- Comportement stable plusieurs semaines en testnet.
- Documentation opérateur complète avant mainnet.

---

## Paramètres consensus à figer dès le départ

- Algorithme PoW exact.
- Temps cible de bloc.
- Fenêtre/algorithme de retarget difficulté.
- Récompense initiale et calendrier de réduction.
- Règles coinbase (maturité, format, plafond par bloc).
- Taille/poids maximal des blocs.
- Règles mempool minimales (frais minimum, standardness).
- Checkpoints éventuels (uniquement bootstrap, jamais consensus caché).

---

## Politique monétaire de Novacoin (exigence centrale)

- **Supply maximale absolue** : `29_000_000.00000000 NOVA`.
- Cette limite doit être protégée par :
  - des règles de consensus ;
  - des tests de non-régression ;
  - des vérifications explicites à chaque bloc.
- Aucun mode opératoire (RPC, wallet, minage, import) ne doit contourner ce plafond.

---

## Organisation recommandée du dépôt

- `include/` : interfaces des composants cœur.
- `src/` : implémentation nœud, consensus, réseau, wallet.
- `tests/` : unitaires, intégration, fuzz/property.
- `docs/` : spécification protocole, runbooks, architecture.
- `scripts/` : outillage build, simulation, diagnostic.

---

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Exécution

```bash
./build/novacoin
```

## Prochaines actions immédiates (Sprint 1)

- Finaliser la spécification de consensus v1.
- Implémenter les assertions du hard cap `29_000_000.00000000` dans la validation de bloc.
- Ajouter un jeu de tests monétaires (émission cumulée, halving, plafond).
- Mettre en place les premiers tests d'intégration multi-blocs.
- Documenter les paramètres de genesis (timestamp, difficulté initiale, message genesis).

---

## Travaux réalisés dans cette itération

- Intégration d'un **hard cap effectif** (`29_000_000.00000000 NOVA`) appliqué pendant la création de transactions `network`, le minage et la validation complète de chaîne.
- Ajout d'une **récompense de bloc décroissante** (halving simplifié) pour amorcer la politique monétaire long terme.
- Passage du minage à une coinbase incluse directement dans le bloc miné (récompense + frais), avec plafond strict.
- Durcissement de la validation des transactions avec rejet des montants/frais non finis (`NaN`, `Inf`) et maintien des contraintes `amount` / `fee`.
- Ajout d'un identifiant de transaction local (`Transaction::id`) pour améliorer la traçabilité et permettre la détection de doublons.
- Rejet des **transactions dupliquées en mempool** (même payload sérialisé).
- Ajout d'un **template local de sélection de transactions** (`getPendingTransactionsForBlockTemplate`) pour préparer l'API mineur.
- Ajout d'un **résumé de chaîne** (`getChainSummary`) pour l'observabilité minimale côté CLI.
- Extension de la suite de tests automatisés C++ (hard cap, rejet de mint illégal, récompenses avec frais, doublons mempool, capacité template bloc).
- Mise à jour du build CMake pour séparer `novacoin_core`, exécutable principal et exécutable de tests.
- Ajout d'une **politique de mempool minimale** avec frais plancher (`kMinRelayFee`) pour filtrer les transactions non économiques.
- Ajout d'un **filtre anti-horloge** : rejet des transactions trop dans le futur, et contrôles d'horodatage côté validation de chaîne.
- Priorisation du template de bloc par frais (ordre décroissant) avec pré-validation de solvabilité projetée, pour améliorer l'efficacité du minage local.

## Travaux supplémentaires à effectuer (roadmap enrichie)

### Priorité P0 — Consensus & sécurité monétaire

1. **Geler les paramètres monétaires v1** dans un fichier de config consensus immuable (halving interval, récompense initiale, précision satoshi NOVA).
2. **Remplacer les montants `double` par des unités entières** (satoshi NOVA) pour éliminer définitivement les risques d'arrondi.
3. **Introduire une vraie transaction coinbase** avec format dédié, maturité configurable et règles explicites par hauteur.
4. **Rendre déterministe la sérialisation des transactions et blocs** (endianness, tailles fixes/varint, versionnage) pour préparer un hash inter-plateformes robuste.
5. **Ajouter une vérification explicite de non-overflow arithmétique** sur tous les calculs monétaires et cumuls de frais/récompenses.

### Priorité P1 — Robustesse nœud local

6. **Ajouter une suite de tests de simulation longue** (plusieurs centaines de milliers de blocs) pour prouver extinction des récompenses et invariants de supply.
7. **Valider les timestamps blocs/transactions** (fenêtres max/min, monotonicité partielle, règles anti-dérive).
8. **Implémenter une politique de mempool** (frais minimum, remplacement optionnel, limites par adresse et taille globale).
9. **Renforcer la logique anti-double-dépense locale** entre transactions pending conflictuelles.
10. **Ajouter un mode relecture/validation complète** au démarrage avec rapport d'erreurs structuré.
11. **Ajouter une politique d'expiration mempool (TTL)** avec purge périodique et bornes de taille adaptatives.
12. **Introduire une priorisation avancée de sélection de transactions** (package fee-rate, dépendances parent/enfant).
13. **Ajouter une stratégie de protection horloge nœud** (median-time-past, détection de dérive locale, fallback NTP).

### Priorité P2 — Outillage développeur & exploitation

14. **Structurer un dossier `docs/`** avec spécification protocolaire v1, runbook d'exploitation et guide incident.
15. **Débuter l'API CLI** (`status`, `mine`, `send`, `getblock`, `gettx`) pour piloter le nœud sans modifier le code.
16. **Ajouter des métriques exportables** (hauteur, hashrate local, taille mempool, reward estimée, supply courante).
17. **Ajouter des logs structurés** pour les événements consensus (validation bloc, rejet tx, minage réussi, erreurs invariants).
18. **Documenter un plan de test reproductible** (scripts build+test, seed fixe, cas limite, checklist release).
19. **Ajouter des scénarios de chaos engineering local** (faux timestamps, blocs mal formés, pression mempool) automatisés en CI.
20. **Créer un tableau de compatibilité multi-plateforme** (Linux/macOS/Windows, versions compilateur, sanitizers).

### Priorité P3 — Préparation réseau & persistance

21. **Créer une couche de persistance locale** (blocs + index minimal) avec reprise après crash.
22. **Mettre en place un format de snapshot d'état** pour accélérer bootstrap et vérification.
23. **Préparer le protocole P2P minimal** (`version`, `verack`, `inv`, `getdata`, `block`, `tx`) avec validation stricte des messages.
24. **Définir une stratégie de gestion des forks/réorgs** (travail cumulé, règles d'adoption, gestion d'orphelins).
25. **Planifier un devnet multi-nœuds** avec objectifs de performance et scénarios d'attaque simulés.
26. **Préparer un mode de stockage append-only avec checksums** pour détecter toute corruption disque silencieuse.
27. **Définir une stratégie de bootstrapping sécurisé** (checkpoints signés, validation progressive, reprise interrompue).
