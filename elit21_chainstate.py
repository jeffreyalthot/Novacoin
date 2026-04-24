"""Chaîne de blocs minimale pour ELIT21."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List

from elit21_tx import Elit21Transaction
from elit21_util import log, now_ts, sha256_hex, to_json


@dataclass(slots=True)
class Elit21Block:
    index: int
    previous_hash: str
    transactions: list[Elit21Transaction]
    miner: str
    nonce: int = 0
    timestamp: int = field(default_factory=now_ts)
    block_hash: str = ""

    def __post_init__(self) -> None:
        log(
            "Initialisation bloc",
            index=self.index,
            previous_hash=self.previous_hash,
            tx_count=len(self.transactions),
            miner=self.miner,
        )
        self.block_hash = self.compute_hash()

    def to_dict(self) -> dict:
        payload = {
            "index": self.index,
            "previous_hash": self.previous_hash,
            "transactions": [tx.to_dict() for tx in self.transactions],
            "miner": self.miner,
            "nonce": self.nonce,
            "timestamp": self.timestamp,
        }
        log("Bloc converti en dictionnaire", index=self.index)
        return payload

    def compute_hash(self) -> str:
        hash_value = sha256_hex(to_json(self.to_dict()))
        log("Hash de bloc calculé", index=self.index, hash=hash_value)
        return hash_value


class Elit21ChainState:
    def __init__(self) -> None:
        log("Création chainstate")
        self.chain: List[Elit21Block] = [self._create_genesis_block()]
        self.mempool: List[Elit21Transaction] = []

    def _create_genesis_block(self) -> Elit21Block:
        log("Création du bloc genesis")
        return Elit21Block(index=0, previous_hash="0" * 64, transactions=[], miner="genesis")

    def latest_block(self) -> Elit21Block:
        block = self.chain[-1]
        log("Récupération dernier bloc", index=block.index, hash=block.block_hash)
        return block

    def add_transaction(self, tx: Elit21Transaction) -> None:
        if not tx.is_valid():
            raise ValueError("Transaction invalide, rejetée de la mempool.")
        self.mempool.append(tx)
        log("Transaction ajoutée à la mempool", txid=tx.txid, mempool_size=len(self.mempool))

    def mine_block(self, miner: str) -> Elit21Block:
        if not self.mempool:
            raise ValueError("Aucune transaction à miner.")
        previous = self.latest_block()
        block = Elit21Block(
            index=previous.index + 1,
            previous_hash=previous.block_hash,
            transactions=self.mempool.copy(),
            miner=miner,
        )
        self.chain.append(block)
        self.mempool.clear()
        log("Bloc miné et ajouté", index=block.index, hash=block.block_hash, miner=miner)
        return block

    def is_chain_valid(self) -> bool:
        log("Validation complète de la chaîne", height=len(self.chain) - 1)
        for idx in range(1, len(self.chain)):
            current = self.chain[idx]
            previous = self.chain[idx - 1]
            if current.previous_hash != previous.block_hash:
                log("Invalidité détectée: previous_hash incohérent", index=idx)
                return False
            if current.compute_hash() != current.block_hash:
                log("Invalidité détectée: hash de bloc altéré", index=idx)
                return False
        log("Chaîne valide")
        return True
