"""
Decoder of ktb files, used by official Daikin service Checker.
ktb stands for "KishuTable".
"""
import glob
import argparse
    # private byte[] DecodeType3Data(byte[] data)
    # {
    #   byte[] numArray = new byte[data.Length];
    #   for (int index = 0; index < data.Length; ++index)
    #   {
    #     byte num = (byte) (((int) data[index] >> 4 & 15) + ((int) data[index] << 4 & 240));
    #     numArray[index] = ~num;
    #   }
    #   return numArray;
    # }

def decode_d3(src: bytes) -> bytes:
    """"Magic" taken from DecodeType3Data in D3Command.cs."""
    decoded = bytes([~((src_byte >> 4 & 15) + (src_byte << 4 & 240)) % 256 for src_byte in src])

    return decoded

def parse_cli() -> argparse.Namespace:
    """Parse command line options."""
    example_text = """
    python .\\bin\\ktb_decoder.py set_decoded.txt 'C:\\\\Program Files (x86)\\\\Daikin\\\\Checker4\\\\D3Model_en\\\\*.set'
    python .\\bin\\ktb_decoder.py ktb_decoded.txt 'C:\\\\Program Files (x86)\\\\Daikin\\\\Checker4\\\\D3Model_en\\\\*.ktb'
    """
    parser = argparse.ArgumentParser(epilog=example_text)
    parser.add_argument('outfile', type=str,
                    help='Output file')
    parser.add_argument('infiles', type=str,
                    help='Input files pattern (glob supported)')
    args = parser.parse_args()
    return args

def main():
    args = parse_cli()
    with open(args.outfile, "wb") as output_file:
        for filepath in glob.iglob(args.infiles):
            separator = f"========== {filepath} =========="
            print(separator)
            with open(filepath, "rb") as f:
                f_bytes = f.read()
                # print(f_bytes)
                decoded = decode_d3(f_bytes)
                print(decoded.decode("utf-8", "backslashreplace"))
                output_file.write(separator.encode() + b"\n\n" + decoded + b"\n\n")

if __name__ == "__main__":
    main()
