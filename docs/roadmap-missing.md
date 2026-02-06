# Liste des éléments manquants à ajouter au code source Novacoin

Ce document synthétise les fonctionnalités, outils et garde-fous **manquants** dans le code source Novacoin,
à ajouter progressivement. Il s’aligne sur la roadmap globale et sert de check-list d’implémentation.

> Objectif : transformer chaque point en tâches techniques concrètes (issues) et en tests associés.

## Mode d'emploi (exécution incrémentale)

1. **Créer une issue par item** : définir le périmètre, les risques et les critères d'acceptation.
2. **Ajouter les tests en premier** : unitaires + fonctionnels pour verrouiller le consensus.
3. **Tracer le statut** : marquer ✅ en fin de PR et référencer la PR associée.

## Priorités immédiates (0–90 jours)

> Focus : consensus, sécurité et tests (verrouillage du cœur).

- [ ] Spécification protocolaire v1 publiée + règles de sérialisation canonique.
- [ ] Invariants de supply testés (hard cap + rewards + fees).
- [ ] Vecteurs de tests consensus (blocs/tx invalides ciblés).
- [ ] Pipeline de validation stricte (timestamps, sigops, poids blocs).
- [ ] Simulations réorgs profondes + remise en mempool.

## 1) Gouvernance technique & spécifications

- Spécification protocolaire v1 (formats bloc/tx, sérialisation, endianness).
- Matrice de compatibilité (versioning, règles de hardfork planifiées).
- Règles de consensus verrouillées + justification cryptographique.
- Modèle de menaces & chiffrage de sécurité.
- Politique de changement (RFC internes, deprecation policy).
- Registre de décisions et traçabilité.

## 2) Consensus & validation

- Retarget PoW défini et borné + tests de résilience.
- Fork-choice basé sur cumulative work avec index multi-branches.
- Validation stricte (timestamps, poids blocs, sigops, limites mempool).
- Gestion complète des réorgs profondes (rollback + mempool).
- Paramètres de bloc (taille/poids max, MTP, fenêtre difficulté).
- Checkpoints (stratégie, rotation, neutralité).
- Règles coinbase (maturité, fees, anti-inflation).

## 3) Économie & émission monétaire

- Courbe d’émission finale (halvings, rythme, durée).
- Invariants supply (reward + fees + hard cap).
- Tests d’émission long terme (simulation complète).
- Outils d’audit (calcul d’offre, explorer interne).
- Politique de fees (estimation, dust limits, relay policy).
- Garde-fous anti-inflation.

## 4) Réseau P2P

- Découverte de pairs (seeds, DNS seeds, bootstrap).
- Gossip blocs/tx (inventaire, anti-spam, DoS).
- Headers-first sync complet.
- Protection réseau (banning, rate limiting, scoring peers).
- Compatibilité NAT (UPnP optionnel, docs).
- Résilience réseau (partition handling, diversité peers).
- Mécanismes anti-eclipse.

## 5) Stockage & persistance

- Block index persistant (multi-branches, orphans).
- Base UTXO optimisée (cache, pruning).
- Snapshots / fast sync.
- Stratégie de pruning (plein vs pruned).
- Réparation & recovery (salvage, vérification intégrité).
- Compaction/GC.

## 6) Wallet & UX

- Wallet HD (BIP32-like), formats d’adresses standard.
- Gestion des clés (chiffrement, rotation, backups).
- Mempool wallet-aware (sélection UTXO, estimation fees).
- CLI avancée (labeling, watch-only, rescan).
- Protection utilisateur (alertes fees, change).
- Compatibilité hardware wallet (si scope validé).

## 7) API & RPC

- RPC stable (noms, formats, erreurs, versioning).
- Auth & RBAC (tokens, whitelist).
- Monitoring (stats réseau, chainstate, mempool).
- SDKs clients (JS/Python).
- OpenAPI/JSON schema + génération clients.
- Rate limits.

## 8) Observabilité & logging

- Logs structurés (JSON, niveaux, correlation ID).
- Metrics (Prometheus/OpenMetrics).
- Dashboards (P2P, consensus, mempool).
- Tracing (pipeline tx).
- Alerting.

## 9) Sécurité

- Audit interne consensus.
- Fuzzing parsers blocs/tx.
- Protection DoS (limites, timeouts, anti-malleability).
- Validation stricte signatures.
- Tests d’invariant supply.
- Hardening (ASLR, sanitizers, flags).
- Key management (rotation, secrets, zeroization).

## 10) Tests & QA

- Tests unitaires par module (consensus, mempool, validation).
- Tests fonctionnels (réorg, forks, resync, halving).
- Fuzzing (mempool, parsing, net messages).
- Golden tests (vecteurs bloc/tx).
- CI (matrices OS/compilos, coverage, sanitize).
- Tests d’endurance.
- Tests d’invariants (supply cap, fees, coinbase).

## 11) Performance & scalabilité

- Profiling CPU/mémoire.
- Optimisation mempool (indexation, eviction policies).
- Compression (storage, wire protocol).
- Caches ciblés (UTXO, headers).
- Parallélisation validation scripts/tx.
- Bootstrap (fast sync, snapshot validation).

## 12) Release engineering & distribution

- Builds reproductibles.
- Signatures binaires (GPG).
- Packaging (deb/rpm, archives).
- Release notes standardisées.
- CI/CD artifacts multi-arch + SBOM.
- Politique versioning (semver, compat réseau).

## 13) Documentation & communauté

- Docs opérateur (setup node, firewall, monitoring).
- Docs développeur (architecture, flows, consensus rules).
- Guides contributeur (lint, style, tests).
- Changelog & roadmap publique.
- Tutoriels (testnet, usage CLI, debug).

## 14) Testnet / Mainnet

- Testnet stable params indépendants.
- Bootstrap testnet (seeds, checkpoints).
- Plan lancement mainnet (genesis, audits, bug bounty).
- Gouvernance testnet (reset policy).
- Monitoring mainnet (SLOs, incident response).

## 15) Backlog long terme

- Améliorations cryptographiques (Schnorr, MuSig, taproot-like).
- Sidechains/L2.
- Clients légers (SPV).
- Interop (indexers, bridges contrôlés).
- Réseaux privés (mode permissioned).
- Extensions VM/scripts.

## 16) Infrastructure & opérations

- Observabilité infra (logs, health checks, autoscaling).
- Résilience (backup nodes, rotation seeds).
- Procédures d’incident (runbooks).
- Capacity planning (coûts stockage/bande passante).

## 17) Gouvernance produit & adoption

- Feuille de route publique.
- Processus de contribution (review, CLA).
- Compatibilité business (partenaires, intégrations).
- Bug bounty.

## 18) Risques & mitigations avancés

- Règles consensus ambiguës → specs strictes + golden vectors.
- DoS réseau → rate limiting + scoring peers.
- Bugs monétaires → tests d’émission longue + invariants.
- Réorgs profondes → simulations + replay tests.
- Perte de clés → backups, rotations.
- Incompatibilités → matrice compat + déprecation claire.

## 19) Priorisation suggérée

1. Consensus + sécurité + tests.
2. Réseau + persistance.
3. Wallet + RPC + observabilité.
4. Performance + release.
5. Docs + communauté + mainnet.
6. Interop + innovations crypto.

## 20) Livrables attendus par étape

- S1 : Spec v1 + invariants consensus + tests de base.
- S2 : PoW retarget + réorgs + block index persistant.
- S3 : Sync headers-first + P2P stable.
- S4 : UTXO DB, snapshots, pruning.
- S5 : Wallet v1 + RPC stable.
- S6 : Observabilité + CI complète.
- S7 : Testnet + release candidates.
- S8 : Outillage écosystème + SDKs officiels.
- S9 : Mainnet v1 + post-mortem + roadmap v2.
