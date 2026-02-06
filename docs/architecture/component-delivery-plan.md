# Plan de développement par composant (P2P, persistance, wallet, API/CLI, observabilité, audit, testnet/mainnet)

Ce plan décline la roadmap globale en **livrables concrets**, ordonnés par dépendances, pour guider les
implémentations P2P, persistance, wallet, API/CLI, observabilité, audit sécurité, et préparation testnet/mainnet.

> Objectif : transformer chaque étape en tickets concrets avec tests associés et critères d'acceptation.

## 1) Réseau P2P (fondations)

### 1.1 Découverte & bootstrap
- [ ] Définir les paramètres réseau (ports, magic bytes, versions minimales).
- [ ] Ajouter/valider DNS seeds + hardcoded seeds.
- [ ] Documenter la procédure de bootstrap (testnet/mainnet).
- [ ] Formaliser la compatibilité ascendante des messages (versioning P2P).
- [ ] Prévoir options de connectivité (Tor/I2P, policy d’addr relay).

### 1.2 Gossip & sync headers-first
- [ ] Pipeline headers-first complet (headers → blocks).
- [ ] Inventaire robuste (inv/getdata) + anti-spam.
- [ ] Stratégies d'annonce (blocks/tx) + backoff.
- [ ] Gestion de la latence (priorité headers, fetch en parallèle).
- [ ] Cohérence reorgs et rollback mempool lors de sync.

### 1.3 Résilience & sécurité réseau
- [ ] Scoring peers + rate limiting + bans temporaires.
- [ ] Anti-eclipse (rotation peers, anchors, quotas de diversité).
- [ ] Protection DoS (limites de messages, timeouts).
- [ ] Politique de diversité (ASN/geo) configurable.
- [ ] Playbook anti-partition (détection et mitigation).

## 2) Persistance & stockage

### 2.1 Index de blocs et chainstate
- [ ] Index persistant multi-branches + gestion d'orphans.
- [ ] UTXO cache + stratégie de pruning.
- [ ] Outils de recovery (salvage, checksums, fsck-like).
- [ ] Journalisation des corruptions + procédures de recovery.
- [ ] Benchmarks compaction/GC et limites d’espace disque.

### 2.2 Fast sync & snapshots
- [ ] Format snapshot + validation incrémentale.
- [ ] Téléchargement sécurisé + vérification d'intégrité.
- [ ] Plan de migration/versioning DB.
- [ ] Stratégie de bootstrap rapide (snapshots signés).
- [ ] Mode pruned vs full documenté et testé.

## 3) Wallet & UX

### 3.1 Wallet HD & gestion des clés
- [ ] Structure HD (BIP32-like) + formats d'adresses.
- [ ] Chiffrement local + rotation/backup.
- [ ] Watch-only + rescan.
- [ ] Standards d’adresses (bech32/legacy) + checksum.
- [ ] Prévention erreurs (confirmation d’adresse, anti-typo).

### 3.2 Mempool aware & protection utilisateur
- [ ] Sélection UTXO + estimation fees.
- [ ] Limites dust + alertes d'erreur (change/fees).
- [ ] Workflow CLI pour labels/metadata.
- [ ] Gestion multi-comptes + labels persistants.
- [ ] API interne pour hardware wallet (si scope validé).

## 4) API / CLI

### 4.1 RPC stable & versioning
- [ ] Convention d'erreurs & versioning RPC.
- [ ] Auth/RBAC (tokens + allowlist).
- [ ] OpenAPI/JSON schema + génération clients.
- [ ] Compatibilité tooling (pagination standard, filtres).
- [ ] Webhooks/WebSocket optionnels (events temps réel).

### 4.2 Observabilité via API
- [ ] Endpoints stats (network, chainstate, mempool).
- [ ] Pagination + filtres uniformes.
- [ ] Rate limiting côté serveur.
- [ ] Statistiques P2P (peers actifs, bans, latence).
- [ ] Endpoints d’audit supply (hard cap, fees, rewards).

## 5) Observabilité & audit

### 5.1 Logs, metrics, tracing
- [ ] Logs structurés (JSON) + rotation/rétention.
- [ ] Exposition metrics (Prometheus/OpenMetrics).
- [ ] Tracing pipeline tx/blocks + correlation IDs.
- [ ] Sanitization logs (données sensibles, clés).
- [ ] Dashboards (P2P, consensus, mempool).

### 5.2 Audit & sécurité
- [ ] Fuzzing parsers blocs/tx + tests déterministes.
- [ ] Tests invariants supply (hard cap, rewards, fees).
- [ ] Hardening build (sanitizers, flags, SBOM).
- [ ] Audits internes périodiques + post-mortems.
- [ ] Playbooks incidents sécurité + délais de réponse.

## 6) Testnet / Mainnet

### 6.1 Testnet
- [ ] Paramètres réseau testnet indépendants.
- [ ] Seeds + checkpoints testnet.
- [ ] Policy reset + versioning public.
- [ ] Checklist de lancement testnet public (monitoring, support).
- [ ] Canaux de release (devnet, testnet, RC).

### 6.2 Mainnet
- [ ] Plan genesis + distribution initiale + audits.
- [ ] Monitoring SLO + incident response plan.
- [ ] Programme bug bounty + release candidates.
- [ ] Critères de sortie testnet → mainnet.
- [ ] Procédures de rollback/patch post-launch.

## 7) Dépendances & priorisation

1. Consensus + sécurité + tests (verrouillage du cœur).
2. Réseau + persistance (multi-nœuds robustes).
3. Wallet + RPC + observabilité (opérations et intégrations).
4. Release + testnet → mainnet (industrialisation).

## 8) Notes d’implémentation (références)

- Spécifications et architecture : `docs/architecture/`.
- Liste exhaustive des manques : `docs/roadmap-missing.md`.
- Répertoires clés : `src/`, `tests/`, `doc/`, `docs/`.
