"""Wallet minimal pour ELIT21."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict

from elit21_chainstate import Elit21ChainState
from elit21_tx import Elit21Transaction
from elit21_util import log


@dataclass(slots=True)
class Elit21Wallet:
    address: str
    nonce: int = 0
    labels: Dict[str, str] = field(default_factory=dict)

    def send(self, chainstate: Elit21ChainState, recipient: str, amount: int, fee: int = 0) -> Elit21Transaction:
        log(
            "Préparation envoi wallet",
            sender=self.address,
            recipient=recipient,
            amount=amount,
            fee=fee,
            nonce=self.nonce,
        )
        tx = Elit21Transaction(
            sender=self.address,
            recipient=recipient,
            amount=amount,
            fee=fee,
            nonce=self.nonce,
        )
        chainstate.add_transaction(tx)
        self.nonce += 1
        log("Transaction wallet envoyée", txid=tx.txid, new_nonce=self.nonce)
        return tx
