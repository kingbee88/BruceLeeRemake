#pragma once

// Settings from config.ini, with the command line applied on top.
// showTitleScreen is the one the game acts on. The others are stored so a
// later change can use the same names.
struct Config {
    bool infiniteLives = false;
    bool enemiesCantHitYou = false;
    int startRoom = 1;
    bool showTitleScreen = true;
    bool fullScreen = false;
    bool showCrtFilter = false;
};

extern Config gConfig;

// Read config.ini beside the exe. If that file is not there, read config.ini
// in the parent folder. Then apply Name=Value words from the command line.
void LoadConfig(const wchar_t* commandLine);
