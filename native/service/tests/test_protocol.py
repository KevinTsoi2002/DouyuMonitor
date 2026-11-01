import json
import unittest
from native.service import protocol

from native.service.protocol import (
    ErrorCode,
    ProtocolError,
    error_response,
    parse_request,
    serialize_jsonl_response,
    success_resolve,
)


class ProtocolTests(unittest.TestCase):
    def test_search_response_preserves_unknown_and_validates_status(self):
        builder = getattr(protocol, "success_search", None)
        self.assertIsNotNone(builder)
        hint = {"roomId": "123", "anchorName": "Hint", "online": False, "statusKnown": False}
        response = builder(1, [hint])
        self.assertFalse(response["results"][0]["statusKnown"])
        with self.assertRaises(ProtocolError):
            builder(1, [{**hint, "statusKnown": "false"}])

    def test_parses_ping_request(self):
        self.assertEqual(
            parse_request('{"requestId":1,"op":"ping"}'),
            {"requestId": 1, "op": "ping"},
        )

    def test_parses_resolve_request_with_quality(self):
        self.assertEqual(
            parse_request(
                '{"requestId":2,"op":"resolve","roomId":"63136","quality":"auto"}'
            ),
            {
                "requestId": 2,
                "op": "resolve",
                "roomId": "63136",
                "quality": "auto",
            },
        )
        self.assertEqual(
            parse_request(
                '{"requestId":2,"op":"resolve","roomId":"63136","quality":"auto","qualityRate":8}'
            ),
            {
                "requestId": 2,
                "op": "resolve",
                "roomId": "63136",
                "quality": "auto",
                "qualityRate": 8,
            },
        )

    def test_parses_search_cancel_and_shutdown_requests(self):
        self.assertEqual(
            parse_request('{"requestId":3,"op":"search","query":"63136"}'),
            {"requestId": 3, "op": "search", "query": "63136"},
        )
        self.assertEqual(
            parse_request('{"requestId":4,"op":"cancel","targetRequestId":2}'),
            {"requestId": 4, "op": "cancel", "targetRequestId": 2},
        )
        self.assertEqual(
            parse_request('{"requestId":5,"op":"shutdown"}'),
            {"requestId": 5, "op": "shutdown"},
        )
        self.assertEqual(
            parse_request('{"requestId":6,"op":"status","query":"63136"}'),
            {"requestId": 6, "op": "status", "query": "63136"},
        )

    def test_rejects_invalid_requests(self):
        invalid_lines = [
            '{"op":"ping"}',
            '{"requestId":"1","op":"ping"}',
            '{"requestId":1,"op":"unknown"}',
            '{"requestId":1,"op":"resolve","roomId":"abc","quality":"auto"}',
            '{"requestId":1,"op":"resolve","roomId":"1","quality":"lossless"}',
            '{"requestId":1,"op":"resolve","roomId":"1","quality":"auto","qualityRate":-1}',
            '{"requestId":1,"op":"resolve","roomId":"1","quality":"auto","qualityRate":256}',
            '{"requestId":1,"op":"resolve","roomId":"1","quality":"auto","qualityRate":true}',
            '{"requestId":1,"op":"resolve","roomId":"1","quality":"auto","qualityRate":"8"}',
            '{"requestId":1,"op":"search","query":""}',
            '{"requestId":1,"op":"cancel","targetRequestId":0}',
            '[]',
            'not-json',
        ]

        for line in invalid_lines:
            with self.subTest(line=line):
                with self.assertRaises(ProtocolError) as context:
                    parse_request(line)
                self.assertEqual(context.exception.code, ErrorCode.INVALID_INPUT)

    def test_rejects_invalid_operation_types_and_values(self):
        for operation in (None, False, True, 0, 1, [], ["ping"], {}, {"name": "ping"},
                          "", "unknown", " PING "):
            with self.subTest(operation=operation):
                line = json.dumps({"requestId": 7, "op": operation})
                with self.assertRaises(ProtocolError) as context:
                    parse_request(line)
                self.assertEqual(context.exception.code, ErrorCode.INVALID_INPUT)

        for value in ({"requestId": 7}, {"requestId": True, "op": []}):
            with self.subTest(request=value):
                with self.assertRaises(ProtocolError) as context:
                    parse_request(json.dumps(value))
                self.assertEqual(context.exception.code, ErrorCode.INVALID_INPUT)

        for value in ([], "ping", 1, None):
            with self.subTest(top_level=value):
                with self.assertRaises(ProtocolError) as context:
                    parse_request(json.dumps(value))
                self.assertEqual(context.exception.code, ErrorCode.INVALID_INPUT)

    def test_rejects_room_ids_longer_than_twenty_digits(self):
        with self.assertRaises(ProtocolError):
            parse_request(
                '{"requestId":1,"op":"resolve","roomId":"123456789012345678901","quality":"auto"}'
            )

    def test_builds_offline_success_response_without_url(self):
        self.assertEqual(
            success_resolve(6, "63136", False, []),
            {
                "requestId": 6,
                "ok": True,
                "roomId": "63136",
                "isLive": False,
                "variants": [],
            },
        )

    def test_builds_success_response_with_safe_quality_options(self):
        response = success_resolve(
            8,
            "63136",
            True,
            [{"id": "flv-8", "label": "Super 8M", "quality": "auto", "qualityRate": 8,
              "container": "flv", "playbackUrl": "https://live.douyucdn.cn/stream.flv"}],
            [{"id": "rate-8", "label": "Super 8M", "rate": 8}],
        )
        self.assertEqual(response["qualityOptions"], [{"id": "rate-8", "label": "Super 8M", "rate": 8}])
        self.assertNotIn("playbackUrl", json.dumps(response["qualityOptions"]))

    def test_builds_fixed_error_response_without_diagnostics(self):
        response = error_response(7, ErrorCode.TIMEOUT)
        self.assertEqual(
            response,
            {
                "requestId": 7,
                "ok": False,
                "error": {"code": "TIMEOUT", "retryable": True},
            },
        )
        self.assertNotIn("message", json.dumps(response))
        self.assertNotIn("url", json.dumps(response).lower())

    def test_serializes_jsonl_responses_with_a_byte_bound(self):
        small = serialize_jsonl_response({"requestId": 1, "ok": True})
        self.assertEqual(small, b'{"requestId":1,"ok":true}\n')

        with self.assertRaises(ProtocolError):
            serialize_jsonl_response({
                "requestId": 2,
                "ok": True,
                "results": [{"roomId": "1", "anchorName": "x" * 1_100_000,
                              "online": False}],
            })
        bounded_error = serialize_jsonl_response(error_response(2, ErrorCode.INVALID_RESPONSE))
        decoded = json.loads(bounded_error)
        self.assertEqual(decoded["requestId"], 2)
        self.assertFalse(decoded["ok"])
        self.assertLessEqual(len(bounded_error) - 1, protocol.MAX_JSONL_RESPONSE_BYTES)


if __name__ == "__main__":
    unittest.main()
