import asyncio
import json
import sys
from collections.abc import AsyncIterable, Awaitable, Callable
from typing import Any

from native.service.douyu_backend import BackendError, DouyuBackend
from native.service.protocol import (
    ErrorCode,
    ProtocolError,
    error_response,
    parse_request,
    success_resolve,
)


def _configure_stdio() -> None:
    # QProcess consumes the service protocol as UTF-8 regardless of the Windows code page.
    for stream in (sys.stdin, sys.stdout):
        try:
            stream.reconfigure(encoding="utf-8", errors="strict")
        except (AttributeError, ValueError):
            pass


def _request_id_from_line(line: str) -> int:
    try:
        value = json.loads(line)
    except (TypeError, json.JSONDecodeError):
        return 0
    request_id = value.get("requestId") if isinstance(value, dict) else 0
    return request_id if isinstance(request_id, int) and not isinstance(request_id, bool) and request_id > 0 else 0


async def run_service(
    lines: AsyncIterable[str],
    emit: Callable[[dict[str, Any]], Awaitable[None]],
    backend: Any,
) -> None:
    tasks: dict[int, asyncio.Task[None]] = {}
    semaphore = asyncio.Semaphore(2)
    shutting_down = False

    async def run_operation(request: dict[str, Any]) -> None:
        request_id = request["requestId"]
        try:
            async with semaphore:
                if request["op"] == "resolve":
                    is_live, variants = await backend.resolve(request["roomId"], request["quality"])
                    await emit(success_resolve(request_id, request["roomId"], is_live, variants))
                elif request["op"] == "search":
                    results = await asyncio.to_thread(backend.search, request["query"])
                    await emit({"requestId": request_id, "ok": True, "results": results})
        except asyncio.CancelledError:
            raise
        except BackendError as error:
            await emit(error_response(request_id, error.code))
        except Exception:
            await emit(error_response(request_id, ErrorCode.SERVICE_FAILED))
        finally:
            tasks.pop(request_id, None)

    async for line in lines:
        if shutting_down:
            break
        try:
            request = parse_request(line)
        except ProtocolError as error:
            await emit(error_response(_request_id_from_line(line), error.code))
            continue

        operation = request["op"]
        request_id = request["requestId"]
        if operation == "ping":
            await emit({"requestId": request_id, "ok": True, "pong": True})
        elif operation == "shutdown":
            shutting_down = True
            for task in list(tasks.values()):
                task.cancel()
            await emit({"requestId": request_id, "ok": True, "shutdown": True})
            break
        elif operation == "cancel":
            target = tasks.get(request["targetRequestId"])
            if target is not None:
                target.cancel()
            await emit({
                "requestId": request_id,
                "ok": True,
                "cancelled": request["targetRequestId"],
            })
        else:
            tasks[request_id] = asyncio.create_task(run_operation(request))

    if tasks:
        await asyncio.gather(*list(tasks.values()), return_exceptions=True)


async def _stdin_lines() -> AsyncIterable[str]:
    while True:
        line = await asyncio.to_thread(sys.stdin.readline)
        if not line:
            return
        yield line


async def _stdout_emit(value: dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(value, ensure_ascii=False, separators=(",", ":")) + "\n")
    sys.stdout.flush()


async def main() -> int:
    _configure_stdio()
    await run_service(_stdin_lines(), _stdout_emit, DouyuBackend())
    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
