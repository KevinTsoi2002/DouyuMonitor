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


def _quality_rate_for(quality: str) -> str:
    if quality == "original":
        return "0"
    if quality == "super":
        return "8"
    if quality == "auto":
        return "-1"
    if quality == "high":
        return "3"
    if quality == "standard":
        return "2"
    return "-1"


def _stream_field(data: Any, name: str) -> Any:
    if isinstance(data, dict):
        return data.get(name)
    return getattr(data, name, None)


def _quality_label(options: Any, rate: int) -> str:
    if isinstance(options, list):
        for option in options:
            if isinstance(option, dict) and option.get("rate") == rate:
                label = _scalar_string(option.get("label") or option.get("name"))
                if label:
                    return label
    return f"清晰度 {rate}"


def _quality_options(payload: Any) -> list[dict[str, Any]]:
    if not isinstance(payload, dict) or payload.get("error") != 0:
        return []
    data = payload.get("data")
    if not isinstance(data, dict) or not isinstance(data.get("multirates"), list):
        return []
    options: list[dict[str, Any]] = []
    seen: set[int] = set()
    for item in data["multirates"]:
        if not isinstance(item, dict):
            continue
        rate = item.get("rate")
        name = _scalar_string(item.get("name"))
        if (
            not isinstance(rate, int)
            or isinstance(rate, bool)
            or not 0 <= rate <= 255
            or not name
            or rate in seen
        ):
            continue
        seen.add(rate)
        options.append({"id": f"rate-{rate}", "label": name, "rate": rate})
    return options


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

    async def resolve(
        self,
        room_id: str,
        quality: str,
        quality_rate: int | None = None,
    ) -> tuple[bool, list[dict[str, Any]], list[dict[str, Any]]]:
        if not ROOM_ID_RE.fullmatch(room_id) or quality not in QUALITY_VALUES:
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        if quality_rate is not None and not 0 <= quality_rate <= 255:
            raise BackendError(ErrorCode.INVALID_RESPONSE)

        try:
            if self._stream_factory is None:
                from streamget import DouyuLiveStream

                stream = DouyuLiveStream()
            else:
                stream = self._stream_factory()

            room_result = stream.fetch_web_stream_data(f"https://www.douyu.com/{room_id}")
            room_data = await room_result if inspect.isawaitable(room_result) else room_result
        except BackendError:
            raise
        except Exception as error:
            raise BackendError(ErrorCode.STREAMGET_UNAVAILABLE) from error

        if not isinstance(room_data, dict) or not isinstance(room_data.get("is_live"), bool):
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        if not room_data["is_live"]:
            return False, [], []

        options: list[dict[str, Any]] = []
        try:
            option_rate = quality_rate if quality_rate is not None else _quality_rate_for(quality)
            stream_result = stream._fetch_web_stream_url(
                str(room_data.get("room_id", room_id)),
                rate=str(option_rate),
            )
            stream_data = await stream_result if inspect.isawaitable(stream_result) else stream_result
            options = _quality_options(stream_data)
            if not options:
                raise BackendError(ErrorCode.INVALID_RESPONSE)
            option_rates = {option["rate"] for option in options}
            reported_rate = _stream_field(stream_data.get("data"), "rate")
            if isinstance(reported_rate, int) and not isinstance(reported_rate, bool) \
                    and reported_rate in option_rates:
                rate = reported_rate
            elif quality_rate is not None and quality_rate in option_rates:
                rate = quality_rate
            else:
                rate = options[0]["rate"]
        except BackendError:
            raise
        except Exception as error:
            raise BackendError(ErrorCode.STREAMGET_UNAVAILABLE) from error

        if not isinstance(stream_data, dict) or stream_data.get("error") != 0:
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        stream_info = stream_data.get("data")
        if not isinstance(stream_info, dict):
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        rtmp_url = _stream_field(stream_info, "rtmp_url")
        rtmp_live = _stream_field(stream_info, "rtmp_live")
        if not isinstance(rtmp_url, str) or not isinstance(rtmp_live, str):
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        playback_url = _safe_stream_url(f"{rtmp_url.rstrip('/')}/{rtmp_live.lstrip('/')}")
        if quality_rate is None:
            label = "自动"
        else:
            label = _quality_label(options, quality_rate) if options else f"清晰度 {quality_rate}"
        return True, [{
            "id": f"flv-{rate}",
            "label": label,
            "quality": "auto",
            "qualityRate": rate,
            "container": "flv",
            "playbackUrl": playback_url,
        }], options
