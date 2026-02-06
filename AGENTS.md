# Roadmap d'amélioration (AGENT)

Ce document propose une **roadmap exhaustive** pour anticiper le maximum d'améliorations possibles de Novacoin Core. Il sert de guide de planification et de priorisation technique.

## Objectifs généraux

- Stabiliser un consensus PoW production-ready.
- Garantir l'intégrité du hard cap **29 000 000.00000000 NOVA**.
- Assurer la scalabilité, la sécurité, et l'observabilité du réseau.
- Industrialiser le build, les tests, et la distribution.
- Préparer l'écosystème (outillage, docs, SDKs, testnet, mainnet).

---

## 1) Gouvernance technique & spécifications (Priorité: Très haute)

- **Spécification protocolaire v1** (formats bloc/tx, règles UTXO ou comptes, sérialisation, endianness).
- **Matrice de compatibilité** (versioning, règles de hardfork planifiées).
- **Règles de consensus** verrouillées + document de justification cryptographique.
- **Chiffrage de sécurité** (modèle de menaces, hypothèses réseau, attaques connues).
- **Politique de changement** (processus de décision, deprecation policy, RFCs internes).
- **Traçabilité** (registre des décisions, sign-off, révisions de spéc).

---

## 2) Consensus & validation (Priorité: Très haute)

- **Retarget PoW**: algorithme précis, bornes d'ajustement, tests de résilience.
- **Fork-choice**: cumulative work avec index persistant multi-branches.
- **Validation stricte**: timestamps, poids des blocs, sigops, limites mempool.
- **Réorgs profondes**: remise en mempool, rollbacks cohérents.
- **Paramètres de bloc**: taille/poids max, fenêtre de difficulté, median time past.
- **Checkpoints**: stratégie, rotation, neutralité des forks.
- **Règles de coinbase**: maturité, fee-policy, anti-inflation.

---

## 3) Économie & émission monétaire (Priorité: Très haute)

- **Courbe d'émission** définitive (halvings, rythme, durée).
- **Validation supply**: invariants sur reward + fees + hard cap.
- **Tests d'émission** à long terme (simulation complète du cycle monétaire).
- **Outillage d'audit**: calcul d'offre, explorer interne.
- **Politique de fees**: estimation, dust limits, relay policy.
- **Mesures anti-inflation**: garde-fous sur coinbase et validation stricte.

---

## 4) Réseau P2P (Priorité: Haute)

- **Découverte de pairs**: seeds, DNS seeds, bootstrap.
- **Gossip blocs/tx**: inventaire, anti-spam, protection DoS.
- **Headers-first sync**: pipeline complet (headers, checkpoints, blocks).
- **Protection réseau**: banning, rate limiting, scoring peers.
- **Compatibilité NAT**: UPnP optionnel, documentation.
- **Résilience réseau**: partition handling, peer diversity, asn/geo policies.
- **Mécanismes d'anti-eclipse**: rotations, ancrage de pairs fiables.

---

## 5) Stockage & persistance (Priorité: Haute)

- **Block index** persistant (multi-branches, orphans).
- **Base UTXO** optimisée (cache, pruning).
- **Snapshots** et bootstrap state (fast sync).
- **Stratégie de pruning**: modes plein vs pruned.
- **Réparation & recovery**: outils de salvage, vérification d'intégrité.
- **Compaction/GC**: plans de compaction et limites d'espace disque.

---

## 6) Wallet & UX (Priorité: Moyenne)

- **Wallet HD** (BIP32-like), formats d'adresses standardisés.
- **Gestion des clés**: chiffrage, rotation, backups.
- **Mempool wallet-aware**: sélection d'UTXO, estimation de frais.
- **Outils CLI**: commandes avancées (labeling, watch-only, rescan).
- **Protection utilisateur**: alertes de fees, prévention d'erreurs de change.
- **Compatibilité hardware wallet** (si scope acceptable).

---

## 7) API & RPC (Priorité: Moyenne)

- **RPC stable** (noms, formats, erreurs, versioning).
- **Auth & RBAC**: accès sécurisé (tokens, whitelist).
- **Monitoring**: stats réseau, chainstate, mempool.
- **SDKs**: clients minimalistes pour intégration (JS/Python).
- **Compatibilité tooling**: openapi/JSON schema, génération clients.
- **Rate limits**: quotas, limites d'usage, audits.

---

## 8) Observabilité & logging (Priorité: Moyenne)

- **Logs structurés** (JSON, niveaux, correlation ID).
- **Metrics**: Prometheus/OpenMetrics.
- **Dashboards**: performance P2P, consensus, mempool.
- **Tracing**: suivi des transactions dans la pipeline.
- **Alerting**: seuils critiques, auto-remediation (si possible).

---

## 9) Sécurité (Priorité: Haute)

- **Audit interne** des règles consensus.
- **Fuzzing** des parsers blocs/tx.
- **Protection DoS**: limites, timeouts, anti-malleability.
- **Signatures**: validation stricte, formats sécurisés.
- **Supply invariant** testé et prouvé.
- **Hardening**: ASLR, sanitizers, compilation flags.
- **Key management**: rotation, secrets handling, zeroization.

---

## 10) Tests & QA (Priorité: Très haute)

- **Tests unitaires** par module (consensus, mempool, validation).
- **Tests fonctionnels** (réorg, forks, resync, halving).
- **Fuzzing** (mempool, tx parsing, net messages).
- **Golden tests**: vecteurs de blocs/tx.
- **CI**: matrices OS/compilateurs, coverage, sanitize.
- **Tests d'endurance**: sync long, large mempool, stress P2P.
- **Tests d'invariants**: supply cap, tx fees, coinbase.

---

## 11) Performance & scalabilité (Priorité: Moyenne)

- **Profiling** CPU/mémoire.
- **Optimisation mempool** (indexation, eviction policies).
- **Compression** (block storage, wire protocol).
- **Caches** ciblés (UTXO, block headers).
- **Parallélisation**: validation des scripts/tx (là où possible).
- **Stratégie de bootstrap**: fast sync, snapshot validation.

---

## 12) Release engineering & distribution (Priorité: Moyenne)

- **Build reproductible** (deterministic builds).
- **Signatures des binaires** (GPG).
- **Packaging** (deb/rpm, archives).
- **Release notes** standardisées.
- **CI/CD**: artifacts, pipelines multi-arch, SBOM.
- **Politique de versioning**: semver, compatibilité réseau.

---

## 13) Documentation & communauté (Priorité: Moyenne)

- **Docs opérateur** (node setup, firewall, monitoring).
- **Docs développeur** (architecture, flows, consensus rules).
- **Guides contributeur** (lint, style, tests).
- **Changelog** et roadmap publique.
- **Tutoriels**: déploiement testnet, usage CLI, debug.

---

## 14) Testnet / Mainnet (Priorité: Haute)

- **Testnet stable** avec paramètres indépendants.
- **Bootstrap testnet** (seeds, checkpoints).
- **Plan de lancement** mainnet (genesis, audits, bug bounty).
- **Governance de testnet**: reset policy, versions publiques.
- **Monitoring mainnet**: SLOs, incident response plan.

---

## 15) Backlog long terme

- **Améliorations cryptographiques** (Schnorr, MuSig, taproot-like si pertinent).
- **Sidechains** ou L2 (optionnel).
- **Modes légers** (SPV, light clients).
- **Interop** (indexers externes, bridges contrôlés).
- **Réseaux privés**: modes permissioned pour tests industriels.
- **Compatibilité VM**: scripts avancés, extension du langage.

---

## 16) Infrastructure & opérations (Priorité: Moyenne)

- **Observabilité d'infra**: logs système, health checks, autoscaling.
- **Résilience**: back-up nodes, rotation des seeds.
- **Procédures d'incident**: runbooks, communication.
- **Coûts**: plans de capacity, stockage et bande passante.

---

## 17) Gouvernance produit & adoption (Priorité: Moyenne)

- **Feuille de route publique**: jalons, transparence.
- **Gestion des contributions**: processus de review, CLA si nécessaire.
- **Compatibilité business**: exigences partenaires, intégrations.
- **Programme de bug bounty**: scope, récompenses.

---

## 18) Risques & mitigations avancés

- **Règles consensus ambiguës** → specs strictes + golden vectors.
- **DoS réseau** → rate limiting + scoring peers.
- **Bugs monétaires** → tests d'émission longue + invariants.
- **Réorgs profondes** → simulations lourdes + replay tests.
- **Perte de clés** → backups, rotations, best practices wallet.
- **Incompatibilités** → matrice de compat + déprecation claire.

---

## 19) Priorisation suggérée (synthèse)

1. **Consensus + sécurité + tests** (verrouiller le cœur).
2. **Réseau + persistance** (multi-nœuds robustes).
3. **Wallet + RPC + observabilité** (opérations et intégrations).
4. **Performance + release** (industrialisation).
5. **Docs + communauté + mainnet** (adoption).
6. **Interop + innovations crypto** (long terme).

---

## 20) Livrables attendus par étape

- **S1**: Spec v1 + invariants consensus + tests de base.
- **S2**: PoW retarget + reorgs + block index persistant.
- **S3**: Sync réseau headers-first + P2P stable.
- **S4**: UTXO DB, snapshots, pruning.
- **S5**: Wallet v1 + RPC stable.
- **S6**: Observabilité + CI complète.
- **S7**: Testnet + release candidates.
- **S8**: Outillage écosystème + SDKs officiels.
- **S9**: Mainnet v1 + post-mortem + roadmap v2.

---

## 21) Compléments à intégrer (pour compléter la liste)

### Gouvernance & spécifications
- **Paramètres de réseau** formalisés (magic bytes, ports, versions minimales).
- **Signalement d’upgrade** (BIP9-like ou équivalent) + calendrier de déploiement.
- **Politique de compatibilité** des APIs/CLI (breaking changes, versioning).

### Consensus & validation
- **Protection time-warp** (règles anti-manipulation des timestamps).
- **Vecteurs de test consensus** (blocs/tx invalides ciblés).
- **Règles de script** documentées (opcodes autorisés, limites, erreurs).

### Économie & émission
- **Politique de genesis** (distribution initiale, premine, lockups).
- **Paramètres de halving** verrouillés + simulation d’offre complète.
- **Audits d’offre** reproductibles (script/outil publié).

### Réseau & P2P
- **Versioning des messages** + compatibilité ascendante.
- **Protection privacy** (options Tor/I2P, addr relay policy).
- **Gestion d’éclipses avancée** (anchors, peer rotation, diversity quotas).

### Stockage & persistance
- **Migrations de DB** documentées (versioning, rollback).
- **Vérification d’intégrité** (checksums, fsck-like).
- **Journalisation des corruptions** + procédures de recovery.

### Wallet & UX
- **Standards d’adresses** (bech32/legacy, checksums).
- **Gestion multi-comptes** + labels/metadata.
- **Prévention erreurs** (confirmation d’adresse, anti-typo).

### API & RPC
- **Pagination & filtres** uniformes (mempool, blocks, txs).
- **Rate limiting côté serveur** + quotas par client.
- **Webhooks/WebSocket** optionnels pour événements temps réel.

### Observabilité & logs
- **Rotation des logs** + rétention configurables.
- **Sanitization des logs** (données sensibles, clés).
- **Profiling continu** (pprof/trace) en environnement debug.

### Sécurité
- **Supply-chain security** (SBOM, scans dépendances).
- **Hardening runtime** (seccomp, sandboxing options).
- **Playbooks incidents** sécurité (procédures et délais).

### Tests & QA
- **Tests de non-régression** post-incident.
- **Tests déterministes** (seeded fuzzing, timings).
- **Simulations réseau** (latence, partition, churn).

### Release & distribution
- **Infrastructure de signature** (HSM/clé hors-ligne).
- **Vérification d’artefacts** (checksums, attestations).
- **Canal de updates** (release channels, RC/beta).

### Documentation & écosystème
- **Guide d’exploitation** (SLO, monitoring, backup).
- **Guide intégration exchanges** (RPC, sécurité, bonnes pratiques).
- **Spec formats** (bloc/tx) publiées et versionnées.

---

Ce document est amené à évoluer avec l'avancement du projet.
