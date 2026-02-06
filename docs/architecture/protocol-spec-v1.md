# Spécification protocolaire v1 (Brouillon)

> **Statut** : brouillon de travail. Cette spécification doit être complétée et validée avant toute mise en production.
>
> Objectif : documenter de manière normative les formats bloc/tx, les règles de consensus et les invariants de supply,
> afin de verrouiller la compatibilité réseau et prévenir les erreurs monétaires.

## 1) Portée et conventions

- **Portée** : format des blocs/transactions, règles de consensus, validation et invariants de supply.
- **Notation normative** : les mots-clés **DOIT**, **NE DOIT PAS**, **DEVRAIT** suivent l'usage RFC 2119.
- **Endianness** :
  - Les champs multi-octets sont **little-endian** par défaut, sauf mention contraire.
  - Les hachages sont affichés en **hex big-endian** (convention d'affichage).

## 2) Paramètres réseau (à verrouiller)

- Magic bytes : _à définir_.
- Port P2P : _à définir_.
- Port RPC : _à définir_.
- Versions minimales supportées : _à définir_.

## 3) Formats de messages réseau (aperçu)

- Liste des messages P2P : _à compléter_.
- Champs communs (magic, commande, longueur, checksum) : _à compléter_.

## 4) Formats de données

### 4.1) Transaction (tx)

- Version : _à définir_.
- Entrées (vin) : _à définir_.
- Sorties (vout) : _à définir_.
- Locktime : _à définir_.

### 4.2) Bloc

- En-tête de bloc : _à définir_.
- Corps de bloc : _à définir_.
- Ordre des transactions : _à définir_.

## 5) Règles de consensus

### 5.1) Validation des blocs

- Poids/taille maximum : _à définir_.
- Horodatage (median time past, dérives autorisées) : _à définir_.
- Sigops et limites : _à définir_.

### 5.2) Validation des transactions

- Scripts autorisés et limites : _à définir_.
- Règles anti-malleability : _à définir_.
- Règles de mempool : _à définir_.

### 5.3) Fork-choice

- Sélection de la chaîne : _à définir_.
- Gestion des réorgs : _à définir_.

## 6) Économie & invariants de supply

- Courbe d'émission : _à définir_.
- Règle de coinbase (maturité, récompense, fees) : _à définir_.
- **Invariant** : le total émis **NE DOIT PAS** excéder **29 000 000.00000000 NOVA**.

## 7) Sécurité & menaces

- Modèle de menaces : _à définir_.
- Attaques connues et mitigations : _à définir_.

## 8) Matrice de compatibilité & upgrades

- Versioning protocolaire : _à définir_.
- Signalement d'upgrade (BIP9-like ou équivalent) : _à définir_.
- Politique de breaking changes : _à définir_.

## 9) Traçabilité

- Journal de décisions : _à définir_.
- Liens vers RFC internes : _à définir_.

## 10) TODO immédiat

1. Compléter les paramètres réseau à partir du code source.
2. Documenter les formats sérialisés bloc/tx.
3. Verrouiller les règles de consensus (timestamps, sigops, poids).
4. Définir la courbe d'émission + règles coinbase.
5. Ajouter des vecteurs de tests consensus en miroir de cette spec.
