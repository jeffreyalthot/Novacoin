"""Utilitaires de base pour ELIT21 blockchain."""

from __future__ import annotations

import hashlib
import json
import time
from datetime import datetime, timezone
from typing import Any


LOG_PREFIX = "[ELIT21][UTIL]"


def log(message: str, **context: Any) -> None:
    """Affiche un log console riche et homogène."""
    payload = {
        "ts": now_iso(),
        "message": message,
        "context": context,
    }
    print(f"{LOG_PREFIX} {json.dumps(payload, ensure_ascii=False, sort_keys=True)}")


def now_ts() -> int:
    ts = int(time.time())
    log("Horodatage généré", timestamp=ts)
    return ts


def now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256_hex(data: str) -> str:
    digest = hashlib.sha256(data.encode("utf-8")).hexdigest()
    log("Hash SHA-256 calculé", source_preview=data[:64], digest=digest)
    return digest


def to_json(data: Any) -> str:
    serialized = json.dumps(data, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    log("Sérialisation JSON effectuée", length=len(serialized))
    return serialized
