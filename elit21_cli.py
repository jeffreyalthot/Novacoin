"""Interface CLI de base pour ELIT21."""

from __future__ import annotations

import argparse

from elit21_chainstate import Elit21ChainState
from elit21_wallet import Elit21Wallet
from elit21_util import log


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="ELIT21 CLI - base blockchain")
    sub = parser.add_subparsers(dest="command", required=True)

    send = sub.add_parser("send", help="Créer une transaction et la placer en mempool")
    send.add_argument("sender")
    send.add_argument("recipient")
    send.add_argument("amount", type=int)
    send.add_argument("--fee", type=int, default=0)

    mine = sub.add_parser("mine", help="Miner les transactions de la mempool")
    mine.add_argument("miner")

    sub.add_parser("status", help="Afficher l'état de la chaîne")
    return parser


def run_cli() -> None:
    parser = build_parser()
    args = parser.parse_args()

    chainstate = Elit21ChainState()
    log("CLI lancée", command=args.command)

    if args.command == "send":
        wallet = Elit21Wallet(address=args.sender)
        tx = wallet.send(chainstate=chainstate, recipient=args.recipient, amount=args.amount, fee=args.fee)
        print(f"Transaction créée: {tx.txid}")
    elif args.command == "mine":
        try:
            block = chainstate.mine_block(miner=args.miner)
            print(f"Bloc miné: #{block.index} {block.block_hash}")
        except ValueError as exc:
            print(f"Erreur: {exc}")
    elif args.command == "status":
        latest = chainstate.latest_block()
        print(f"Hauteur: {latest.index} | Hash: {latest.block_hash} | Mempool: {len(chainstate.mempool)}")


if __name__ == "__main__":
    run_cli()
