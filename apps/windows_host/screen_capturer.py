"""
Screen capture and JPEG compression worker using mss and Pillow.
Supports graceful handling of locked Windows sessions and display sleep states.
"""

import io
import time
from typing import Optional, Tuple
from PIL import Image, ImageDraw
import mss
from mss.exception import ScreenShotError


class ScreenCapturer:
    """Captures desktop screen frames and encodes them to JPEG."""

    def __init__(
        self,
        target_width: Optional[int] = 1280,
        target_height: Optional[int] = 720,
        jpeg_quality: int = 65,
        monitor_index: int = 1,
    ):
        self.target_width = target_width or 1280
        self.target_height = target_height or 720
        self.jpeg_quality = max(10, min(95, jpeg_quality))
        self.monitor_index = monitor_index
        self._sct = mss.MSS() if hasattr(mss, "MSS") else mss.mss()
        self.last_grab_was_real = False

    def capture_frame(self) -> Tuple[bytes, float, float]:
        """
        Captures the configured monitor, resizes it if needed, and encodes to JPEG.

        If the Windows screen is locked or powered off (GDI BitBlt restriction),
        generates an informative lock screen status frame instead of crashing.

        Returns:
            (jpeg_bytes, capture_duration_ms, encode_duration_ms)
        """
        t0 = time.perf_counter()
        img: Optional[Image.Image] = None

        try:
            monitors = self._sct.monitors
            if self.monitor_index >= len(monitors):
                mon = monitors[1] if len(monitors) > 1 else monitors[0]
            else:
                mon = monitors[self.monitor_index]

            sct_img = self._sct.grab(mon)
            t1 = time.perf_counter()

            # Convert BGRA to RGB Pillow Image
            img = Image.frombytes("RGB", sct_img.size, sct_img.bgra, "raw", "BGRX")
            self.last_grab_was_real = True

            # Resize if target dimensions differ from original
            if self.target_width and self.target_height:
                if img.size != (self.target_width, self.target_height):
                    img = img.resize(
                        (self.target_width, self.target_height),
                        Image.Resampling.BILINEAR,
                    )
        except (ScreenShotError, OSError):
            t1 = time.perf_counter()
            self.last_grab_was_real = False
            # Generate lock/sleep status frame
            img = Image.new("RGB", (self.target_width, self.target_height), color=(15, 23, 42))
            draw = ImageDraw.Draw(img)
            msg = "SecondScreen: Windows Desktop is Locked or Asleep"
            sub = "Unlock Windows or wake screen to resume live desktop capture."
            draw.text((self.target_width // 4, self.target_height // 2 - 20), msg, fill=(241, 245, 249))
            draw.text((self.target_width // 4, self.target_height // 2 + 10), sub, fill=(148, 163, 184))

        # Encode to JPEG in memory
        buffer = io.BytesIO()
        img.save(buffer, format="JPEG", quality=self.jpeg_quality, optimize=False)
        jpeg_bytes = buffer.getvalue()
        t2 = time.perf_counter()

        capture_ms = (t1 - t0) * 1000.0
        encode_ms = (t2 - t1) * 1000.0

        return jpeg_bytes, capture_ms, encode_ms

    def close(self):
        """Releases capture resources."""
        if self._sct:
            self._sct.close()
            self._sct = None
