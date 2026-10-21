; Title picture and the crack's two questions, from boot01_title.vsf.
;
; This RAM is not in game.asm. The snapshot is sitting in the button wait at
; $C546, with the picture already on screen. By the options page the same
; addresses have been overwritten, and $0400 is screen memory.
;
; $C000-$C4FF is the game's $0400-$08FF, parked up here so the picture can use
; $0400 and $2000. The copy at $C618 moves it back and jumps to $0400, which is
; the stub below. boot in game.asm is the instruction at $C066.
;
; The picture itself is the original title art. The questions after the button
; are the Remember +2 trainer, not Datasoft. N leaves the game bytes alone.
; Y patches them. The room-0 listing in game.asm is the unpatched bytes.

; ---- runs at $0400 after the copy ----

        * = $C000

; Re-entry flag. Nonzero skips the noise fill and the charset copy and enters
; boot at the fourth expand_charset, $04D1.
title_entry:
        SEI                             ; $C000
        CLD                             ; $C001
        LDX #$FF                        ; $C002
        TXS                             ; $C004
        LDA $D011                       ; $C005
        AND #$EF                        ; $C008
        STA $D011                       ; $C00A
        LDA $03FF                       ; $C00D
        BEQ fill_noise                  ; $C010
        JMP $04D1                       ; $C012

; 256 noise bytes at $0200. noise_byte reads them through ($16).
fill_noise:
        LDA #$00                        ; $C015
        STA $EB00                       ; $C017
        STA $EF70                       ; $C01A
        STA $F070                       ; $C01D
        STA $D41B                       ; $C020
        LDA #$FF                        ; $C023
        STA $D40F                       ; $C025
        STA $D40E                       ; $C028
        LDA #$80                        ; $C02B
        STA $D412                       ; $C02D
        LDX #$00                        ; $C030
noise_page:
        LDY #$60                        ; $C032
noise_wait:
        DEY                             ; $C034
        BNE noise_wait                  ; $C035
        LDA $D41B                       ; $C037
        STA $0200,X                     ; $C03A
        INX                             ; $C03D
        BNE noise_page                  ; $C03E
        NOP                             ; $C040
        NOP                             ; $C041
        NOP                             ; $C042

; $01 = $32 banks in the character ROM at $D000. Two 256-byte copies, each
; followed by its inverse $200 bytes higher. That is $40 glyph codes, which
; is why a menu highlight is EOR #$40 and why $4D, the inverse dash, is the
; glyph flip_menu refuses to flip back.
;
; $D100 is ROM characters $20-$3F. They land at font codes $00-$1F, inverse
; at $40-$5F. Space becomes code 0. The dash becomes $0D. Digits land at $10.
; $D000 is ROM characters $00-$1F. They land at font codes $20-$3F, inverse
; at $60-$7F. 'A' is code $21.
copy_score_font:
        LDA #$32                        ; $C043
        STA $01                         ; $C045
        LDX #$00                        ; $C047
copy_font_byte:
        LDA $D000,X                     ; $C049
        STA $8900,X                     ; $C04C
        EOR #$FF                        ; $C04F
        STA $8B00,X                     ; $C051
        LDA $D100,X                     ; $C054
        STA $8800,X                     ; $C057
        EOR #$FF                        ; $C05A
        STA $8A00,X                     ; $C05C
        INX                             ; $C05F
        BNE copy_font_byte              ; $C060
        LDA #$36                        ; $C062
        STA $01                         ; $C064
        ; falls into boot at $0466. In this image that instruction is $C066.

; ---- title picture, still running at $C500 in the snapshot ----

        * = $C500

; Show the picture and wait for either joystick button. The bitmap is already
; at $2000; the copy that covers it with game code happens after the button.
;
; $D011 ends at $3B: bitmap on, 25 rows, yscroll 3, display on. $D016 is $18,
; multicolor, 40 columns, xscroll 0. $D018 is $18: bitmap $2000, matrix $0400.
; $D020 is $0E, light blue, and that is the border outside the picture.
; $D021 is $80, so the background is black.
;
; Every matrix byte is $1E and color RAM is $0C. A pixel pair is two bits:
;   00  background, black
;   01  high nibble of $1E, white
;   10  low nibble of $1E, light blue
;   11  color RAM, grey
; Light blue that touches the outer edge is the same color as $D020, so on
; the Commodore it reads as more border. The top character row is entirely
; that pair.
show_title:
        LDA #$80                        ; $C500
        STA $D021                       ; $C502
        STA $0291                       ; $C505
        JSR $E536                       ; $C508
        LDA #$0B                        ; $C50B
        STA $D011                       ; $C50D
        LDA #$0E                        ; $C510
        STA $D020                       ; $C512
        LDA #$18                        ; $C515
        STA $D016                       ; $C517
        STA $D018                       ; $C51A
        LDX #$00                        ; $C51D
fill_title_cell:
        LDA #$1E                        ; $C51F
        STA $0400,X                     ; $C521
        STA $0500,X                     ; $C524
        STA $0600,X                     ; $C527
        STA $0700,X                     ; $C52A
        LDA #$0C                        ; $C52D
        STA $D800,X                     ; $C52F
        STA $D900,X                     ; $C532
        STA $DA00,X                     ; $C535
        STA $DB00,X                     ; $C538
        INX                             ; $C53B
        BNE fill_title_cell             ; $C53C
        LDA #$3B                        ; $C53E
        STA $D011                       ; $C540
wait_title_button:
        LDA $DC00                       ; $C543
        AND $DC01                       ; $C546
        AND #$10                        ; $C549
        BNE wait_title_button           ; $C54B

; Button down. Screen off, then copy $A000-$BFFF over the bitmap. $01 = $36
; so $A000 is RAM. The destination high byte is the instruction at $C573,
; and the loop stops when that byte has been incremented to $40. What lands
; at $2000 is the game code game.asm lists there.
        LDA $D011                       ; $C54D
        AND #$0F                        ; $C550
        STA $D011                       ; $C552
        LDA #$C8                        ; $C555
        STA $D016                       ; $C557
        LDA #$16                        ; $C55A
        STA $D018                       ; $C55C
        LDA #$00                        ; $C55F
        STA $D020                       ; $C561
        JSR $E536                       ; $C564
        SEI                             ; $C567
        LDA #$36                        ; $C568
        STA $01                         ; $C56A
copy_game_page:
        LDX #$00                        ; $C56C
copy_game_byte:
        LDA $A000,X                     ; $C56E
        STA $2000,X                     ; $C571
        INX                             ; $C574
        BNE copy_game_byte              ; $C575
        INC $C570                       ; $C577
        INC $C573                       ; $C57A
        LDA $C573                       ; $C57D
        CMP #$40                        ; $C580
        BNE copy_game_page              ; $C582
        LDA #$37                        ; $C584
        STA $01                         ; $C586
        CLI                             ; $C588

; "Unlimited lives??" N skips the three stores. Y writes $A5 over DEC $28 at
; $1826 and over INC $28 at $2FE5 and $3155, so each of those becomes LDA $28.
; The count stays on the stock clear_work_ram stored, 4 or 9. It does not
; rise for a score mark or a room 16 lantern, and a fall does not lower it.
; spend_fall still reaches between_lives. BMI does not see a negative byte,
; so game_over does not run.
        JSR ask_trainer                 ; $C589
        CMP #$4E                        ; $C58C
        BEQ ask_hits                    ; $C58E
        LDA #$A5                        ; $C590
        STA $1826                       ; $C592
        STA $2FE5                       ; $C595
        STA $3155                       ; $C598

; "Opponents can't hit you??" N skips. Y writes $09 over the branch offsets
; at $29E2 and $29F7. Each branch then lands past the ORA into $B3 and on
; the ORA into $B4 or $B5, so Bruce's hit byte is missed and the Ninja's
; and Yamo's bytes are still updated. Their attack poses are chosen before
; this register is read, so they still swing. Bruce never records the contact.
ask_hits:
        JSR ask_trainer                 ; $C59B
        CMP #$4E                        ; $C59E
        BEQ trainer_done                ; $C5A0
        LDA #$09                        ; $C5A2
        STA $29E2                       ; $C5A4
        STA $29F7                       ; $C5A7

; Zero page, then the editor kernal calls, then a blue screen while the parked
; game is copied back to $0400. The source high byte is the instruction at
; $C61C and the destination high byte is $C61F. The copy stops when the
; destination has reached $09, so $C000-$C4FF lands on $0400-$08FF.
trainer_done:
        SEI                             ; $C5AA
        CLD                             ; $C5AB
        LDX #$FF                        ; $C5AC
        TXS                             ; $C5AE
        INX                             ; $C5AF
        TXA                             ; $C5B0
clear_zp:
        STA $00,X                       ; $C5B1
        INX                             ; $C5B3
        BNE clear_zp                    ; $C5B4
        INX                             ; $C5B6
        STX $CC                         ; $C5B7
        LDA #$2F                        ; $C5B9
        STA $00                         ; $C5BB
        LDA #$36                        ; $C5BD
        STA $01                         ; $C5BF
        JSR $E536                       ; $C5C1
        JSR $E3BF                       ; $C5C4
        LDX #$00                        ; $C5C7
        LDA #$06                        ; $C5C9
        STA $D020                       ; $C5CB
        STA $D021                       ; $C5CE
paint_blue:
        STA $D800,X                     ; $C5D1
        STA $D900,X                     ; $C5D4
        STA $DA00,X                     ; $C5D7
        STA $DB00,X                     ; $C5DA
        INX                             ; $C5DD
        BNE paint_blue                  ; $C5DE
        LDA $D011                       ; $C5E0
        AND #$0F                        ; $C5E3
        STA $D011                       ; $C5E5
        LDA #$32                        ; $C5E8
        STA $02                         ; $C5EA
        LDA #$40                        ; $C5EC
        STA $0301                       ; $C5EE
        LDA #$03                        ; $C5F0
        STA $0319                       ; $C5F2
        LDA #$01                        ; $C5F4
        STA $0313                       ; $C5F6
        STA $0318                       ; $C5F8
        STA $BA                         ; $C5FE
        LDA #$00                        ; $C600
        STA $0302                       ; $C602
        STA $BB                         ; $C605
        LDA #$04                        ; $C607
        STA $0303                       ; $C609
        CLI                             ; $C60C
        LDX #$00                        ; $C60D
        TXA                             ; $C60F
clear_0334:
        STA $0334,X                     ; $C610
        INX                             ; $C613
        CPX #$CC                        ; $C614
        BNE clear_0334                  ; $C616
move_game_down:
        LDX #$00                        ; $C618
move_game_byte:
        LDA $C000,X                     ; $C61A
        STA $0400,X                     ; $C61D
        INX                             ; $C620
        BNE move_game_byte              ; $C621
        INC $C61C                       ; $C623
        INC $C61F                       ; $C626
        LDA $C61F                       ; $C629
        CMP #$09                        ; $C62C
        BNE move_game_down              ; $C62E
        JMP $0400                       ; $C630

; Print the 32 bytes at the patched address, in light green, from column 4.
; Then point the next call at $C679, turn the screen on, and wait for Y or N.
; The first call still reads $C659: "Unlimited lives??"
; The second reads $C679: "Opponents can't hit you??"
ask_trainer:
        LDX #$1F                        ; $C633
ask_char:
        LDA #$0D                        ; $C635
        STA $D804,X                     ; $C637
        LDA $C659,X                     ; $C63A
        STA $0404,X                     ; $C63D
        DEX                             ; $C640
        BPL ask_char                    ; $C641
        LDA #$79                        ; $C643
        STA $C63B                       ; $C645
        LDA #$1B                        ; $C648
        STA $D011                       ; $C64A
ask_key:
        JSR $FFE4                       ; $C64D  GETIN
        CMP #$4E                        ; $C650  N
        BEQ ask_done                    ; $C652
        CMP #$59                        ; $C654  Y
        BNE ask_key                     ; $C656
ask_done:
        RTS                             ; $C658

; $C659  32 bytes, "       Unlimited lives??"
; $C679  32 bytes, "       Opponents can't hit you??"
; Screen codes are the lowercase ROM set: $41 is 'A', $01 is 'a'.
