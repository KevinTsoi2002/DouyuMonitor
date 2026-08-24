import asyncio
import unittest

from native.service.douyu_backend import BackendError, DouyuBackend
from native.service.protocol import ErrorCode


class FakeStream:
    def __init__(self, payload=None, error=None):
        self.payload = payload
        self.error = error

    async def fetch_app_stream_data(self, _url):
        if self.error:
            raise self.error
        return self.payload


class BackendTests(unittest.TestCase):
    def test_searches_numeric_room_and_maps_metadata(self):
        calls = []

        def fetch_json(url, _timeout):
            calls.append(url)
            return {
                "error": 0,
                "data": {
                    "room_id": 63136,
                    "room_name": "测试直播",
                    "owner_name": "主播",
                    "avatar": "https://example.com/avatar.jpg",
                    "cate_name": "游戏",
                    "room_status": "1",
                    "online": 12345,
                },
            }

        backend = DouyuBackend(fetch_json=fetch_json)
        self.assertEqual(
            backend.search("63136"),
            [
                {
                    "roomId": "63136",
                    "anchorName": "主播",
                    "avatarUrl": "https://example.com/avatar.jpg",
                    "title": "测试直播",
                    "category": "游戏",
                    "online": True,
                    "viewerLabel": "1.2 万",
                }
            ],
        )
        self.assertEqual(calls, ["https://open.douyucdn.cn/api/RoomApi/room/63136"])

    def test_searches_anchor_name_and_deduplicates_room_ids(self):
        def fetch_json(_url, _timeout):
            return {
                "error": 0,
                "data": {
                    "relateShow": [
                        {"rid": "1", "nickName": "甲", "roomName": "一", "cateName": "游戏", "isLive": "1", "hot": 100},
                        {"rid": 1, "nickName": "甲", "roomName": "重复", "cateName": "游戏", "isLive": "1", "hot": 1},
                        {"rid": "2", "nickName": "乙", "roomName": "二", "cateName": "音乐", "isLive": "0", "hot": 0},
                    ]
                },
            }

        backend = DouyuBackend(fetch_json=fetch_json)
        results = backend.search("主播")
        self.assertEqual([item["roomId"] for item in results], ["1", "2"])
        self.assertEqual(results[0]["viewerLabel"], "100")
        self.assertFalse(results[1]["online"])

    def test_maps_malformed_http_payload_to_fixed_error(self):
        backend = DouyuBackend(fetch_json=lambda _url, _timeout: {"unexpected": True})
        with self.assertRaises(BackendError) as context:
            backend.search("63136")
        self.assertEqual(context.exception.code, ErrorCode.INVALID_RESPONSE)
        self.assertEqual(str(context.exception), ErrorCode.INVALID_RESPONSE.value)

    def test_resolves_live_stream_to_one_auto_flv_variant(self):
        backend = DouyuBackend(
            stream_factory=lambda: FakeStream(
                {"is_live": True, "flv_url": "https://live.douyucdn2.cn/live/63136.flv?wsAuth=redacted"}
            )
        )
        is_live, variants = asyncio.run(backend.resolve("63136", "auto"))
        self.assertTrue(is_live)
        self.assertEqual(variants[0]["quality"], "auto")
        self.assertEqual(variants[0]["container"], "flv")
        self.assertEqual(variants[0]["playbackUrl"], "https://live.douyucdn2.cn/live/63136.flv?wsAuth=redacted")

    def test_preserves_offline_resolution_without_url(self):
        backend = DouyuBackend(stream_factory=lambda: FakeStream({"is_live": False}))
        is_live, variants = asyncio.run(backend.resolve("63136", "auto"))
        self.assertFalse(is_live)
        self.assertEqual(variants, [])

    def test_rejects_unsafe_stream_url(self):
        backend = DouyuBackend(
            stream_factory=lambda: FakeStream(
                {"is_live": True, "flv_url": "https://example.invalid/live.flv"}
            )
        )
        with self.assertRaises(BackendError) as context:
            asyncio.run(backend.resolve("63136", "auto"))
        self.assertEqual(context.exception.code, ErrorCode.UNSAFE_STREAM_URL)

    def test_rejects_stream_url_with_credentials(self):
        backend = DouyuBackend(
            stream_factory=lambda: FakeStream(
                {"is_live": True, "flv_url": "https://user:secret@live.douyucdn.cn/live.flv"}
            )
        )
        with self.assertRaises(BackendError) as context:
            asyncio.run(backend.resolve("63136", "auto"))
        self.assertEqual(context.exception.code, ErrorCode.UNSAFE_STREAM_URL)

    def test_maps_resolver_exception_without_diagnostics(self):
        backend = DouyuBackend(stream_factory=lambda: FakeStream(error=RuntimeError("secret traceback")))
        with self.assertRaises(BackendError) as context:
            asyncio.run(backend.resolve("63136", "auto"))
        self.assertEqual(context.exception.code, ErrorCode.STREAMGET_UNAVAILABLE)
        self.assertEqual(str(context.exception), ErrorCode.STREAMGET_UNAVAILABLE.value)


if __name__ == "__main__":
    unittest.main()
