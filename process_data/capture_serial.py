"""Capture only: python capture_serial.py --help. No vital-sign estimation."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time
from capture_protocol import Decoder


def expect(port, command, reply, timeout=3, transcript=None):
    port.write(command.encode('ascii')+b'\n')
    port.flush()
    pending = bytearray()
    deadline = time.monotonic()+timeout
    while time.monotonic() < deadline:
        chunk = port.read(1)
        if not chunk: continue
        pending.extend(chunk)
        if len(pending) > 1024: del pending[:-256]
        if chunk == b'\n':
            line = bytes(pending).strip()
            if transcript is not None: transcript.append(line.decode('ascii', errors='replace'))
            if line == reply.encode('ascii'): return
            if line.startswith(b'AT+ERROR='): raise RuntimeError(line.decode())
            pending.clear()
    raise TimeoutError(f'{command}: did not receive {reply}')


def sync(port, transcript):
    # In work state any RX byte resets the old firmware. A retry after boot
    # obtains an explicit AT+OK, instead of assuming the first command survived.
    deadline = time.monotonic()+10
    while time.monotonic() < deadline:
        try:
            expect(port, 'AT', 'AT+OK', timeout=2, transcript=transcript)
            return
        except TimeoutError:
            pass
    raise TimeoutError('Radar did not return to configuration state within 10 seconds')


def bridge_status(port):
    port.write(b'AT+STATUS?\n'); port.flush()
    deadline = time.monotonic()+2
    while time.monotonic()<deadline:
        line = port.readline().strip()
        if line.startswith(b'AT+STATUS='):
            values = [int(v) for v in line.split(b'=')[1].split(b',')]
            if len(values) != 3: raise RuntimeError('Malformed bridge status')
            return values
    raise TimeoutError('No bridge status reply')


def capture(args, serial_factory=None):
    if serial_factory is None:
        import serial
        serial_factory = serial.Serial
    if args.duration <= 0: raise ValueError('duration must be positive')
    output = Path(args.output)
    # A session directory must be new; never overwrite an earlier recording.
    output.mkdir(parents=True, exist_ok=False)
    session = dict(port=args.port, baud=args.baud, link=args.link, mode=args.mode,
                   duration_requested_s=args.duration, started_utc=datetime.now(timezone.utc).isoformat(),
                   status='initializing', transcript=[])
    decoder = Decoder('legacy' if args.mode == 'LEGACY' else 'new')
    port = None
    error = None
    frame_times = []
    try:
        port = serial_factory(args.port, args.baud, timeout=0.05, write_timeout=2)
        port.reset_input_buffer()
        if args.link == 'bridge':
            expect(port, 'AT+LINK=BRIDGE', 'AT+LINK=BRIDGE', transcript=session['transcript'])
            session['bridge_initial_counters'] = bridge_status(port)
        sync(port, session['transcript'])
        expect(port, 'AT+MODE='+args.mode, 'AT+MODE='+args.mode, transcript=session['transcript'])
        expect(port, 'AT+START', 'AT+STARTED', transcript=session['transcript'])
        session['status'] = 'recording'
        start = last_data = time.monotonic()
        offset = 0
        with (output/'raw.bin').open('xb') as raw, (output/'frames.jsonl').open('x', encoding='utf-8') as meta, (output/'chunks.jsonl').open('x', encoding='utf-8') as chunks:
            while time.monotonic()-start < args.duration:
                data = port.read(min(max(port.in_waiting, 1), 8192))
                now = time.monotonic()
                if not data:
                    if now-last_data > args.idle_timeout:
                        raise TimeoutError('No radar data: stopped recording')
                    continue
                last_data = now
                receipt = time.time_ns()
                raw.write(data)
                chunks.write(json.dumps(dict(offset=offset, length=len(data), host_receive_ns=receipt))+'\n')
                offset += len(data)
                for frame in decoder.feed(data):
                    if frame.mode != args.mode:
                        raise RuntimeError(f'Expected {args.mode}, received {frame.mode}')
                    row = frame.metadata()
                    row['host_receive_ns'] = receipt
                    meta.write(json.dumps(row)+'\n')
                    if frame.ticks is not None: frame_times.append((frame.ticks, frame.tick_hz))
        if not decoder.stats['frames']: raise RuntimeError('No valid frames were received')
        session['duration_actual_s'] = time.monotonic()-start
        session['status'] = 'captured'
    except BaseException as exc:
        error = exc
        session['status'] = 'failed'
        session['error'] = f'{type(exc).__name__}: {exc}'
    finally:
        if port is not None:
            stop_errors = []
            try:
                sync(port, session['transcript'])
            except Exception as exc:
                stop_errors.append(exc)
            if args.link == 'bridge':
                try:
                    status = bridge_status(port)
                    baseline = session.get('bridge_initial_counters', status)
                    delta = [(b-a) & 0xffffffff for a,b in zip(baseline,status)]
                    session['bridge_errors'] = dict(zip(('radar_overflow','host_overflow','uart_errors'),delta))
                    if any(delta): raise RuntimeError('Bridge reported lost bytes or UART errors')
                except Exception as exc:
                    stop_errors.append(exc)
                try:
                    # Release PA2 even when radar handshake or diagnostics failed.
                    expect(port,'AT+LINK=DIRECT','AT+LINK=DIRECT',transcript=session['transcript'])
                except Exception as exc:
                    stop_errors.append(exc)
            if stop_errors:
                session['stop_error'] = '; '.join(f'{type(e).__name__}: {e}' for e in stop_errors)
                session['status'] = 'failed'
                if error is None: error = stop_errors[0]
            port.close()
        session['decoder'] = decoder.finish()
        if any(decoder.stats[k] for k in ('crc_errors','malformed','missing_frames','resets','sequence_errors')):
            if session['status'] != 'failed': session['status'] = 'captured_with_errors'
        intervals = [(b[0]-a[0])/a[1] for a,b in zip(frame_times,frame_times[1:]) if b[0]>=a[0] and b[1]==a[1]]
        if intervals:
            session['device_interval_s'] = dict(min=min(intervals), max=max(intervals), mean=sum(intervals)/len(intervals))
        (output/'session.json').write_text(json.dumps(session,ensure_ascii=False,indent=2),encoding='utf-8')
    if error is not None: raise error
    return session


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--baud', type=int, default=921600)
    parser.add_argument('--link', choices=['direct','bridge'], default='direct')
    parser.add_argument('--mode', choices=['ADC','IQ','POINT','LEGACY'], default='LEGACY')
    parser.add_argument('--duration', type=float, default=180)
    parser.add_argument('--idle-timeout', type=float, default=5)
    parser.add_argument('--output', default='captures/'+datetime.now().strftime('%Y%m%d_%H%M%S'))
    args = parser.parse_args()
    result = capture(args)
    print(json.dumps(dict(status=result['status'], decoder=result['decoder']), indent=2))
    return 0 if result['status']=='captured' else 2


if __name__ == '__main__':
    raise SystemExit(main())
