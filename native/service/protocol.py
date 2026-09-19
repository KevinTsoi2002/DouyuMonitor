import json
import re
from enum import Enum
from typing import Any


ROOM_ID_RE = re.compile(r"^[0-9]{1,20}$")
QUALITY_VALUES = frozenset({"auto", "original", "super", "high", "standard"})
MAX_QUALITY_RATE = 255
OPERATIONS = frozenset({"ping", "resolve", "search", "cancel", "shutdown"})


class ErrorCode(str, Enum):
    INVALID_INPUT = "INVALID_INPUT"
    ROOM_OFFLINE = "ROOM_OFFLINE"
    STREAMGET_UNAVAILABLE = "STREAMGET_UNAVAILABLE"
    UNSAFE_STREAM_URL = "UNSAFE_STREAM_URL"
    TIMEOUT = "TIMEOUT"
    INVALID_RESPONSE = "INVALID_RESPONSE"
    SERVICE_FAILED = "SERVICE_FAILED"


RETRYABLE_CODES = frozenset(
    {
        ErrorCode.STREAMGET_UNAVAILABLE,
        ErrorCode.TIMEOUT,
        ErrorCode.SERVICE_FAILED,
    }
)


class ProtocolError(ValueError):
    def __init__(self, code: ErrorCode, message: str = "invalid protocol input"):
        super().__init__(message)
        self.code = code


def _positive_integer(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool) and value > 0


def _valid_room_id(value: Any) -> bool:
    return isinstance(value, str) and ROOM_ID_RE.fullmatch(value) is not None


def _valid_quality(value: Any) -> bool:
    return isinstance(value, str) and value in QUALITY_VALUES


def _valid_quality_rate(value: Any) -> bool:
    return (
        isinstance(value, int)
        and not isinstance(value, bool)
        and 0 <= value <= MAX_QUALITY_RATE
    )


def parse_request(line: str) -> dict[str, Any]:
    try:
        value = json.loads(line)
    except (TypeError, json.JSONDecodeError) as error:
        raise ProtocolError(ErrorCode.INVALID_INPUT) from error

    if not isinstance(value, dict) or not _positive_integer(value.get("requestId")):
        raise ProtocolError(ErrorCode.INVALID_INPUT)

    operation = value.get("op")
    if operation not in OPERATIONS:
        raise ProtocolError(ErrorCode.INVALID_INPUT)

    request_id = value["requestId"]
    if operation == "ping" or operation == "shutdown":
        return {"requestId": request_id, "op": operation}

    if operation == "resolve":
        room_id = value.get("roomId")
        quality = value.get("quality")
        quality_rate = value.get("qualityRate")
        if (
            not _valid_room_id(room_id)
            or not _valid_quality(quality)
            or (quality_rate is not None and not _valid_quality_rate(quality_rate))
        ):
            raise ProtocolError(ErrorCode.INVALID_INPUT)
        result = {
            "requestId": request_id,
            "op": operation,
            "roomId": room_id,
            "quality": quality,
        }
        if quality_rate is not None:
            result["qualityRate"] = quality_rate
        return result

    if operation == "search":
        query = value.get("query")
        if not isinstance(query, str) or not query.strip() or len(query.strip()) > 200:
            raise ProtocolError(ErrorCode.INVALID_INPUT)
        return {"requestId": request_id, "op": operation, "query": query.strip()}

    target_request_id = value.get("targetRequestId")
    if not _positive_integer(target_request_id):
        raise ProtocolError(ErrorCode.INVALID_INPUT)
    return {
        "requestId": request_id,
        "op": operation,
        "targetRequestId": target_request_id,
    }


def success_resolve(
    request_id: int,
    room_id: str,
    is_live: bool,
    variants: list[dict[str, Any]],
    quality_options: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    response = {
        "requestId": request_id,
        "ok": True,
        "roomId": room_id,
        "isLive": bool(is_live),
        "variants": list(variants),
    }
    if quality_options:
        response["qualityOptions"] = list(quality_options)
    return response


def error_response(request_id: int, code: ErrorCode) -> dict[str, Any]:
    return {
        "requestId": request_id,
        "ok": False,
        "error": {
            "code": code.value,
            "retryable": code in RETRYABLE_CODES,
        },
    }
