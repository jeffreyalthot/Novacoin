"""Modèle transactionnel minimal pour ELIT21."""

from __future__ import annotations

from dataclasses import dataclass, field

from elit21_util import log, now_ts, sha256_hex, to_json


@dataclass(slots=True)
class Elit21Transaction:
    sender: str
    recipient: str
    amount: int
    fee: int = 0
    nonce: int = 0
    timestamp: int = field(default_factory=now_ts)
    txid: str = ""

    def __post_init__(self) -> None:
        log(
            "Initialisation transaction",
            sender=self.sender,
            recipient=self.recipient,
            amount=self.amount,
            fee=self.fee,
            nonce=self.nonce,
            timestamp=self.timestamp,
        )
        if self.amount <= 0:
            raise ValueError("Le montant de transaction doit être strictement positif.")
        if self.fee < 0:
            raise ValueError("Les frais ne peuvent pas être négatifs.")
        self.txid = self.compute_txid()

    def to_dict(self) -> dict:
        payload = {
            "sender": self.sender,
            "recipient": self.recipient,
            "amount": self.amount,
            "fee": self.fee,
            "nonce": self.nonce,
            "timestamp": self.timestamp,
        }
        log("Transaction convertie en dictionnaire", payload=payload)
        return payload

    def compute_txid(self) -> str:
        txid = sha256_hex(to_json(self.to_dict()))
        log("TXID calculé", txid=txid)
        return txid

    def is_valid(self) -> bool:
        recomputed = self.compute_txid()
        is_ok = recomputed == self.txid
        log("Validation transaction", txid=self.txid, recomputed=recomputed, valid=is_ok)
        return is_ok
