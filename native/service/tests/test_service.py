import asyncio
import json
import subprocess
import sys
import textwrap
import time
import threading
import unittest
from pathlib import Path

from native.service.protocol import ErrorCode
from native.service.streamget_service import run_service


class FakeBackend:
    def __init__(self):
        self.active = 0
        self.max_active = 0
        self.started = []
        self.release = asyncio.Event()
        self.fail = False

    def search(self, _query):
        return [{"roomId": "63136", "title": "测试"}]

    def room_status(self, _room_id):
        return False

    async def resolve(self, room_id, _quality, _quality_rate=None):
        self.started.append(room_id)
        self.active += 1
        self.max_active = max(self.max_active, self.active)
        try:
            await asyncio.sleep(0.02)
            if self.fail:
                raise RuntimeError("private diagnostic")
            return True, [{
                "id": "flv-auto",
                "label": "StreamGet FLV",
                "quality": "auto",
                "container": "flv",
                "playbackUrl": "https://live.douyucdn.cn/live/test.flv?secret=redacted",
            }], [{"id": "rate-0", "label": "原画", "rate": 0}]
        finally:
            self.active -= 1


async def collect(lines, backend):
    async def source():
        for line in lines:
            yield line
            await asyncio.sleep(0)

    output = []

    async def emit(value):
        output.append(value)

    shutdown_requested = await run_service(source(), emit, backend)
    return output, shutdown_requested


class ServiceTests(unittest.IsolatedAsyncioTestCase):
    async def test_cancelled_sync_workers_keep_concurrency_slots(self):
        class BlockingBackend:
            active = 0
            maximum = 0
            release = threading.Event()
            lock = threading.Lock()

            def search(self, _query):
                with self.lock:
                    self.active += 1
                    self.maximum = max(self.maximum, self.active)
                self.release.wait(2)
                with self.lock:
                    self.active -= 1
                return []

        backend = BlockingBackend()
        output = []

        async def source():
            for request_id in (1, 2):
                yield json.dumps({"requestId": request_id, "op": "search", "query": "test"})
            for _ in range(100):
                if backend.active == 2:
                    break
                await asyncio.sleep(0.01)
            for request_id in (1, 2):
                yield json.dumps({"requestId": request_id + 2, "op": "cancel",
                                  "targetRequestId": request_id})
            for request_id in (5, 6):
                yield json.dumps({"requestId": request_id, "op": "search", "query": "test"})
            await asyncio.sleep(0.1)
            backend.release.set()

        async def emit(value):
            output.append(value)

        await run_service(source(), emit, backend)
        self.assertEqual(backend.maximum, 2)
        self.assertFalse(any(value["requestId"] in (1, 2) for value in output))
        self.assertEqual({value["requestId"] for value in output}, {3, 4, 5, 6})

    async def test_ping_and_malformed_input_keep_the_loop_alive(self):
        output, shutdown_requested = await collect(
            [
                "not-json\n",
                '{"requestId":2,"op":"ping"}\n',
                '{"requestId":3,"op":"shutdown"}\n',
            ],
            FakeBackend(),
        )
        self.assertEqual(output[0]["error"]["code"], ErrorCode.INVALID_INPUT.value)
        self.assertEqual(output[1], {"requestId": 2, "ok": True, "pong": True})
        self.assertEqual(output[2], {"requestId": 3, "ok": True, "shutdown": True})
        self.assertTrue(shutdown_requested)

    async def test_resolve_is_bounded_to_two_concurrent_tasks(self):
        backend = FakeBackend()
        output, shutdown_requested = await collect(
            [
                '{"requestId":1,"op":"resolve","roomId":"1","quality":"auto"}\n',
                '{"requestId":2,"op":"resolve","roomId":"2","quality":"auto"}\n',
                '{"requestId":3,"op":"resolve","roomId":"3","quality":"auto"}\n',
            ],
            backend,
        )
        self.assertEqual(backend.max_active, 2)
        self.assertEqual({item["requestId"] for item in output}, {1, 2, 3})
        self.assertFalse(shutdown_requested)

    async def test_cancel_prevents_a_late_resolve_response(self):
        backend = FakeBackend()
        output, shutdown_requested = await collect(
            [
                '{"requestId":1,"op":"resolve","roomId":"1","quality":"auto"}\n',
                '{"requestId":2,"op":"cancel","targetRequestId":1}\n',
            ],
            backend,
        )
        self.assertEqual(output, [{"requestId": 2, "ok": True, "cancelled": 1}])
        self.assertFalse(shutdown_requested)

    async def test_room_status_uses_lightweight_operation(self):
        output, shutdown_requested = await collect(
            ['{"requestId":7,"op":"status","query":"63136"}\n'],
            FakeBackend(),
        )
        self.assertEqual(
            output,
            [{"requestId": 7, "ok": True, "status": True, "isLive": False}],
        )
        self.assertFalse(shutdown_requested)

    async def test_backend_exception_is_fixed_and_does_not_leak_text(self):
        backend = FakeBackend()
        backend.fail = True
        output, shutdown_requested = await collect(
            ['{"requestId":1,"op":"resolve","roomId":"1","quality":"auto"}\n'],
            backend,
        )
        self.assertEqual(output[0]["error"]["code"], ErrorCode.SERVICE_FAILED.value)
        self.assertNotIn("private diagnostic", json.dumps(output))
        self.assertFalse(shutdown_requested)


class ShutdownExitTests(unittest.TestCase):
    def test_shutdown_does_not_wait_for_blocked_worker_threads(self):
        repo_root = Path(__file__).resolve().parents[3]
        script = textwrap.dedent(
            f"""
            import asyncio
            import sys
            import threading

            sys.path.insert(0, {str(repo_root)!r})
            import native.service.streamget_service as service

            class BlockingBackend:
                def search(self, _query):
                    threading.Event().wait(20)
                    return []

                def room_status(self, _room_id):
                    return False

                async def resolve(self, _room_id, _quality, _quality_rate=None):
                    return True, [], []

            service.DouyuBackend = BlockingBackend
            asyncio.run(service.main())
            """
        )
        process = subprocess.Popen(
            [sys.executable, "-c", script],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            process.stdin.write('{"requestId":1,"op":"search","query":"busy"}\n')
            process.stdin.flush()
            time.sleep(0.5)

            started = time.monotonic()
            process.stdin.write('{"requestId":2,"op":"shutdown"}\n')
            process.stdin.flush()
            process.stdin.close()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.fail("shutdown waited for a blocked worker thread")
            elapsed = time.monotonic() - started
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stdout.close()
            process.stderr.close()

        self.assertEqual(process.returncode, 0)
        self.assertLess(elapsed, 2.0)


if __name__ == "__main__":
    unittest.main()
