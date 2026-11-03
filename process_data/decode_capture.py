"""Decode recordings to a streaming JSONL file; sample arrays are optional."""
import argparse
import json
from pathlib import Path
from capture_protocol import Decoder


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('input')
    p.add_argument('--format', choices=['new','legacy','adc-legacy'], default='new')
    p.add_argument('--output', required=True)
    p.add_argument('--values', action='store_true')
    args=p.parse_args()
    decoder=Decoder(args.format)
    with Path(args.input).open('rb') as source, Path(args.output).open('x',encoding='utf-8') as target:
        while chunk:=source.read(8192):
            for frame in decoder.feed(chunk):
                row=frame.metadata()
                if args.values:
                    values=frame.values()
                    if frame.mode=='LEGACY':
                        row['real'],row['imag']=values[:2]; row['point_hex']=values[2].hex()
                    elif frame.mode=='IQ': row['real'],row['imag']=values
                    elif frame.mode in ('ADC','ADC_LEGACY'): row['samples']=values
                    else: row['point_hex']=values.hex()
                target.write(json.dumps(row)+'\n')
    print(json.dumps(decoder.finish(),indent=2))


if __name__=='__main__': main()
