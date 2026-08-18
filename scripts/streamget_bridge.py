#!/usr/bin/env python3
"""Resolve one Douyu room through StreamGet's web H5 flow.

The process emits one JSON object and keeps signed URLs in memory only. When
the web quality endpoint cannot return an FLV URL, it falls back to the app
search endpoint so an otherwise playable room does not become unavailable.
"""

import asyncio
import json
import re
import sys


QUALITY_CODES = {
    "auto": None,
    "original": "OD",
    "super": "UHD",
    "high": "HD",
    "standard": "SD",
    "720p": "HD",
}


def emit(payload: dict) -> None:
    sys.stdout.write(json.dumps(payload, ensure_ascii=False) + "\n")
    sys.stdout.flush()


async def resolve_live(live, room_url: str, room_id: str, quality: str) -> dict:
    web_data = await live.fetch_web_stream_data(room_url)
    if not web_data.get("is_live"):
        return {"roomId": room_id, "isLive": False}

    try:
        stream = await live.fetch_stream_url(
            web_data,
            video_quality=QUALITY_CODES[quality],
        )
        if stream.is_live and stream.flv_url:
            return {
                "roomId": room_id,
                "isLive": True,
                "flvUrl": stream.flv_url,
                "resolvedQuality": quality,
                "source": "web-h5",
            }
    except Exception:
        pass

    app_data = await live.fetch_app_stream_data(room_url)
    if app_data.get("is_live") and app_data.get("flv_url"):
        return {
            "roomId": room_id,
            "isLive": True,
            "flvUrl": app_data["flv_url"],
            "resolvedQuality": "original",
            "source": "app-fallback",
        }
    return {"roomId": room_id, "isLive": False}


def resolve(room_id: str, quality: str) -> None:
    if not re.fullmatch(r"\d{1,20}", room_id) or quality not in QUALITY_CODES:
        emit({"roomId": room_id, "isLive": False})
        return

    try:
        from streamget import DouyuLiveStream
    except Exception:
        emit({"roomId": room_id, "error": "STREAMGET_UNAVAILABLE"})
        return

    try:
        live = DouyuLiveStream()
        result = asyncio.run(
            resolve_live(live, f"https://www.douyu.com/{room_id}", room_id, quality)
        )
        emit(result)
    except Exception:
        emit({"roomId": room_id, "error": "STREAMGET_UNAVAILABLE"})


if __name__ == "__main__":
    if len(sys.argv) != 3:
        emit({"roomId": "", "error": "INVALID_INPUT"})
        raise SystemExit(2)
    resolve(sys.argv[1], sys.argv[2])
