from screen_capturer import ScreenCapturer


def test_screen_capturer_produces_jpeg():
    capturer = ScreenCapturer(target_width=320, target_height=180, jpeg_quality=50)
    try:
        jpeg_bytes, cap_ms, enc_ms = capturer.capture_frame()

        # JPEG signature check: Starts with SOI (0xFFD8) and ends with EOI (0xFFD9)
        assert len(jpeg_bytes) > 100
        assert jpeg_bytes[:2] == b"\xff\xd8"
        assert jpeg_bytes[-2:] == b"\xff\xd9"
        assert cap_ms >= 0.0
        assert enc_ms >= 0.0
    finally:
        capturer.close()
