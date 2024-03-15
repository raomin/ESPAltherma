import re
import json
import sys
import os

def hex_to_decimal(hex_string):
    return int(hex_string, 16)

def parse_c_array(filename):
    label_defs = []
    match_count = 0
    with open(filename, 'r', encoding='utf-8') as file:
        lines = file.readlines()
        for line in lines:
            matches = re.findall(r'0x([0-9a-fA-F]+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),"(.*?)"', line)
            for match in matches:
                hex_value = hex_to_decimal(match[0])
                decimal_values = [int(match[i]) for i in range(1, 5)]
                description = match[5]
                label_defs.append([match_count + 1] + [hex_value] + decimal_values + ['"' + description + '"'])
                match_count += 1
    return label_defs, match_count

def main():
    if len(sys.argv) < 3:
        print("Usage: python script.py <source directory> <target directory> [console]")
        return

    sourceDirectory = sys.argv[1]
    targetDirectory = sys.argv[2]
    if not os.path.isdir(sourceDirectory):
        print("Error: Invalid source directory path.")
        return

    if not os.path.isdir(targetDirectory):
        print("Error: Invalid target directory path.")
        return

    consoleOutput = len(sys.argv) == 4 and sys.argv[3] == "console"

    all_label_defs = []
    founded_filenames = {}
    total_match_count = 0
    for root, _, files in os.walk(sourceDirectory):
        for file in files:
            filename = os.path.join(root, file)

            if file in founded_filenames:
                label_defs, match_count = parse_c_array(filename)
                if len(founded_filenames[file]) != match_count:
                    print(f"Error: Missleading label defs count in file {filename}. Org count: {len(founded_filenames[file])}, language count: {match_count}")
                    print(founded_filenames[file])
                    return

                for label_def in label_defs:
                    del label_def[1:6]

                if consoleOutput:
                    print(f"--- BEGIN Output LANGUAGE file '{filename}' ---")
                    for label_def in label_defs:
                        print('[ ' + ',\t'.join(map(lambda x: str(x), label_def)) + '],')
                    print(f"--- END Output of LANGUAGE file '{filename}' ---")
                else:
                    last_folder_name = os.path.basename(os.path.normpath(root))
                    output_path = os.path.join(targetDirectory, last_folder_name)
                    output_file = os.path.join(output_path, file.split('.')[0] + '.json')

                    if not os.path.exists(output_path):
                        os.makedirs(output_path)

                    with open(output_file, 'w', encoding='utf-8') as json_file:
                        for label_def in label_defs:
                            json_file.write('[ ' + ',\t'.join(map(lambda x: str(x), label_def)) + '],\n')
            else:
                label_defs, match_count = parse_c_array(filename)
                total_match_count += match_count
                if label_defs:
                    if consoleOutput:
                        print(f"--- BEGIN Output of file '{filename}' ---")
                        for label_def in label_defs:
                            print('[ ' + ',\t'.join(map(lambda x: str(x), label_def)) + '],')
                        print(f"--- END Output of file '{filename}' ---")
                    else:
                        output_file = os.path.join(targetDirectory, file.split('.')[0] + '.json')
                        with open(output_file, 'w', encoding='utf-8') as json_file:
                            for label_def in label_defs:
                                json_file.write('[ ' + ',\t'.join(map(lambda x: str(x), label_def)) + '],\n')

                founded_filenames[file] = label_defs

    print(f"Scanned C arrays from files in '{sourceDirectory}' and saved as JSON")
    print(f"Found and converted {total_match_count} matches.")

if __name__ == "__main__":
    main()
