#pragma once

// The options page and the "PLAYER 1" page that follow the title picture.
// Both are drawn from data/front.bin, copied out of the menu snapshot.

// Read data/front.bin. False when the file is missing or short.
bool LoadFront();

// Blue options page. twoPlayers is $42C9 (F3). humanOpponent is $42CA (F5).
// Both start clear: 1 PLAYER and COMPUTER are the highlighted words.
void ComposeMenu(bool twoPlayers, bool humanOpponent);

// Blue page from between_rooms. player 0 is PLAYER 1, player 1 is PLAYER 2.
void ComposePlayerUp(int player);
