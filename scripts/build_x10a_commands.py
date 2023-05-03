
import json
import os
import glob

print('Generating X10A language files...')

projectRoot =  os.path.join(os.path.dirname(os.path.realpath(__file__)), '..')
data_src_dir = os.path.join(projectRoot, 'definitions', 'X10A')
buildDir = os.path.join(projectRoot, 'build', 'X10A')

if not os.path.exists(buildDir):
   os.makedirs(buildDir)

files_to_convert = []
files_to_convert.extend(glob.glob(os.path.join(data_src_dir, '*.json')))

language_directorys = [os.path.abspath(os.path.join(data_src_dir,name)) for name in os.listdir(os.path.join(data_src_dir, '.')) if os.path.isdir(os.path.join(data_src_dir, name))]

print('  files to convert: ' + str(files_to_convert))
print('  directorys to convert: ' + str(language_directorys))

for file in files_to_convert:
    print('  Converting file: ' + os.path.basename(file))

    modelFile = open(file, "r", encoding='utf8')
    modelDefinition = json.load(modelFile)
    modelFile.close()

    # remove comments in file
    if "Comment" in modelDefinition:
        del modelDefinition["Comment"]

    idx = 0
    while idx < len(modelDefinition["Parameters"]):
        if len(modelDefinition["Parameters"][idx]) == 1:
            modelDefinition["Parameters"].pop(idx)
        else:
            idx += 1

    modelFileLanguageDir = os.path.join(buildDir, modelDefinition["Language"])

    if not os.path.exists(modelFileLanguageDir):
        os.makedirs(modelFileLanguageDir)

    newModelFileLocation = os.path.join(modelFileLanguageDir, os.path.basename(file))

    # compress json file
    with open(newModelFileLocation, "w", encoding='utf8') as outfile:
        json.dump(modelDefinition, outfile, ensure_ascii=False)

    # update language files
    for langDirectory in language_directorys:
        languageFilePath = os.path.join(langDirectory, os.path.basename(file))

        if not os.path.isfile(languageFilePath):
            continue

        print('  Converting language file: ' + languageFilePath)

        languageFile = open(languageFilePath, "r", encoding='utf8')
        languageDefinition = json.load(languageFile)
        languageFile.close()

        modelDefinition["Language"] = languageDefinition["Language"]

        languageFileDir = os.path.join(buildDir, languageDefinition["Language"])

        if len(languageDefinition["Parameters"]) != len(modelDefinition["Parameters"]):
            print('Index missmatch between file ' + languageFilePath + ': ' + str(len(languageDefinition["Parameters"])) + ' -> ' + str(len(modelDefinition["Parameters"])))
            continue

        counter = 0
        for parameter in languageDefinition["Parameters"]:
            modelDefinition["Parameters"][counter][5] = languageDefinition["Parameters"][counter]
            counter += 1

        if not os.path.exists(languageFileDir):
            os.makedirs(languageFileDir)

        with open(os.path.join(languageFileDir, os.path.basename(file)), "w", encoding='utf8') as outfile:
            json.dump(modelDefinition, outfile, ensure_ascii=False)

print('Finished generating X10A language files!')