; Rooms. The running game has twenty, and $29 holds the index, 0 through $13.
; game.asm is the code that walks these tables. This file is the tables
; and the notes that came from reading them.

; ---------------------------------------------------------------------------
; Charset expand. boot in game.asm fills eight bytes, then expands the
; charsets, and falls into start at $050F. Each expand call
; sets $F3 to $7F, so 128 glyphs. expand_charset at $4892 doubles four source
; bytes into ($06) and the next four into ($08). The two destinations are the
; two halves of one 2048-byte charset: glyphs 0-127, then glyphs 128-255.
;
;   source $9000 -> $A000 and $A400
;   source $9400 -> $A800 and $AC00
;   source $9800 -> $B000 and $B400
;   source $9C00 -> $B800 and $BC00
;
; The fourth call does TAX/TXS with A still $B8, so the destination is $B800
; and the stack pointer is left at $B8 until start sets it to $FD. $B800 is
; never stored in $D018. In the snapshot, $9000 is no longer the packed
; source: $9060 is used as a mask buffer, so the packed bytes cannot be
; expanded again. The live charsets are what the rooms draw.


; enter_room picks the charset from $29. $D018 = $38 for rooms 0-9, which is
; $A000. $D018 = $3C for room $12, which is $B000. $D018 = $3A for the rest,
; rooms 10-17 and 19, which is $A800.
;
; flicker_lamps always writes fixed addresses. On $A000 it rewrites glyphs
; $93, $86, $B1, $94, and $E7. On $A800 it writes the mask alone into glyphs
; $93, $B1, and $94, and leaves $86 and $E7 alone. $B000 is never written, so
; room 18's lamps do not flicker. The three counters still run.

; ---------------------------------------------------------------------------
; Picture. $4B34,X and $4B49,X are the low and high bytes of the packed
; stream for room X. The streams sit one after another. Room 0 starts at
; $68F9. The command bytes are described at L_0AD7 in game.asm.
;
; Rooms 0-3, 5, 6, 8, 9, 11, 12, 14-16, and 19 emit 440 cells, which is the
; playfield: character rows 3-24, eleven tile rows of two character rows.
; Rooms 4, 7, 10, 13, 17, and 18 emit 442. The extra two are written past
; the 1000-byte screen and are not part of the picture.
;
; Before the stream, $47A0,X is written through $D878. The high nibble is $F
; and the low nibble is the color. Color RAM keeps the low nibble, so $FE is
; ink $0E. A tile with bit 7 set paints $F8, which is ink $08, on both rows
; of its pair.

; $47A0. One fill byte per room.
; FE FE FE FE FC FC FC FF FA FA F9 FB F9 FA F9 FC F9 FF FE FF

; Low and high of the packed stream, rooms 0-19.
; $4B34: F9 09 3E 9D 1B 82 DD 27 11 91 D8 4C 43 C6 46 B4 5C 8D 90 E0
; $4B49: 68 6A 6B 6C 6E 6F 70 72 73 74 75 77 78 79 7A 7B 7C 7D 7E 7F

; ---------------------------------------------------------------------------
; Raster colors. $480B, room*2 points at twelve bytes: four bands of $D021,
; $D022, $D023. The high nibble is $F and the VIC uses the low nibble.
; $4833, room*2 points at the split list. raster_irq copies the first four
; bytes to $47F5. $FD ends the chain, so a list that starts with $FD is one
; band. Rooms 0-3 share the color list at $47C8. Rooms 4-6 share $47D1,
; rooms 7-9 share $47D4, and rooms 10, 12, 14, and 16 share $47D7. The split
; list at $4805 starts with $FD and is shared by rooms 4-9, 11, 13, and
; 17-19. Rooms 10, 12, 14, and 16 share the split list at $4806. The color
; lists for rooms 17-19 sit on top of those split bytes. The interrupt still
; copies twelve, and those rooms' split list begins with $FD, so only the
; first band is shown.
;
; enter_room also copies 20 bytes from the pointer at $4AA8, room*2, into
; $0153. raster_irq overwrites the first twelve of those every frame from the
; $480B list, so the picture colors are the $480B list. The score bar is
; separate: above the picture, $D021 is $FC whatever the room is.
;
; Bands below are the low nibbles. Splits are the four bytes, and the byte
; after $FD is unused.

; Room 0.  Bands 0B 01 0E, 0B 02 01, 0C 02 01, 06 0E 01. Splits 78 97 FD 78.
; Room 1.  Same bands and splits as room 0.
; Room 2.  Same bands. Splits 78 97 FD 62.
; Room 3.  Same bands. Splits 62 97 FD FD.
; Room 4.  Bands 06 0E 01, 08 09 01, 06 0E 0B, 0C 0E 0B. Splits FD 56 FD B6.
; Room 5.  Same as room 4.
; Room 6.  Same as room 4.
; Room 7.  Bands 08 09 01, 06 0E 0B, 0C 0E 0B, 04 0E 0B. Splits FD.
; Room 8.  Same as room 7.
; Room 9.  Same as room 7.
; Room 10. Bands 06 0E 0B, 0C 0E 0B, 04 0E 0B, 0E 06 01. Splits 56 FD B6 C7.
; Room 11. Bands 04 0E 0B, 0E 06 01, 0E 06 0B, 0C 0E 01. Splits FD.
; Room 12. Same bands and splits as room 10.
; Room 13. Bands 0E 06 01, 0E 06 0B, 0C 0E 01, 0F 08 0C. Splits FD.
; Room 14. Same bands and splits as room 10.
; Room 15. Bands 0E 06 0B, 0C 0E 01, 0F 08 0C, 0C 08 0B. Splits B6 C7 FD C8.
; Room 16. Same bands and splits as room 10.
; Room 17. Bands 0C 08 0B, 00 0E 04, 06 02 01, 08 07 0D. Splits FD.
; Room 18. Bands 00 0E 04, 06 02 01, 08 07 0D, 08 08 07. Splits FD.
; Room 19. Bands 06 02 01, 08 07 0D, 08 08 07, 0D 08 07. Splits FD.

; ---------------------------------------------------------------------------
; Where Bruce appears. Cold start calls wake_actor with X = 0 and A = 0,
; and $0104 clear, so xpos and ypos come from $4BAE,X and $4BC2,X. The
; screen position is xpos*2-24, ypos-21. Facing starts at 1. An exit writes
; the slot's xpos and ypos and then calls enter_room, which does not call
; wake_actor, so the facing he had is kept and the position is the slot,
; not this spawn.
;
; $4BAE xpos, $4BC2 ypos, rooms 0-19:
; 1A 18 30 20 94 22 86 1C 14 12 34 40 18 98 18 1C 18 18 18 12
; C4 62 C4 C0 B4 51 B3 44 46 C6 8C 4C 5C 32 5C C3 5C B4 C6 95

; ---------------------------------------------------------------------------
; Exits. match_special tests tiles 2, 3, 4, and 5 before the bit-1 gate.
; Tile 2 reads $4B5E (room), $4BFE (xpos), $4C12 (ypos). Tile 3 reads $4B72,
; $4C26, $4C3A. Tile 4 reads $4B86, $4C4E, $4C62. Tile 5 reads $4B9A, $4C76,
; $4C8A. Tiles 2, 3, and 4 fall through to the next slot when the xpos byte
; is 0. Tile 5 does not test xpos. Every real arrival has a nonzero xpos, and
; a slot of 00 00 00 is unused. Room 18 draws tiles 4 and 5 as part of the
; boss while those two slots are 00 00 00. Taking them would send him to room
; 0 at sprite 0, 0. Room 19 has no door tiles and every slot is zero.
;
; L_30D4 allows the exit when the room is $0B, or when he is leaving through
; the edge he stands on. The four step counters are described there. Room $0D
; adds 4 to ypos in cell_of_actor before the row is chosen. That changes which
; row is scanned. It does not change the edge test or the overlap.
;
; Each line is room, xpos, ypos for tiles 2, 3, 4, 5. An xpos of 00 is unused.
;
;  0: 01 10 92, 00 00 00, 00 00 00, 01 0F 62
;  1: 00 A0 92, 05 2C 32, 00 A1 62, 02 10 62
;  2: 00 00 00, 00 00 00, 01 A1 62, 03 10 C4
;  3: 00 00 00, 0A 10 B0, 02 A1 C5, 00 10 62
;  4: 05 10 C2, 07 1F 32, 00 00 00, 00 00 00
;  5: 04 9C 51, 06 10 40, 06 10 74, 01 58 66
;  6: 05 A0 80, 05 A0 66, 02 16 51, 00 20 51
;  7: 00 30 51, 08 0F 46, 08 10 C0, 00 20 51
;  8: 07 A0 74, 09 10 53, 07 A1 B5, 09 0F C6
;  9: 08 A0 47, 08 A0 C0, 06 9D C7, 00 00 00
; 10: 03 A0 C4, 0B 10 C4, 00 00 00, 00 00 00
; 11: 0A A0 5A, 0C 10 5C, 0E 10 5E, 10 10 5C
; 12: 00 00 00, 0B 34 BE, 0E 10 BE, 00 00 00
; 13: 0F 10 C3, 00 00 00, 00 00 00, 00 00 00
; 14: 00 00 00, 0B 50 BE, 0C A0 BE, 10 10 BE
; 15: 0D A0 C4, 11 10 B3, 00 00 00, 00 00 00
; 16: 00 00 00, 0B 80 BE, 0E A0 BE, 00 00 00
; 17: 0F A0 93, 12 10 C6, 00 00 00, 00 00 00
; 18: 11 A0 44, 13 10 96, 00 00 00, 00 00 00
; 19: 00 00 00, 00 00 00, 00 00 00, 00 00 00

; ---------------------------------------------------------------------------
; Lamps. $4CE2, room*2 points at the records. A record is the flag, the
; replacement tile with bit 7 set, the column, and the tile row. The high bit
; of the flag ends the list. $4CCE is the remaining count copied to $2A. It
; counts lamps still to take, so rooms 13 and 19 are 00 even though each list
; has a dummy record at column 0, row 0, tile 0. A taken flag stays 0, and
; coming back into the room paints the mount again.
;
; $4CCE: 06 06 0A 02 02 0A 08 0D 04 05 03 03 03 00 04 06 02 02 01 00
;
; Column, tile row, mount. The mount is the replacement with bit 7 off.
;
;  0: 31,7,$1F  38,7,$12  2,10,$1F  25,10,$1F  29,10,$12  38,10,$12
;  1: 9,7,$1E  30,7,$1F  6,10,$12  15,10,$1E  24,10,$1F  32,10,$12
;  2: 5,7,$12  9,7,$1E  22,7,$1F  26,7,$1E  30,7,$1F
;     5,10,$1E  9,10,$1F  15,10,$1E  21,10,$1F  27,10,$1E
;  3: 29,10,$1E  38,10,$1F
;  4: 11,4,$1E  28,4,$1F
;  5: 2,3,$1E  6,3,$1E  28,3,$1F  32,3,$1E  38,3,$1F
;     30,6,$1F  2,10,$1E  23,10,$1F  26,10,$1F  30,10,$1E
;  6: 8,5,$1F  10,5,$1E  14,5,$1F  22,5,$1F  26,5,$1F  28,5,$1E
;     6,9,$1F  30,9,$1E
;  7: 3,2,$1E  12,2,$1F  13,2,$1E  25,2,$1F  26,2,$1E
;     5,5,$1F  6,5,$1E  12,5,$1F  13,5,$1E  25,5,$1F  26,5,$1E
;     2,9,$1F  3,9,$1E
;  8: 15,3,$1F  24,3,$1F  6,7,$1E  33,7,$1F
;  9: 32,2,$1F  15,7,$1E  27,10,$1E  28,10,$1F  32,10,$1E
; 10: 1,4,$1F  30,7,$1F  30,10,$1F
; 11: 18,3,$1E  21,3,$1F  7,10,$1F
; 12: 36,4,$1E  36,7,$1E  36,10,$1E
; 13: dummy at 0,0
; 14: 9,7,$1F  30,7,$1E  9,10,$1F  30,10,$1E
; 15: 19,3,$1F  21,3,$1E  14,4,$1E  26,4,$1F  13,6,$1E  27,6,$1F
; 16: 32,4,$1E  5,7,$1F
; 17: 38,10,$1E  31,11,$1E
; 18: 37,11,$1E
; 19: dummy at 0,0

; $4E9C, room*2 runs once from L_336E, after the taken lamps are painted.
; The word is the address minus one. It clears $4E97-$4E9B first. The
; enter hooks are hook_enter_0 through hook_enter_19 in game.asm. Room 1's
; hook is hook_enter_1 and stores 3 in $4E97.
;
; $4EC4, room*2 is called every logic frame from L_3A6F, after $7B has
; flipped. The per-frame hooks are hook_frame_0 through hook_frame_19 in
; game.asm. Room 1's hook is hook_frame_1. It opens the trap door in the floor.
; $2A, $2B, and $2C have to be zero, which is every lamp in rooms 0, 1, and
; 2 taken. $30 is room 6's count and has to be nonzero, so the door stays
; shut once room 6 is clear. $4E97 then counts 3, 2, 1, 0, $FF. Each step
; prints five tiles at column $11 of tile row $0C. The first step, when the
; count becomes 2, calls play_sound_8a, which plays sound $8A and keeps A.

;
; print_string subtracts $20 from each byte and stops on $A0. The four
; strings, and the tiles they become, are:
;
;   $12 at $45AA: 31 7C 7D 7E 7F A0    tiles 11 5C 5D 5E 5F
;   $11 at $45B0: 31 42 42 A3 A3 A0    tiles 11 22 22 03 03
;   $10 at $45B6: 7C 7D 7E A3 A3 A0    tiles 5C 5D 5E 03 03
;   $0F at $45BC: 42 42 A3 A3 A3 A0    tiles 22 22 03 03 03
;
; Tile $03 is the exit in slot 1, so the open hole leads to room 5. The
; packed stream is still the closed floor: columns $11-$15 of that row are
; tiles 11 11 22 22 11 until this hook paints over them. Coming back into
; the room decompresses that floor and arms $4E97 with 3 again.

; Room 5's enter hook is hook_enter_5 in game.asm. It repeats until the
; byte at $02 is $32, writes color $FC across $D89A-$D89C and $D8C2-$D8C4,
; and reloads the three beam columns, rows, and timers. $74, $75, and $76
; are tile rows 9, $0A, and 5. A timer is a byte, so $FF, $FC, and $F8 are
; the opening pauses.


; Room 5's per-frame hook is hook_frame_5 in game.asm. It does not read $7B,
; so the beams move on every logic frame. These are energy beams.
;
; $71/$77 slides the beam under the middle gap on tile row $74. $72/$78
; slides the beam on tile row $75, the next row down. Both run from column
; $1B to column $1C. $73/$79 slides the beam at the left of the upper level,
; tile row $76, from column $01 to column $02. Count 3 erases with
; accumulator 6 and, while the column is still left of the limit, steps one
; column right and draws accumulator 2. Count 7 reloads the column and the
; pause. $E0 is -32 and $E2 is -30.
;
; print_row74 and print_row75 add $1F to the frame and print it. The two gap
; beams therefore use strings $1F-$25:
;   56 / 55 / 55 56 / 54 55 / 00 55 / 00 54 / 00 00
; print_row76_later adds 7 before that, so the upper beam uses strings
; $26-$2C:
;   5A / 59 / 59 5A / 58 59 / 00 59 / 00 58 / 00 00
;
; The second beam runs only when $4D99 is nonzero. The opening snapshot holds
; 1 there, and nothing from $0500 to $4FFF stores $4D99.
;
; $38AB-$3914 are one-shot blocks on the counters at $4E99, $4E9A, and
; $4E9B. Each waits while its $4Dxx flag is still clear. play_sound_8a,
; called from L_3A8A, plays the sound and keeps A.

; ---------------------------------------------------------------------------
; Scrolling surface. scroll_surface in game.asm rewrites glyphs $61, $62,
; and $63, and the shadow glyphs $E1, $E2, and $E3. hook_frame_4,
; hook_frame_5, hook_frame_6, hook_frame_8, hook_frame_9, and hook_frame_11
; jump to it. Rooms 0-9 share charset $A000, so a step written in one of
; those rooms is what the next of them shows. Room 11 uses charset $A800
; and the pattern at $4309. The other pattern, at $42F1, is the one rooms
; below 10 read.
;
; Room 4's ladder is the vertical run at character columns 18-21. The
; picture row is $61, $62, $62, $63. The shadow row is those tiles with
; bit 7 set. Rooms 5 and 6 draw the same glyphs in horizontal runs, and
; rooms 8, 9, and 11 draw them too. Room 18 contains the tile numbers,
; but its charset is $B000 and this routine never writes that bank. Room
; 19 contains them on charset $A800 and does not call the routine, so
; they stay as expand_charset left them until room 11 writes that bank.
;
; enter_room stores 0 in $65 and 7 in $4321, so the picture starts upward
; and the delay slot starts at the last byte of $4CB2. $64 is not cleared.
; hook_enter_4 does not touch either byte. The direction reverses only
; when the slot wraps, unless a hook stores $65 itself. Room 4 does that,
; upward, when lamp $4D74 is taken.

