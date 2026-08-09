#!/usr/bin/env python3
"""Resolve one Douyu room through StreamGet's app-search path.

The web signature resolver is intentionally not used here. The process emits
one JSON object and keeps all returned URLs in memory for the parent process.
"""

import asyncio
import json
import re
import sys


def emit(payload: dict) -> None:
    sys.stdout.write(json.dumps(payload, ensure_ascii=False) + "\n")
    sys.stdout.flush()


def resolve(room_id: str) -> None:
    if not re.fullmatch(r"\d{1,20}", room_id):
        emit({"roomId": room_id, "isLive": False})
        return

    try:
        from streamget import DouyuLiveStream
    except Exception:
        emit({"roomId": room_id, "error": "STREAMGET_UNAVAILABLE"})
        return

    try:
        live = DouyuLiveStream()
        data = asyncio.run(
            live.fetch_app_stream_data(f"https://www.douyu.com/{room_id}")
        )
        emit({
            "roomId": room_id,
            "isLive": bool(data.get("is_live")),
            "flvUrl": data.get("flv_url"),
        })
    except Exception:
        emit({"roomId": room_id, "error": "STREAMGET_UNAVAILABLE"})


if __name__ == "__main__":
    if len(sys.argv) != 2:
        emit({"roomId": "", "error": "INVALID_INPUT"})
        raise SystemExit(2)
    resolve(sys.argv[1])
