#pragma once

// Settings from config.ini, with the command line applied on top.
struct Config {
    bool infiniteLives = false;
    bool enemiesCantHitYou = false;
    // Off keeps the original landing loop. On pays 50 once, and one knockout.
    bool fixBugsInOriginal = false;
    int startRoom = 0;
    bool showTitleScreen = true;
    bool fullScreen = false;
    bool showCrtFilter = false;
};

extern Config gConfig;

// Read config.ini beside the exe. If that file is not there, read config.ini
// one folder up, then two folders up. Then apply Name=Value words from the
// command line.
void LoadConfig(const wchar_t* commandLine);
