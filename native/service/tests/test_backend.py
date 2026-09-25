import asyncio
import unittest

from native.service.douyu_backend import BackendError, DouyuBackend
from native.service.protocol import ErrorCode


def live_payload(rate=4):
    return {
        "is_live": True,
        "room_id": "63136",
        "title": "Test Room",
    }


def stream_payload(rate=4, rtmp_url="https://stream-shantou.edgesrv.com:443/live"):
    return {
        "error": 0,
        "data": {
            "rate": rate,
            "multirates": [
                {"name": "Original 2K120", "rate": 0},
                {"name": "Super 8M", "rate": 8},
                {"name": "Smooth 4M", "rate": 4},
                {"name": "High", "rate": 3},
                {"name": "Standard", "rate": 2},
            ],
            "rtmp_url": rtmp_url,
            "rtmp_live": "63136.flv?wsAuth=redacted",
        },
    }


class FakeStream:
    def __init__(self, room=None, stream=None, error=None):
        self.room = room if room is not None else live_payload()
        self.stream = stream if stream is not None else stream_payload()
        self.error = error
        self.requested_rates = []

    async def fetch_web_stream_data(self, _url):
        if self.error:
            raise self.error
        return self.room

    async def _fetch_web_stream_url(self, _rid, rate="-1", _cdn=None):
        if self.error:
            raise self.error
        self.requested_rates.append(rate)
        return self.stream


class BackendTests(unittest.TestCase):
    def test_searches_numeric_room_and_maps_metadata(self):
        calls = []

        def fetch_json(url, _timeout):
            calls.append(url)
            return {
                "error": 0,
                "data": {
                    "room_id": 63136,
                    "room_name": "Test Live",
                    "owner_name": "Anchor",
                    "avatar": "https://example.com/avatar.jpg",
                    "cate_name": "Game",
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
                    "anchorName": "Anchor",
                    "avatarUrl": "https://example.com/avatar.jpg",
                    "title": "Test Live",
                    "category": "Game",
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
                    "relateUser": {
                        "list": [
                            {
                                "anchorInfo": {
                                    "rid": "1",
                                    "nickName": "A",
                                    "description": "One",
                                    "cateName": "Game",
                                    "isLive": 1,
                                    "avatar": "https://example.com/a.jpg",
                                }
                            },
                            {
                                "anchorInfo": {
                                    "rid": 1,
                                    "nickName": "A",
                                    "description": "Duplicate",
                                    "cateName": "Game",
                                    "isLive": 1,
                                }
                            },
                            {
                                "anchorInfo": {
                                    "rid": "2",
                                    "nickName": "B",
                                    "description": "Two",
                                    "cateName": "Music",
                                    "isLive": 0,
                                }
                            },
                        ]
                    }
                },
            }

        backend = DouyuBackend(fetch_json=fetch_json)
        results = backend.search("anchor")
        self.assertEqual([item["roomId"] for item in results], ["1", "2"])
        self.assertEqual(results[0]["title"], "One")
        self.assertEqual(results[0]["avatarUrl"], "https://example.com/a.jpg")
        self.assertFalse(results[1]["online"])

    def test_searches_numeric_vip_id_and_maps_real_room_id(self):
        def fetch_json(url, _timeout):
            if url.startswith("https://www.douyu.com/wgapi/livenc/search/overallSearchV8"):
                return {
                    "error": 0,
                    "data": {
                        "relateUser": {
                            "list": [
                                {
                                    "anchorInfo": {
                                        "rid": "12767534",
                                        "vipId": "55588",
                                        "nickName": "可可or",
                                        "description": "【CSTG】出发！",
                                        "cateName": "户外",
                                        "isLive": 1,
                                        "avatar": "https://example.com/avatar.jpg",
                                    }
                                }
                            ]
                        }
                    },
                }
            raise BackendError(ErrorCode.INVALID_RESPONSE)

        backend = DouyuBackend(fetch_json=fetch_json)
        self.assertEqual(
            backend.search("55588"),
            [
                {
                    "roomId": "12767534",
                    "anchorName": "可可or",
                    "avatarUrl": "https://example.com/avatar.jpg",
                    "title": "【CSTG】出发！",
                    "category": "户外",
                    "online": True,
                    "viewerLabel": "0",
                }
            ],
        )

    def test_falls_back_to_legacy_search_when_current_search_is_empty(self):
        calls = []

        def fetch_json(url, _timeout):
            calls.append(url)
            if url.startswith("https://www.douyu.com/wgapi/livenc/search/overallSearchV8"):
                return {"error": 0, "data": {"relateUser": {"list": []}}}
            return {
                "error": 0,
                "data": {
                    "relateShow": [
                        {
                            "rid": "63136",
                            "nickName": "Anchor",
                            "roomName": "Legacy",
                            "cateName": "Game",
                            "isLive": "1",
                            "hot": 1,
                        }
                    ]
                },
            }

        backend = DouyuBackend(fetch_json=fetch_json)
        results = backend.search("legacy")
        self.assertEqual([item["roomId"] for item in results], ["63136"])
        self.assertEqual(len(calls), 2)

    def test_falls_back_to_legacy_search_when_current_search_rejects_request(self):
        calls = []

        def fetch_json(url, _timeout):
            calls.append(url)
            if url.startswith("https://www.douyu.com/wgapi/livenc/search/overallSearchV8"):
                return {"error": 9, "msg": "搜索行为异常", "data": {}}
            return {
                "error": 0,
                "data": {
                    "relateShow": [
                        {
                            "rid": "63136",
                            "nickName": "Anchor",
                            "roomName": "Legacy",
                            "cateName": "Game",
                            "isLive": "1",
                            "hot": 1,
                        }
                    ]
                },
            }

        backend = DouyuBackend(fetch_json=fetch_json, roster_factory=lambda: [])
        results = backend.search("legacy")
        self.assertEqual([item["roomId"] for item in results], ["63136"])
        self.assertEqual(len(calls), 2)

    def test_reports_search_failure_when_both_remote_searches_are_rejected(self):
        def fetch_json(_url, _timeout):
            return {"error": 8, "msg": "您的行为可能存在风险", "data": {}}

        backend = DouyuBackend(fetch_json=fetch_json, roster_factory=lambda: [])
        with self.assertRaises(BackendError) as context:
            backend.search("unknown anchor")
        self.assertEqual(context.exception.code, ErrorCode.INVALID_RESPONSE)
    def test_searches_verified_guild_roster_without_remote_request(self):
        calls = []

        def fetch_json(url, _timeout):
            calls.append(url)
            raise AssertionError("remote search should not be used for a verified roster member")

        backend = DouyuBackend(
            fetch_json=fetch_json,
            roster_factory=lambda: [
                {"id": "hamster-002", "name": "主播阿飞", "roomId": "84452"},
                {"id": "hamster-048", "name": "雾蒙蒙y", "roomId": "12874029"},
            ],
        )
        self.assertEqual(
            [item["roomId"] for item in backend.search("阿飞")],
            ["84452"],
        )
        self.assertEqual(calls, [])

    def test_loads_bundled_roster_for_verified_members(self):
        def fetch_json(_url, _timeout):
            raise AssertionError("bundled roster should avoid a remote search")

        backend = DouyuBackend(fetch_json=fetch_json)
        self.assertEqual(
            [item["roomId"] for item in backend.search("雾蒙蒙y")],
            ["12874029"],
        )
        self.assertEqual(
            [item["roomId"] for item in backend.search("小六HQ")],
            ["12900462"],
        )
        self.assertEqual(
            [item["roomId"] for item in backend.search("阿飞")],
            ["84452"],
        )

    def test_maps_malformed_http_payload_to_fixed_error(self):
        backend = DouyuBackend(fetch_json=lambda _url, _timeout: {"unexpected": True})
        with self.assertRaises(BackendError) as context:
            backend.search("63136")
        self.assertEqual(context.exception.code, ErrorCode.INVALID_RESPONSE)
        self.assertEqual(str(context.exception), ErrorCode.INVALID_RESPONSE.value)

    def test_resolves_live_stream_with_multiple_safe_quality_options(self):
        stream = FakeStream()
        backend = DouyuBackend(stream_factory=lambda: stream)

        is_live, variants, options = asyncio.run(backend.resolve("63136", "auto"))

        self.assertTrue(is_live)
        self.assertEqual(stream.requested_rates, ["-1"])
        self.assertEqual(
            options,
            [
                {"id": "rate-0", "label": "Original 2K120", "rate": 0},
                {"id": "rate-8", "label": "Super 8M", "rate": 8},
                {"id": "rate-4", "label": "Smooth 4M", "rate": 4},
                {"id": "rate-3", "label": "High", "rate": 3},
                {"id": "rate-2", "label": "Standard", "rate": 2},
            ],
        )
        self.assertEqual(
            variants,
            [{
                "id": "flv-4",
                "label": "自动",
                "quality": "auto",
                "qualityRate": 4,
                "container": "flv",
                "playbackUrl": "https://stream-shantou.edgesrv.com:443/live/63136.flv?wsAuth=redacted",
            }],
        )

    def test_resolves_requested_rate_and_labels_selected_variant(self):
        stream = FakeStream(stream=stream_payload(rate=8))
        backend = DouyuBackend(stream_factory=lambda: stream)

        is_live, variants, options = asyncio.run(backend.resolve("63136", "auto", 8))

        self.assertTrue(is_live)
        self.assertEqual(stream.requested_rates, ["8"])
        self.assertEqual(options[1], {"id": "rate-8", "label": "Super 8M", "rate": 8})
        self.assertEqual(variants[0]["qualityRate"], 8)
        self.assertEqual(variants[0]["label"], "Super 8M")
        self.assertEqual(variants[0]["playbackUrl"],
                         "https://stream-shantou.edgesrv.com:443/live/63136.flv?wsAuth=redacted")

    def test_maps_policy_quality_to_rate_when_rate_is_unspecified(self):
        stream = FakeStream(stream=stream_payload(rate=2))
        backend = DouyuBackend(stream_factory=lambda: stream)

        is_live, variants, _options = asyncio.run(backend.resolve("63136", "standard"))

        self.assertEqual(stream.requested_rates, ["2"])
        self.assertTrue(is_live)
        self.assertEqual(variants[0]["qualityRate"], 2)

    def test_preserves_offline_resolution_without_url(self):
        stream = FakeStream(room={"is_live": False, "room_id": "63136"})
        backend = DouyuBackend(stream_factory=lambda: stream)
        is_live, variants, options = asyncio.run(backend.resolve("63136", "auto"))
        self.assertFalse(is_live)
        self.assertEqual(variants, [])
        self.assertEqual(options, [])
        self.assertEqual(stream.requested_rates, [])

    def test_rejects_unsafe_stream_url(self):
        stream = FakeStream(stream=stream_payload(rtmp_url="https://example.invalid/live"))
        backend = DouyuBackend(stream_factory=lambda: stream)
        with self.assertRaises(BackendError) as context:
            asyncio.run(backend.resolve("63136", "auto"))
        self.assertEqual(context.exception.code, ErrorCode.UNSAFE_STREAM_URL)

    def test_rejects_stream_url_with_credentials(self):
        stream = FakeStream(stream=stream_payload(rtmp_url="https://user:secret@stream.edgesrv.com/live"))
        backend = DouyuBackend(stream_factory=lambda: stream)
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
