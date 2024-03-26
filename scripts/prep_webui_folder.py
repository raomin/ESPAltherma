Import("env")

try:
    import rcssmin
    import htmlmin
    from jsmin import jsmin
    import step
except ImportError:
    env.Execute("$PYTHONEXE -m pip install rcssmin htmlmin jsmin step-template")
finally:
    import rcssmin
    import htmlmin
    from jsmin import jsmin
    import step

import os
import shutil
import glob
import gzip

cppDefines = env.ParseFlags(env['BUILD_FLAGS']).get('CPPDEFINES')
namespace = {'platform': env['PIOPLATFORM']}

for idx, define,  in enumerate( cppDefines):
    namespace[define] = idx

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
            parsedJSFile = step.Template(js_file.read()).expand(namespace)

            if minify:
                minified = jsmin(parsedJSFile)
            else:
                minified = parsedJSFile

        with open(tmpFile, 'w') as js_file:
            js_file.write(minified)
    elif extension == "html":
        with open(tmpFile,'r') as fileHandler:

            parsedHTMLFile = step.Template(fileHandler.read()).expand(namespace)

            if minify:
                htmlContent = htmlmin.minify(parsedHTMLFile)
            else:
                htmlContent = parsedHTMLFile

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