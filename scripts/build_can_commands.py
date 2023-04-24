
import json
import os
import shutil
import glob
import gzip

print('Generating CAN language files...')

projectRoot =  os.path.join(os.path.dirname(os.path.realpath(__file__)), '..')
data_src_dir = os.path.join(projectRoot, 'definitions', 'CAN/')
buildDir = os.path.join(projectRoot, 'build', 'CAN')

if not os.path.exists(buildDir):
   os.makedirs(buildDir)

files_to_convert = []
files_to_convert.extend(glob.glob(os.path.join(data_src_dir, '*.json')))

language_directorys = [os.path.abspath(os.path.join(data_src_dir,name)) for name in os.listdir(os.path.join(data_src_dir, '.')) if os.path.isdir(os.path.join(data_src_dir, name))]

print('  files to convert: ' + str(files_to_convert))
print('  directorys to convert: ' + str(language_directorys))

for file in files_to_convert:
    print('  Converting file: ' + os.path.basename(file))

    commandsFile = open(file, "r", encoding='utf8')
    commands = json.load(commandsFile)["commands"]
    commandsFile.close()

    for langDirectory in language_directorys:
        languageFilePath = os.path.join(langDirectory, os.path.basename(file))

        if not os.path.isfile(languageFilePath):
            continue

        print('  Converting file: ' + languageFilePath)
        languageFile = open(languageFilePath, "r", encoding='utf8')
        languageDefinition = json.load(languageFile)
        languageFile.close()

        srcFile = os.path.join(data_src_dir, os.path.basename(file))
        targetFile = os.path.join(buildDir, os.path.basename(file))
        languageFileDir = os.path.join(buildDir, languageDefinition["Language"])

        if not os.path.exists(languageFileDir):
            os.makedirs(languageFileDir)

        counter = 0
        for definition in languageDefinition["Commands"]:
            defName = definition["name"]
            command = commands[defName]

            for c in command:
                languageDefinition["Commands"][counter][c] = command[c]

            counter += 1

        with open(os.path.join(languageFileDir, os.path.basename(file)), "w", encoding='utf8') as outfile:
            json.dump(languageDefinition, outfile, ensure_ascii=False)

print('Finished generating CAN language files!')