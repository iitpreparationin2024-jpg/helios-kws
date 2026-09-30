# WebSocket Transport

HELIOS uses a persistent, full-duplex WebSocket connection at `ws://<host>:8765/helios`.

- Client to server: binary frames containing 16 kHz mono, little-endian int16 PCM.
- Command terminator: text frame `END`.
- Server response: JSON such as `{"text":"...","confidence":"high"}`.

WebSocket avoids an HTTP handshake per chunk and binary frames avoid Base64's 33% expansion. The stream is 32 KB/s before framing overhead. The client connects at boot and reconnects periodically; LAN `ws://` is suitable for demos, while cloud deployments should use `wss://` and a CA bundle.

Test the server without hardware using `server/test_client.py`. Implementation references are `firmware/arduino/HELIOS/wifi_ws.cc` and `server/asr_server.py`.
