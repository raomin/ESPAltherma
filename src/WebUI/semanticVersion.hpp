#pragma once

#include "arduinoC.hpp"

class SemanticVersion
{
private:
    uint8_t index = 0;
    const String version;
    const uint8_t versionLength;

    void parseVersion();
    void parseBuild();
    void parseExtra();

public:
    uint8_t Major = 0;
    uint8_t Minor = 0;
    uint16_t Patch = 0;
    uint16_t Build = 0;
    char Extra[16] = "";

    bool hasVersionNr = true;
    bool hasBuildNr = true;
    bool hasCommitNr = true;

    SemanticVersion(const String version);
};