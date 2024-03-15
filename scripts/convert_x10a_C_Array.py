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
            activated = not line.startswith("//")
            for match in matches:
                hex_value = hex_to_decimal(match[0])
                decimal_values = [int(match[i]) for i in range(1, 5)]
                description = match[5]
                label_defs.append([match_count + 1] + [activated] + [hex_value] + decimal_values + ['"' + description + '"'])
                match_count += 1
    return label_defs, match_count

def generateJson(name, language, label_defs):
    jsonTemplateHeader = '''{{
    "Model": "{0}",
    "Language": "{1}",
    "Presets": {{
{2}
    }},
    "Parameters": [
'''
    preset = []
    presetText = ""
    for label_def in label_defs:

        if len(label_def) == 2:
            break

        activated = label_def[1]
        del label_def[1]

        if activated:
            preset.append(label_def[0])

    if len(preset) > 0:
        presetText += '        "Normal": [' + ', '.join(map(lambda x: str(x), preset)) + '],\n'

    preset.clear()
    lightValues = ["(R1T)", "(R2T)", "(R3T)", "(R4T)", "(R5T)", "(R6T)"]

    for label_def in label_defs:
        if len(label_def) == 2:
            break

        for textSearch in lightValues:
            if textSearch in label_def[6]:
                preset.append(label_def[0])

    if len(preset) > 0:
        presetText += '        "Light": [' + ', '.join(map(lambda x: str(x), preset)) + '],\n'

    presetText = presetText[:-2]

    outputContent = jsonTemplateHeader.format(name, language, presetText)
    for label_def in label_defs:
        outputContent += '        [ ' + ',\t'.join(map(lambda x: str(x), label_def)) + '],\n'

    outputContent = outputContent[:-2]
    outputContent += "\n    ]\n}"

    return outputContent

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

                language = os.path.basename(os.path.normpath(root))

                outputContent = generateJson(file.split('.')[0], language, label_defs)

                if consoleOutput:
                    print(f"--- BEGIN Output LANGUAGE file '{filename}' ---")
                    print(outputContent)
                    print(f"--- END Output of LANGUAGE file '{filename}' ---")
                else:
                    output_path = os.path.join(targetDirectory, language)
                    output_file = os.path.join(output_path, file.split('.')[0] + '.json')

                    if not os.path.exists(output_path):
                        os.makedirs(output_path)

                    with open(output_file, 'w', encoding='utf-8') as json_file:
                        json_file.write(outputContent)
            else:
                label_defs, match_count = parse_c_array(filename)
                total_match_count += match_count
                if label_defs:
                    outputContent = generateJson(file.split('.')[0], "English", label_defs)

                    if consoleOutput:
                        print(f"--- BEGIN Output of file '{filename}' ---")
                        print(outputContent)
                        print(f"--- END Output of file '{filename}' ---")
                    else:
                        output_file = os.path.join(targetDirectory, file.split('.')[0] + '.json')
                        with open(output_file, 'w', encoding='utf-8') as json_file:
                            json_file.write(outputContent)

                founded_filenames[file] = label_defs

    print(f"Scanned C arrays from files in '{sourceDirectory}' and saved as JSON")
    print(f"Found and converted {total_match_count} matches.")

if __name__ == "__main__":
    main()
