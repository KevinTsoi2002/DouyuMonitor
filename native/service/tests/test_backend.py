import asyncio
import io
import json
import unittest
from unittest import mock

from native.service import douyu_backend
from native.service.douyu_backend import BackendError, DouyuBackend
from native.service.protocol import ErrorCode


class _FakeHttpResponse(io.BytesIO):
    def __init__(self, payload, headers=None):
        super().__init__(payload)
        self.headers = headers or {}
        self.read_calls = 0

    def getheader(self, name, default=None):
        return self.headers.get(name, default)

    def read(self, size=-1):
        self.read_calls += 1
        return super().read(size)


class _FakeStreamgetResponse:
    def __init__(self, text):
        self.text = text


class _FakeStreamgetHttpClient:
    responses = {}
    options = []

    def __init__(self, **options):
        type(self).options.append(options)

    async def __aenter__(self):
        return self

    async def __aexit__(self, *_args):
        return False

    async def get(self, url, **_kwargs):
        return _FakeStreamgetResponse(self.responses[url])

    async def post(self, url, **_kwargs):
        return _FakeStreamgetResponse(self.responses[url])


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
    def test_default_douyu_stream_verifies_tls_for_all_requests(self):
        from streamget.requests import async_http

        _FakeStreamgetHttpClient.options = []
        _FakeStreamgetHttpClient.responses = {
            "https://www.douyu.com/betard/63136": json.dumps({"room": {
                "room_name": "Test Room",
                "show_status": 1,
                "videoLoop": 0,
                "nickname": "Test Anchor",
                "room_id": "63136",
            }}),
            "https://www.douyu.com/wgapi/livenc/liveweb/websec/getEncryption"
            "?did=10000000000000000000000000001501": json.dumps({
                "error": 0,
                "data": {
                    "rand_str": "seed",
                    "key": "key",
                    "enc_time": 1,
                    "is_special": False,
                    "enc_data": "encoded",
                },
            }),
            "https://playweb.douyucdn.cn/lapi/live/getH5PlayV1/63136": json.dumps({
                "error": 0,
                "data": {
                    "rate": 4,
                    "multirates": [{"name": "Smooth 4M", "rate": 4}],
                    "rtmp_url": "https://stream-shantou.edgesrv.com:443/live",
                    "rtmp_live": "63136.flv?wsAuth=redacted",
                },
            }),
        }

        with mock.patch.object(async_http.httpx, "AsyncClient", _FakeStreamgetHttpClient):
            asyncio.run(DouyuBackend().resolve("63136", "auto"))

        self.assertEqual(len(_FakeStreamgetHttpClient.options), 3)
        self.assertTrue(all(options.get("verify") is True
                            for options in _FakeStreamgetHttpClient.options))

    def test_default_fetch_json_accepts_response_at_limit(self):
        target_size = 1024 * 1024
        padding_size = target_size - len(b'{"error":0,"data":{"padding":""}}')
        payload = json.dumps({"error": 0, "data": {"padding": "x" * padding_size}},
                             separators=(",", ":")).encode()
        self.assertEqual(len(payload), target_size)
        response = _FakeHttpResponse(payload, headers={"Content-Length": str(len(payload))})
        with mock.patch.object(douyu_backend, "urlopen", return_value=response):
            self.assertEqual(douyu_backend._default_fetch_json("https://example.test", 1),
                             {"error": 0, "data": {"padding": "x" * padding_size}})

    def test_default_fetch_json_rejects_body_over_limit_during_read(self):
        payload = b"{" + b"x" * (1024 * 1024 + 1) + b"}"
        response = _FakeHttpResponse(payload)
        with mock.patch.object(douyu_backend, "urlopen", return_value=response):
            with self.assertRaises(BackendError) as context:
                douyu_backend._default_fetch_json("https://example.test", 1)
        self.assertEqual(context.exception.code, ErrorCode.SERVICE_FAILED)
        self.assertGreater(response.read_calls, 1)

    def test_default_fetch_json_rejects_declared_length_before_reading(self):
        response = _FakeHttpResponse(b'{"error":0,"data":{}}',
                                     headers={"Content-Length": str(1024 * 1024 + 1)})
        with mock.patch.object(douyu_backend, "urlopen", return_value=response):
            with self.assertRaises(BackendError) as context:
                douyu_backend._default_fetch_json("https://example.test", 1)
        self.assertEqual(context.exception.code, ErrorCode.SERVICE_FAILED)
        self.assertEqual(response.read_calls, 0)

    def test_default_fetch_json_uses_actual_bytes_when_length_header_is_wrong(self):
        payload = b'{"error":0,"data":{}}'
        for declared in ("1", str(len(payload) + 100), "not-a-length"):
            with self.subTest(declared=declared):
                response = _FakeHttpResponse(payload, headers={"Content-Length": declared})
                with mock.patch.object(douyu_backend, "urlopen", return_value=response):
                    self.assertEqual(
                        douyu_backend._default_fetch_json("https://example.test", 1),
                        {"error": 0, "data": {}},
                    )

    def test_default_fetch_json_rejects_chunked_body_without_length(self):
        payload = b"{" + b"x" * (1024 * 1024 + 1) + b"}"
        response = _FakeHttpResponse(payload)
        with mock.patch.object(douyu_backend, "urlopen", return_value=response):
            with self.assertRaises(BackendError) as context:
                douyu_backend._default_fetch_json("https://example.test", 1)
        self.assertEqual(context.exception.code, ErrorCode.SERVICE_FAILED)

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

    def test_search_with_nan_viewer_count_preserves_room_metadata(self):
        backend = DouyuBackend(fetch_json=lambda _url, _timeout: {
            "error": 0,
            "data": {
                "room_id": "63136",
                "room_name": "Test Live",
                "owner_name": "Anchor",
                "room_status": "1",
                "online": float("nan"),
            },
        })

        self.assertEqual(backend.search("63136"), [{
            "roomId": "63136",
            "anchorName": "Anchor",
            "title": "Test Live",
            "category": "未分类",
            "online": True,
            "viewerLabel": "",
        }])

    def test_viewer_count_boundary_labels(self):
        def fetch_json(_url, _timeout):
            return {"error": 0, "data": {
                "room_id": "63136", "room_name": "Test Live",
                "owner_name": "Anchor", "room_status": "1",
                "online": viewer_count,
            }}

        backend = DouyuBackend(fetch_json=fetch_json)
        cases = (
            (None, "0"), ("", "0"), ("invalid", "0"),
            (0, "0"), (-1, "0"), (1234, "1,234"),
            ("1234", "1,234"), (1234.5, "1,234"),
            (12345, "1.2 万"), ("NaN", ""),
            (float("inf"), ""), (float("-inf"), ""),
        )
        for viewer_count, label in cases:
            with self.subTest(viewer_count=viewer_count):
                self.assertEqual(backend.search("63136")[0]["viewerLabel"], label)

    def test_default_json_fetch_can_decode_nonstandard_nan(self):
        payload = (b'{"error":0,"data":{"room_id":"63136","room_name":"Test Live",'
                   b'"owner_name":"Anchor","room_status":"1","online":NaN}}')
        with mock.patch.object(douyu_backend, "urlopen", return_value=_FakeHttpResponse(payload)):
            backend = DouyuBackend()
            self.assertEqual(backend.search("63136")[0]["viewerLabel"], "")

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

    def test_queries_only_room_status_without_building_search_results(self):
        calls = []

        def fetch_json(url, _timeout):
            calls.append(url)
            return {
                "error": 0,
                "data": {
                    "room_id": "63136",
                    "room_name": "Test Live",
                    "owner_name": "Anchor",
                    "room_status": "0",
                },
            }

        backend = DouyuBackend(fetch_json=fetch_json)
        self.assertFalse(backend.room_status("63136"))
        self.assertEqual(calls, ["https://open.douyucdn.cn/api/RoomApi/room/63136"])

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
    def test_hydrates_exact_roster_search(self):
        calls = []

        def fetch_json(url, _timeout):
            calls.append(url)
            return {"error": 0, "data": {"room_id": "84452", "owner_name": "主播阿飞",
                    "room_name": "Live", "room_status": "1"}}

        backend = DouyuBackend(
            fetch_json=fetch_json,
            roster_factory=lambda: [
                {"id": "hamster-002", "name": "主播阿飞", "roomId": "84452"},
                {"id": "hamster-048", "name": "雾蒙蒙y", "roomId": "12874029"},
            ],
        )
        self.assertEqual(
            [item["roomId"] for item in backend.search("主播阿飞")],
            ["84452"],
        )
        self.assertEqual(len(calls), 1)
        self.assertTrue(backend.search("主播阿飞")[0]["online"])

    def test_merges_roster_hints_and_remote_search(self):
        def fetch_json(_url, _timeout):
            return {"error": 0, "data": {"relateUser": {"list": [
                {"anchorInfo": {"rid": "123", "nickName": "另一个主播", "isLive": 1}},
            ]}}}
        backend = DouyuBackend(fetch_json=fetch_json, roster_factory=lambda: [
            {"name": "主播阿飞", "roomId": "84452"}])
        results = backend.search("主播")
        self.assertEqual([item["roomId"] for item in results], ["84452", "123"])
        self.assertFalse(results[0]["statusKnown"])

    def test_failed_roster_hydration_is_unknown(self):
        def fetch_json(_url, _timeout):
            raise BackendError(ErrorCode.INVALID_RESPONSE)
        backend = DouyuBackend(fetch_json=fetch_json, roster_factory=lambda: [
            {"name": "主播阿飞", "roomId": "84452"}])
        self.assertEqual(backend.search("主播阿飞")[0].get("statusKnown"), False)

    def test_loads_bundled_roster_for_verified_members(self):
        def fetch_json(_url, _timeout):
            raise BackendError(ErrorCode.INVALID_RESPONSE)

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
