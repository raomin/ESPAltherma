Import("env")

try:
    import rcssmin
    import htmlmin
    from jsmin import jsmin
except ImportError:
    env.Execute("$PYTHONEXE -m pip install rcssmin htmlmin jsmin")

import os
import shutil
import glob
import gzip

filetypes_to_gzip = ['js', 'html', 'css']

print('Generating gzip webui files...')

data_src_dir = os.path.join(env.get('PROJECT_DIR'), 'webui')

minify = env.get('BUILD_TYPE') != 'debug'

files_to_gzip = []
for extension in filetypes_to_gzip:
    files_to_gzip.extend(glob.glob(os.path.join(data_src_dir, '*.' + extension)))

print('  files to gzip: ' + str(files_to_gzip))

for file in files_to_gzip:
    print('  Minifying file: ' + file)
    srcFile = os.path.join(data_src_dir, os.path.basename(file))
    tmpFile = srcFile + ".tmp"
    gzFile = srcFile + ".gz"
    extension = file.split(".")[-1]

    shutil.copyfile(srcFile, tmpFile)

    if extension == "js":
        with open(tmpFile, 'r') as js_file:
            if minify:
                minified = jsmin(js_file.read())
            else:
                minified = js_file.read()

        with open(tmpFile, 'w') as js_file:
            js_file.write(minified)
    elif extension == "html":
        with open(tmpFile,'r') as fileHandler:
            if minify:
                htmlContent = htmlmin.minify(fileHandler.read())
            else:
                htmlContent = fileHandler.read()

        with open(tmpFile,'w') as fileHandler:
            fileHandler.write(htmlContent)
    elif extension == "css":
        with open(tmpFile,'r') as fileHandler:
            if minify:
                minified = rcssmin.cssmin(fileHandler.read())
            else:
                minified = fileHandler.read()

        with open(tmpFile,'w') as fileHandler:
            fileHandler.write(minified)

    print('  GZipping file: ' + file)
    with open(tmpFile, "rb") as src, gzip.open(gzFile, 'wb') as dst:
        dst.writelines(src)

    os.remove(tmpFile)

print('Finished generating gzip webui files!')