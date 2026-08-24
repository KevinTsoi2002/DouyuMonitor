import asyncio
import inspect
import json
from collections.abc import Callable
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode, urlparse
from urllib.request import Request, urlopen

from native.service.protocol import ErrorCode, QUALITY_VALUES, ROOM_ID_RE


ROOM_API_BASE_URL = "https://open.douyucdn.cn/api/RoomApi/room/"
SEARCH_API_URL = "https://www.douyu.com/japi/search/api/searchShow"
ALLOWED_HOST_SUFFIXES = (".douyucdn.cn", ".douyucdn2.cn", ".edgesrv.com")


class BackendError(RuntimeError):
    def __init__(self, code: ErrorCode):
        super().__init__(code.value)
        self.code = code


def _scalar_string(value: Any) -> str | None:
    if isinstance(value, str) and value:
        return value
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        return str(value)
    return None


def _safe_http_url(value: Any) -> str | None:
    if not isinstance(value, str) or not value:
        return None
    parsed = urlparse(value)
    if parsed.scheme not in {"http", "https"} or not parsed.hostname:
        return None
    return value


def _safe_stream_url(value: Any) -> str:
    if not isinstance(value, str) or not value:
        raise BackendError(ErrorCode.INVALID_RESPONSE)
    parsed = urlparse(value)
    hostname = (parsed.hostname or "").lower()
    if parsed.scheme not in {"http", "https"}:
        raise BackendError(ErrorCode.UNSAFE_STREAM_URL)
    if parsed.username is not None or parsed.password is not None:
        raise BackendError(ErrorCode.UNSAFE_STREAM_URL)
    if not any(hostname.endswith(suffix) for suffix in ALLOWED_HOST_SUFFIXES):
        raise BackendError(ErrorCode.UNSAFE_STREAM_URL)
    return value


def _viewer_label(value: Any) -> str:
    try:
        viewers = float(value)
    except (TypeError, ValueError):
        return "0"
    if viewers <= 0:
        return "0"
    if viewers >= 10_000:
        value = f"{viewers / 10_000:.1f}".rstrip("0").rstrip(".")
        return f"{value} 万"
    return f"{round(viewers):,}"


def _default_fetch_json(url: str, timeout: float) -> Any:
    request = Request(url, headers={"Accept": "application/json"})
    try:
        with urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except (HTTPError, URLError, TimeoutError, OSError, json.JSONDecodeError) as error:
        raise BackendError(ErrorCode.SERVICE_FAILED) from error


class DouyuBackend:
    def __init__(
        self,
        fetch_json: Callable[[str, float], Any] | None = None,
        stream_factory: Callable[[], Any] | None = None,
        timeout: float = 10.0,
    ):
        self._fetch_json = fetch_json or _default_fetch_json
        self._stream_factory = stream_factory
        self._timeout = timeout

    def search(self, query: str) -> list[dict[str, Any]]:
        value = query.strip()
        if ROOM_ID_RE.fullmatch(value):
            return [self._fetch_room(value)]

        url = f"{SEARCH_API_URL}?{urlencode({'kw': value, 'page': 1, 'pageSize': 20})}"
        payload = self._fetch_json(url, self._timeout)
        if not isinstance(payload, dict) or payload.get("error") != 0:
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        data = payload.get("data")
        if not isinstance(data, dict) or not isinstance(data.get("relateShow"), list):
            raise BackendError(ErrorCode.INVALID_RESPONSE)

        candidates: dict[str, dict[str, Any]] = {}
        for item in data["relateShow"]:
            if not isinstance(item, dict):
                continue
            room_id = _scalar_string(item.get("rid"))
            anchor_name = _scalar_string(item.get("nickName"))
            title = _scalar_string(item.get("roomName"))
            if not room_id or not anchor_name or not title:
                continue
            candidate = {
                "roomId": room_id,
                "anchorName": anchor_name,
                "title": title,
                "category": _scalar_string(item.get("cateName")) or "未分类",
                "online": _scalar_string(item.get("isLive")) == "1",
                "viewerLabel": _viewer_label(item.get("hot")),
            }
            avatar_url = _safe_http_url(item.get("avatar"))
            if avatar_url:
                candidate["avatarUrl"] = avatar_url
            candidates.setdefault(room_id, candidate)
        return list(candidates.values())

    def _fetch_room(self, room_id: str) -> dict[str, Any]:
        payload = self._fetch_json(f"{ROOM_API_BASE_URL}{quote(room_id)}", self._timeout)
        if not isinstance(payload, dict) or payload.get("error") != 0:
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        data = payload.get("data")
        if not isinstance(data, dict):
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        mapped_room_id = _scalar_string(data.get("room_id"))
        title = _scalar_string(data.get("room_name"))
        anchor_name = _scalar_string(data.get("owner_name"))
        if not mapped_room_id or not title or not anchor_name:
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        result = {
            "roomId": mapped_room_id,
            "anchorName": anchor_name,
            "title": title,
            "category": _scalar_string(data.get("cate_name")) or "未分类",
            "online": _scalar_string(data.get("room_status")) == "1",
            "viewerLabel": _viewer_label(data.get("online")),
        }
        avatar_url = _safe_http_url(data.get("avatar"))
        if avatar_url:
            result["avatarUrl"] = avatar_url
        return result

    async def resolve(self, room_id: str, quality: str) -> tuple[bool, list[dict[str, Any]]]:
        if not ROOM_ID_RE.fullmatch(room_id) or quality not in QUALITY_VALUES:
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        try:
            if self._stream_factory is None:
                from streamget import DouyuLiveStream

                stream = DouyuLiveStream()
            else:
                stream = self._stream_factory()
            result = stream.fetch_app_stream_data(f"https://www.douyu.com/{room_id}")
            data = await result if inspect.isawaitable(result) else result
        except BackendError:
            raise
        except Exception as error:
            raise BackendError(ErrorCode.STREAMGET_UNAVAILABLE) from error

        if not isinstance(data, dict) or not isinstance(data.get("is_live"), bool):
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        if not data["is_live"]:
            return False, []

        playback_url = _safe_stream_url(data.get("flv_url"))
        return True, [{
            "id": "flv-auto",
            "label": "StreamGet FLV",
            "quality": "auto",
            "container": "flv",
            "playbackUrl": playback_url,
        }]
