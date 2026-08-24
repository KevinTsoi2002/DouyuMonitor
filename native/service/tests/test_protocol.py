import json
import unittest

from native.service.protocol import (
    ErrorCode,
    ProtocolError,
    error_response,
    parse_request,
    success_resolve,
)


class ProtocolTests(unittest.TestCase):
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

    def test_rejects_invalid_requests(self):
        invalid_lines = [
            '{"op":"ping"}',
            '{"requestId":"1","op":"ping"}',
            '{"requestId":1,"op":"unknown"}',
            '{"requestId":1,"op":"resolve","roomId":"abc","quality":"auto"}',
            '{"requestId":1,"op":"resolve","roomId":"1","quality":"lossless"}',
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


if __name__ == "__main__":
    unittest.main()
