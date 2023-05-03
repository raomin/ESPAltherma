Import("env", "projenv")

import os
import glob

def postBuild(source, target, env):
    print('Cleaning up gzip webui files...')

    data_src_dir = os.path.join(env.get('PROJECT_DIR'), 'webui')
    files_to_delete = []
    files_to_delete.extend(glob.glob(os.path.join(data_src_dir, '*.gz')))

    for file in files_to_delete:
        print('  Delete file: ' + file)
        os.remove(file)

    print('Finished cleaning up gzip webui files...')

env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.elf",
    postBuild
)