"""
SecondScreen Windows Streaming Host Server (Prototype Phase 3)
Streams Windows desktop frames encoded as JPEG over TCP to the Flutter Android receiver.
"""

import argparse
import socket
import sys
import time
from typing import List
from protocol import encode_frame_packet
from screen_capturer import ScreenCapturer


def get_local_ip_addresses() -> List[str]:
    """Finds all non-loopback IPv4 addresses assigned to active local interfaces."""
    ip_list = []
    try:
        # Method 1: Connect to a public DNS IP (doesn't send traffic) to find outbound interface
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("8.8.8.8", 80))
            primary_ip = s.getsockname()[0]
            if primary_ip and primary_ip not in ip_list and not primary_ip.startswith("127."):
                ip_list.append(primary_ip)
    except Exception:
        pass

    try:
        # Method 2: Hostname inspection
        hostname = socket.gethostname()
        for ip in socket.gethostbyname_ex(hostname)[2]:
            if not ip.startswith("127.") and ip not in ip_list:
                ip_list.append(ip)
    except Exception:
        pass

    return ip_list or ["127.0.0.1"]


class StreamingServer:
    def __init__(
        self,
        host: str = "0.0.0.0",
        port: int = 52025,
        target_fps: int = 15,
        width: int = 1280,
        height: int = 720,
        quality: int = 65,
    ):
        self.host = host
        self.port = port
        self.target_fps = max(1, min(60, target_fps))
        self.width = width
        self.height = height
        self.quality = quality
        self._running = False
        self._server_sock: socket.socket | None = None

    def start(self):
        """Starts the TCP streaming server."""
        self._running = True
        self._server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            self._server_sock.bind((self.host, self.port))
            self._server_sock.listen(1)
        except Exception as e:
            print(f"[ERROR] Failed to bind to {self.host}:{self.port} - {e}")
            sys.exit(1)

        local_ips = get_local_ip_addresses()
        print("=" * 60)
        print("  SecondScreen Windows Streaming Host (Phase 3 Prototype)")
        print("=" * 60)
        print(f"Listening on port: {self.port}")
        print("Reachable Local IPv4 Addresses for Android connection:")
        for ip in local_ips:
            print(f"  --> {ip}:{self.port}")
        print(f"Configuration: {self.width}x{self.height} @ {self.target_fps} FPS (JPEG Quality: {self.quality})")
        print("Press Ctrl+C to stop the server.")
        print("=" * 60)

        capturer = ScreenCapturer(
            target_width=self.width,
            target_height=self.height,
            jpeg_quality=self.quality,
        )

        try:
            while self._running:
                print("\n[INFO] Waiting for Android device to connect...")
                try:
                    client_sock, client_addr = self._server_sock.accept()
                except (socket.error, OSError):
                    break

                # Enable TCP_NODELAY for lowest latency transmission
                client_sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                # Set 10-second timeout on socket operations
                client_sock.settimeout(10.0)

                print(f"[INFO] Android client connected from {client_addr[0]}:{client_addr[1]}")
                self._stream_to_client(client_sock, capturer)
                print(f"[INFO] Client {client_addr[0]} disconnected.")
        finally:
            capturer.close()
            self.stop()

    def _stream_to_client(self, client_sock: socket.socket, capturer: ScreenCapturer):
        """Streams desktop frames to a connected client at target FPS."""
        frame_interval = 1.0 / self.target_fps
        frames_sent = 0
        total_bytes_sent = 0
        stats_start = time.perf_counter()

        try:
            while self._running:
                loop_start = time.perf_counter()

                # 1. Capture and encode frame
                jpeg_bytes, cap_ms, enc_ms = capturer.capture_frame()

                # 2. Package with protocol framing
                packet = encode_frame_packet(jpeg_bytes)

                # 3. Transmit packet over TCP
                client_sock.sendall(packet)

                frames_sent += 1
                total_bytes_sent += len(packet)

                # Periodic diagnostic logging every 5 seconds
                elapsed_stats = time.perf_counter() - stats_start
                if elapsed_stats >= 5.0:
                    effective_fps = frames_sent / elapsed_stats
                    kbps = (total_bytes_sent * 8 / 1024) / elapsed_stats
                    avg_kb = (total_bytes_sent / frames_sent) / 1024
                    print(
                        f"[STATS] Sent: {effective_fps:.1f} FPS | "
                        f"Bandwidth: {kbps:.0f} kbps | "
                        f"Avg Frame: {avg_kb:.1f} KB | "
                        f"Cap: {cap_ms:.1f}ms | Enc: {enc_ms:.1f}ms"
                    )
                    frames_sent = 0
                    total_bytes_sent = 0
                    stats_start = time.perf_counter()

                # Pace frames according to target FPS
                processing_time = time.perf_counter() - loop_start
                sleep_time = frame_interval - processing_time
                if sleep_time > 0:
                    time.sleep(sleep_time)

        except (ConnectionResetError, BrokenPipeError, ConnectionAbortedError):
            print("[INFO] Connection closed by Android client.")
        except socket.timeout:
            print("[WARNING] Socket timed out during transmission.")
        except Exception as e:
            print(f"[ERROR] Streaming error: {e}")
        finally:
            try:
                client_sock.shutdown(socket.SHUT_RDWR)
                client_sock.close()
            except Exception:
                pass

    def stop(self):
        """Stops the server cleanly."""
        self._running = False
        if self._server_sock:
            try:
                self._server_sock.close()
            except Exception:
                pass
            self._server_sock = None
        print("\n[INFO] Streaming server stopped.")


def main():
    parser = argparse.ArgumentParser(description="SecondScreen Windows Streaming Host")
    parser.add_argument("--host", default="0.0.0.0", help="Bind IP address (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=52025, help="Port to listen on (default: 52025)")
    parser.add_argument("--fps", type=int, default=15, help="Target frame rate (default: 15 FPS)")
    parser.add_argument("--width", type=int, default=1280, help="Frame width (default: 1280)")
    parser.add_argument("--height", type=int, default=720, help="Frame height (default: 720)")
    parser.add_argument("--quality", type=int, default=65, help="JPEG quality 10-95 (default: 65)")

    args = parser.parse_args()

    server = StreamingServer(
        host=args.host,
        port=args.port,
        target_fps=args.fps,
        width=args.width,
        height=args.height,
        quality=args.quality,
    )

    try:
        server.start()
    except KeyboardInterrupt:
        server.stop()


if __name__ == "__main__":
    main()
