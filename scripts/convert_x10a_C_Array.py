import re
import json
import sys

def hex_to_decimal(hex_string):
    return int(hex_string, 16)

def parse_c_array(filename):
    label_defs = []
    match_count = 0
    with open(filename, 'r') as file:
        lines = file.readlines()
        for line in lines:
            matches = re.findall(r'0x([0-9a-fA-F]+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),"(.*?)"', line)
            for match in matches:
                hex_value = hex_to_decimal(match[0])
                decimal_values = [int(match[i]) for i in range(1, 5)]
                description = match[5]
                label_defs.append([hex_value] + decimal_values + ['"' + description + '"'])
                match_count += 1
    return label_defs, match_count

def main():
    if len(sys.argv) < 2:
        print("Usage: python script.py <filename> [console]")
        return

    filename = sys.argv[1]
    label_defs, match_count = parse_c_array(filename)

    if len(sys.argv) == 3 and sys.argv[2] == "console":
        for label_def in label_defs:
            print('[ ' + ',\t'.join(map(lambda x: str(x), label_def)) + '],')
    else:
        output_file = filename.split('.')[0] + '.json'
        with open(output_file, 'w') as json_file:
            for label_def in label_defs:
                json_file.write(json.dumps(label_def, separators=(',', ':')) + '\n')

        print(f"Scanned C array from '{filename}' and saved as JSON to '{output_file}'")

    print(f"Found and converted {match_count} matches.")

if __name__ == "__main__":
    main()
