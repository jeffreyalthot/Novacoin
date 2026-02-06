# Roadmap d'amélioration (AGENT)

Ce document propose une **roadmap exhaustive** pour anticiper le maximum d'améliorations possibles de Novacoin Core. Il sert de guide de planification et de priorisation technique.

## Objectifs généraux

- Stabiliser un consensus PoW production-ready.
- Garantir l'intégrité du hard cap **29 000 000.00000000 NOVA**.
- Assurer la scalabilité, la sécurité, et l'observabilité du réseau.
- Industrialiser le build, les tests, et la distribution.

---

## 1) Gouvernance technique & spécifications (Priorité: Très haute)

- **Spécification protocolaire v1** (formats bloc/tx, règles UTXO ou comptes, sérialisation, endianness).
- **Matrice de compatibilité** (versioning, règles de hardfork planifiées).
- **Règles de consensus** verrouillées + document de justification cryptographique.
- **Chiffrage de sécurité** (modèle de menaces, hypothèses réseau, attaques connues).

---

## 2) Consensus & validation (Priorité: Très haute)

- **Retarget PoW**: algorithme précis, bornes d'ajustement, tests de résilience.
- **Fork-choice**: cumulative work avec index persistant multi-branches.
- **Validation stricte**: timestamps, poids des blocs, sigops, limites mempool.
- **Réorgs profondes**: remise en mempool, rollbacks cohérents.
- **Paramètres de bloc**: taille/poids max, fenêtre de difficulté, median time past.

---

## 3) Économie & émission monétaire (Priorité: Très haute)

- **Courbe d'émission** définitive (halvings, rythme, durée).
- **Validation supply**: invariants sur reward + fees + hard cap.
- **Tests d'émission** à long terme (simulation complète du cycle monétaire).
- **Outillage d'audit**: calcul d'offre, explorer interne.

---

## 4) Réseau P2P (Priorité: Haute)

- **Découverte de pairs**: seeds, DNS seeds, bootstrap.
- **Gossip blocs/tx**: inventaire, anti-spam, protection DoS.
- **Headers-first sync**: pipeline complet (headers, checkpoints, blocks).
- **Protection réseau**: banning, rate limiting, scoring peers.
- **Compatibilité NAT**: UPnP optionnel, documentation.

---

## 5) Stockage & persistance (Priorité: Haute)

- **Block index** persistant (multi-branches, orphans).
- **Base UTXO** optimisée (cache, pruning).
- **Snapshots** et bootstrap state (fast sync).
- **Stratégie de pruning**: modes plein vs pruned.

---

## 6) Wallet & UX (Priorité: Moyenne)

- **Wallet HD** (BIP32-like), formats d'adresses standardisés.
- **Gestion des clés**: chiffrage, rotation, backups.
- **Mempool wallet-aware**: sélection d'UTXO, estimation de frais.
- **Outils CLI**: commandes avancées (labeling, watch-only, rescan).

---

## 7) API & RPC (Priorité: Moyenne)

- **RPC stable** (noms, formats, erreurs, versioning).
- **Auth & RBAC**: accès sécurisé (tokens, whitelist).
- **Monitoring**: stats réseau, chainstate, mempool.
- **SDKs**: clients minimalistes pour intégration (JS/Python).

---

## 8) Observabilité & logging (Priorité: Moyenne)

- **Logs structurés** (JSON, niveaux, correlation ID).
- **Metrics**: Prometheus/OpenMetrics.
- **Dashboards**: performance P2P, consensus, mempool.
- **Tracing**: suivi des transactions dans la pipeline.

---

## 9) Sécurité (Priorité: Haute)

- **Audit interne** des règles consensus.
- **Fuzzing** des parsers blocs/tx.
- **Protection DoS**: limites, timeouts, anti-malleability.
- **Signatures**: validation stricte, formats sécurisés.
- **Supply invariant** testé et prouvé.

---

## 10) Tests & QA (Priorité: Très haute)

- **Tests unitaires** par module (consensus, mempool, validation).
- **Tests fonctionnels** (réorg, forks, resync, halving).
- **Fuzzing** (mempool, tx parsing, net messages).
- **Golden tests**: vecteurs de blocs/tx.
- **CI**: matrices OS/compilateurs, coverage, sanitize.

---

## 11) Performance & scalabilité (Priorité: Moyenne)

- **Profiling** CPU/mémoire.
- **Optimisation mempool** (indexation, eviction policies).
- **Compression** (block storage, wire protocol).
- **Caches** ciblés (UTXO, block headers).

---

## 12) Release engineering & distribution (Priorité: Moyenne)

- **Build reproducible** (deterministic builds).
- **Signatures des binaires** (GPG).
- **Packaging** (deb/rpm, archives).
- **Release notes** standardisées.

---

## 13) Documentation & communauté (Priorité: Moyenne)

- **Docs opérateur** (node setup, firewall, monitoring).
- **Docs développeur** (architecture, flows, consensus rules).
- **Guides contributeur** (lint, style, tests).
- **Changelog** et roadmap publique.

---

## 14) Testnet / Mainnet (Priorité: Haute)

- **Testnet stable** avec paramètres indépendants.
- **Bootstrap testnet** (seeds, checkpoints).
- **Plan de lancement** mainnet (genesis, audits, bug bounty).

---

## 15) Backlog long terme

- **Améliorations cryptographiques** (Schnorr, MuSig, taproot-like si pertinent).
- **Sidechains** ou L2 (optionnel).
- **Modes légers** (SPV, light clients).
- **Interop** (indexers externes, bridges contrôlés).

---

## Priorisation suggérée (synthèse)

1. **Consensus + sécurité + tests** (verrouiller le cœur).
2. **Réseau + persistance** (multi-nœuds robustes).
3. **Wallet + RPC + observabilité** (opérations et intégrations).
4. **Performance + release** (industrialisation).
5. **Docs + communauté + mainnet** (adoption).

---

## Livrables attendus par étape

- **S1**: Spec v1 + invariants consensus + tests de base.
- **S2**: PoW retarget + reorgs + block index persistant.
- **S3**: Sync réseau headers-first + P2P stable.
- **S4**: UTXO DB, snapshots, pruning.
- **S5**: Wallet v1 + RPC stable.
- **S6**: Observabilité + CI complète.
- **S7**: Testnet + release candidates.

---

## Risques & mitigations

- **Règles consensus ambiguës** → specs strictes + golden vectors.
- **DoS réseau** → rate limiting + scoring peers.
- **Bugs monétaires** → tests d'émission longue + invariants.
- **Réorgs profondes** → simulations lourdes + replay tests.

---

Ce document est amené à évoluer avec l'avancement du projet.
