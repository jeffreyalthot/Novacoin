# Alignement de structure Novacoin ↔ Bitcoin Core

Ce document décrit l'extension de l'arborescence Novacoin pour se rapprocher de la structure de Bitcoin Core, tout en gardant les conventions Novacoin.

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

Permettre une croissance modulaire compatible avec les pratiques de Bitcoin Core (tests, outils, packaging, séparation des responsabilités), sans casser l'existant (`docs/`, `tests/`, `src/apps/`, etc.).
