import argparse, queue, sys, time
import numpy as np
import sounddevice as sd
from websocket import create_connection

parser=argparse.ArgumentParser(description='Stream PC microphone to HELIOS ESP32')
parser.add_argument('--esp32', required=True, help='ESP32 IP, e.g. 192.168.1.42')
parser.add_argument('--port', type=int, default=8765)
parser.add_argument('--device', default=None, help='sounddevice input device id/name')
args=parser.parse_args()

RATE=16000
BLOCK=1600  # 100 ms
q=queue.Queue(maxsize=20)

def cb(indata, frames, t, status):
    if status: print('[AUDIO]', status, file=sys.stderr)
    x=np.asarray(indata[:,0] if indata.ndim>1 else indata, dtype=np.float32)
    x=np.clip(x,-1,1)
    pcm=(x*32767).astype('<i2').tobytes()
    try: q.put_nowait(pcm)
    except queue.Full: pass

url=f'ws://{args.esp32}:{args.port}'
print('[WS] connecting',url)
ws=create_connection(url, timeout=5)
print('[WS] connected. Speak into the PC microphone. Ctrl+C to stop.')

try:
    with sd.InputStream(samplerate=RATE, channels=1, dtype='float32', blocksize=BLOCK, device=args.device, callback=cb):
        while True:
            ws.send(q.get(), opcode=2)
except KeyboardInterrupt:
    pass
finally:
    ws.close()
    print('\n[WS] stopped')
