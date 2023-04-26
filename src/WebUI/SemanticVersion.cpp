#include "SemanticVersion.hpp"

void SemanticVersion::parseVersion()
{
    int8_t indexEnd = version.indexOf('.', index);

    while(index < indexEnd) {
        if(isdigit(version[index])) {
            break;
        }
        index++;
    }

    Major = version.substring(index, indexEnd).toInt();

    index = indexEnd + 1;
    indexEnd = version.indexOf('.', index);
    Minor = version.substring(index, indexEnd).toInt();

    index = indexEnd + 1;
    indexEnd = version.indexOf('-', index);

    if(indexEnd == -1) {
        indexEnd = versionLength - 1;
        hasBuildNr = false;
        hasCommitNr = false;
    }

    Patch = version.substring(index, indexEnd).toInt();

    index = indexEnd + 1;
}

void SemanticVersion::parseBuild()
{
    int8_t indexEnd = version.indexOf('-', index);

    if(indexEnd == -1) {
        indexEnd = versionLength - 1;
        hasCommitNr = false;
    }

    Build = version.substring(index, indexEnd).toInt();

    index = indexEnd + 1;
}

void SemanticVersion::parseExtra()
{
    version.substring(index).toCharArray(Extra, sizeof(Extra));
}

// v1.0.4-14-g2414721
SemanticVersion::SemanticVersion(const String _version) : version(_version), versionLength(_version.length())
{
    if(versionLength == 0 || version[0] != 'v') {
        hasVersionNr = false;
        hasBuildNr = false;
        parseExtra();
        return;
    }

    parseVersion();

    if(!hasBuildNr)
        return;

    parseBuild();

    if(!hasCommitNr)
        return;

    parseExtra();
}