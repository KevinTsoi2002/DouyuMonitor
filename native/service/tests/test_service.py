import asyncio
import json
import unittest

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

    await run_service(source(), emit, backend)
    return output


class ServiceTests(unittest.IsolatedAsyncioTestCase):
    async def test_ping_and_malformed_input_keep_the_loop_alive(self):
        output = await collect(
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

    async def test_resolve_is_bounded_to_two_concurrent_tasks(self):
        backend = FakeBackend()
        output = await collect(
            [
                '{"requestId":1,"op":"resolve","roomId":"1","quality":"auto"}\n',
                '{"requestId":2,"op":"resolve","roomId":"2","quality":"auto"}\n',
                '{"requestId":3,"op":"resolve","roomId":"3","quality":"auto"}\n',
            ],
            backend,
        )
        self.assertEqual(backend.max_active, 2)
        self.assertEqual({item["requestId"] for item in output}, {1, 2, 3})

    async def test_cancel_prevents_a_late_resolve_response(self):
        backend = FakeBackend()
        output = await collect(
            [
                '{"requestId":1,"op":"resolve","roomId":"1","quality":"auto"}\n',
                '{"requestId":2,"op":"cancel","targetRequestId":1}\n',
            ],
            backend,
        )
        self.assertEqual(output, [{"requestId": 2, "ok": True, "cancelled": 1}])

    async def test_backend_exception_is_fixed_and_does_not_leak_text(self):
        backend = FakeBackend()
        backend.fail = True
        output = await collect(
            ['{"requestId":1,"op":"resolve","roomId":"1","quality":"auto"}\n'],
            backend,
        )
        self.assertEqual(output[0]["error"]["code"], ErrorCode.SERVICE_FAILED.value)
        self.assertNotIn("private diagnostic", json.dumps(output))


if __name__ == "__main__":
    unittest.main()
