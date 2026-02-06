# Alignement de structure Novacoin Core (fork de Bitcoin Core)

Ce document décrit l'extension de l'arborescence de Novacoin Core issue d'un fork de Bitcoin Core, tout en gardant les conventions du projet.

## Dossiers top-level ajoutés

- `build-aux/`
- `contrib/`
- `depends/`
- `doc/`
- `m4/`
- `share/`
- `test/`

## Dossiers `src/` ajoutés

- `bench/`, `crypto/`, `interfaces/`, `kernel/`, `node/`, `policy/`, `primitives/`, `script/`, `secp256k1/`, `support/`, `qt/`, `zmq/`
- `test/util/`
- `wallet/test/`

## Objectif

Permettre une croissance modulaire compatible avec les pratiques du projet amont Bitcoin Core (tests, outils, packaging, séparation des responsabilités), sans casser l'existant (`docs/`, `tests/`, `src/apps/`, etc.).
