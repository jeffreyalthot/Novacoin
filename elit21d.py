"""Daemon simplifié ELIT21."""

from __future__ import annotations

import time

from elit21_chainstate import Elit21ChainState
from elit21_util import log


def main() -> None:
    chainstate = Elit21ChainState()
    log("Démarrage daemon ELIT21", height=chainstate.latest_block().index)

    try:
        while True:
            log(
                "Heartbeat daemon",
                height=chainstate.latest_block().index,
                mempool_size=len(chainstate.mempool),
            )
            time.sleep(5)
    except KeyboardInterrupt:
        log("Arrêt daemon demandé par utilisateur")


if __name__ == "__main__":
    main()
