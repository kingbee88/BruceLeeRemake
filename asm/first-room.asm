; Bruce Lee, the code that is running in the first room.
;
; Bruce Lee.d64 holds one program, "BRUCE LEE+2 /REM", packed so that the
; only machine code stored in the clear is the loader in disk-boot.asm.
; This file is the game with the first room on screen, from the memory
; image bruceleestart.vsf. These are the routines the Windows port follows,
; at the same addresses. The packed file is not a plain copy of this listing.
;
; X selects the actor. X = 0 is Bruce. The same fields exist for the other
; two fighters.
;
;   xpos,X    $9E   horizontal position. One step here is two screen pixels.
;   ypos,X    $A1   vertical position. One step is one screen pixel. Up decrements it.
;   facing,X  $A4   0 left, 1 right.
;   hit,X     $B6   result of the overlap test. flags_from_nibbles rolls it in.
;                   bit 0: ground. Nibbles 1, 4, 5, 6, 7, 9, and 13.
;                   bit 1: prize ink. Nibbles 2, 6, 8, 9, and 10. The lamp hanger is nibble 2.
;                   bit 2: ladder, which also holds him. Nibbles 3, 7, and 11-15
;                          when the cell property is not 0.
;                   bit 3: wall. Those same nibbles when the property is 0.
;                          push_out walks the step back out.
;   state,X   $BF   which scripted move is running. 0 means none.
;   timer,X   $CE   counts down through that script.
;   active,X  $95   bit 0 set: this actor is in the room.
;   $0106,X          how many frames to hold the picture chosen for the state.
;   $98,X            the move he wants. $0E up, $0D down, $0B left, $07 right, $0F none.
;   $9B              fire. 0 means the button is held. irq_service stores it.
;
; States used by Bruce:
;   1 and 2      climb up, climb down
;   5            standing jump
;   6 and 7      sideways jump, left and right
;   $0C and $0D  flying kick, left and right
;   $0E          fall
;   $11          punch
;   $1A          duck
;
; Where to start reading
;   start             cold start
;   read_stick        joystick and keyboard
;   actor_tick        one actor for this frame
;   on_ground         the fall stops when hit bits 0 or 2 are set
;   push_out          bit 3 pushes him back out of a wall
;   move_for_state    the jump, the kick, the sideways jump
;   move_up           one pixel up
;   move_down         one pixel down
;   move_left         one step left
;   move_right        one step right
;   pose_jump         which picture the standing jump shows
;   pose_kick_left    which picture the flying kick shows
;   scan_row          the overlap test for one row of his sprite
;   flags_from_nibbles  nibbles to the hit bits
;   scan_specials     tiles under him, a 3 by 2 window
;   take_lamp         replace a lamp and add 125
;   flicker_lamps     rewrite the lamp glyph rows
;   raster_irq        the in-game color bands
;   irq_service       count $48 down and read the stick
;
; More names
;   frame             one frame, then wait for the raster count
;   init_hardware     VIC bank, colors, row pointers
;   enter_room        load room $29
;   title_loop        the title screen keys
;   step_actors       hold, then choose a picture, for each actor
;   choose_pose       state and facing to a library number
;   blit_sprites      copy those pictures into the sprite slots
;   release_hold      the $0106,X pause between pose changes
;   show_screen       turn the VIC display back on
;   silence_sid       clear the SID and the three effect slots

; Cold start. The screen is turned off until the room is ready.
; The charset expand at $047A runs before this. It is listed in rooms.asm,
; along with the per-room pointers this file's enter_room walks.
start:
        SEI                             ; $050F
        CLD                             ; $0510
        LDX #$FD                        ; $0511
        TXS                             ; $0513
        LDA $D011                       ; $0514
        AND #$EF                        ; $0517
        STA $D011                       ; $0519
        JSR reset_music                      ; $051C
        LDA #$00                        ; $051F
        STA $16                         ; $0521
        LDA #$02                        ; $0523
        STA $17                         ; $0525
        SEI                             ; $0527
        JSR clear_work_ram                      ; $0528
        JSR init_hardware                      ; $052B
        JMP open_room                   ; $052E
reset_actors:
        SEI                             ; $0531
        LDA $D011                       ; $0532
        AND #$EF                        ; $0535
        STA $D011                       ; $0537
        LDA #$00                        ; $053A
        STA $D015                       ; $053C
        STA $D000                       ; $053F
        STA $D001                       ; $0542
        LDA #$01                        ; $0545
        STA $E30A                       ; $0547
        JSR reset_music                      ; $054A
        LDX #$FD                        ; $054D
        TXS                             ; $054F
        JSR clear_work_ram                      ; $0550
        JSR init_hardware                      ; $0553
        LSR $0104                       ; $0556
        LDA $42C9                       ; $0559
        STA $42CB                       ; $055C
        LDA $42CA                       ; $055F
        STA $42CC                       ; $0562
        LDA #$00                        ; $0565
        STA $42CA                       ; $0567
        STA $42C9                       ; $056A
        JSR enter_room                      ; $056D
        LDA #$00                        ; $0570
        STA $1D                         ; $0572
        LDA #$F3                        ; $0574
        STA $1E                         ; $0576
        JSR show_screen                      ; $0578
        NOP                             ; $057B
        NOP                             ; $057C
        NOP                             ; $057D
        NOP                             ; $057E
        NOP                             ; $057F
        CLI                             ; $0580
        JMP frame                      ; $0581
open_room:
        SEI                             ; $0584
        LDA $D011                       ; $0585
        AND #$EF                        ; $0588
        STA $D011                       ; $058A
        JSR silence_sid                      ; $058D
        LDA #$00                        ; $0590
        STA $0100                       ; $0592
        JSR clear_playfield                      ; $0595
        JSR L_0E2E                      ; $0598
        LDX #$3F                        ; $059B
        LDA #$00                        ; $059D
L_059F:
        STA $83C0,X                     ; $059F
        DEX                             ; $05A2
        BPL L_059F                      ; $05A3
        LDA #$01                        ; $05A5
        STA $D015                       ; $05A7
        STA $D01D                       ; $05AA
        STA $D010                       ; $05AD
        LDA #$00                        ; $05B0
        STA $D017                       ; $05B2
        STA $D01B                       ; $05B5
        STA $D01C                       ; $05B8
        LDA #$0A                        ; $05BB
        STA $D000                       ; $05BD
        LDA #$56                        ; $05C0
        STA $D001                       ; $05C2
        LDA #$FF                        ; $05C5
        STA $D027                       ; $05C7
        LDA #$0F                        ; $05CA
        STA $8FF8                       ; $05CC
        LDX #$0B                        ; $05CF
L_05D1:
        LDA $0E40,X                     ; $05D1
        STA $83CC,X                     ; $05D4
        DEX                             ; $05D7
        BPL L_05D1                      ; $05D8
        LDA #$2C                        ; $05DA
        STA $0408                       ; $05DC
        LDA #$15                        ; $05DF
        STA $0409                       ; $05E1
        LDA #$01                        ; $05E4
        STA $D01A                       ; $05E6
        STA $D019                       ; $05E9
        LDA $D011                       ; $05EC
        AND #$7F                        ; $05EF
        STA $D011                       ; $05F1
        LDA #$FD                        ; $05F4
        STA $D012                       ; $05F6
        LDA #$F6                        ; $05F9
        STA $D020                       ; $05FB
        STA $D021                       ; $05FE
        LDA #$FF                        ; $0601
        STA $D022                       ; $0603
        STA $D023                       ; $0606
        LDX #$08                        ; $0609
        STX $D016                       ; $060B
        JSR fill_color_ram                      ; $060E
        LDX #$00                        ; $0611
L_0613:
        LDA #$F1                        ; $0613
        STA $D9E0,X                     ; $0615
        STA $DAE0,X                     ; $0618
        LDA #$DC                        ; $061B
        STA $8C00,X                     ; $061D
        STA $8C68,X                     ; $0620
        DEX                             ; $0623
        BNE L_0613                      ; $0624
        LDX #$27                        ; $0626
L_0628:
        LDA $4A0D,X                     ; $0628
        ORA #$80                        ; $062B
        STA $8C50,X                     ; $062D
        LDA $49BD,X                     ; $0630
        ORA #$80                        ; $0633
        STA $8CC8,X                     ; $0635
        LDA $49E5,X                     ; $0638
        ORA #$80                        ; $063B
        STA $8CF0,X                     ; $063D
        LDA $4A35,X                     ; $0640
        ORA #$80                        ; $0643
        STA $8D68,X                     ; $0645
        DEX                             ; $0648
        BPL L_0628                      ; $0649
        LDA #$FF                        ; $064B
        STA $0100                       ; $064D
        LDA #$4F                        ; $0650
        LDX #$06                        ; $0652
        LDY #$31                        ; $0654
        JSR print_string                      ; $0656
        LDA #$50                        ; $0659
        LDX #$07                        ; $065B
        LDY #$2B                        ; $065D
        JSR print_string                      ; $065F
        LDA #$51                        ; $0662
        LDX #$08                        ; $0664
        LDY #$31                        ; $0666
        JSR print_string                      ; $0668
        LDX #$09                        ; $066B
L_066D:
        LDA $8DF0,X                     ; $066D
        CLC                             ; $0670
        ADC #$40                        ; $0671
        STA $8DF0,X                     ; $0673
        LDA $8E40,X                     ; $0676
        CLC                             ; $0679
        ADC #$40                        ; $067A
        STA $8E40,X                     ; $067C
        CPX #$02                        ; $067F
        BCS L_069E                      ; $0681
        LDA $8DEB,X                     ; $0683
        CLC                             ; $0686
        ADC #$40                        ; $0687
        STA $8DEB,X                     ; $0689
        LDA $8E3B,X                     ; $068C
        CLC                             ; $068F
        ADC #$40                        ; $0690
        STA $8E3B,X                     ; $0692
        LDA $8E8B,X                     ; $0695
        CLC                             ; $0698
        ADC #$40                        ; $0699
        STA $8E8B,X                     ; $069B
L_069E:
        DEX                             ; $069E
        BPL L_066D                      ; $069F
        LDA $42CB                       ; $06A1
        STA $42C9                       ; $06A4
        LDA $42CC                       ; $06A7
        STA $42CA                       ; $06AA
        LDA $42C9                       ; $06AD
        BEQ L_06B5                      ; $06B0
        JSR flip_menu_a                      ; $06B2
L_06B5:
        LDA $42CA                       ; $06B5
        BEQ L_06BD                      ; $06B8
        JSR flip_menu_b                      ; $06BA
L_06BD:
        LDA $1385                       ; $06BD
        BNE L_06DD                      ; $06C0
        DEC $09BB                       ; $06C2
        BPL L_06DD                      ; $06C5
        LDA #$04                        ; $06C7
        STA $09BB                       ; $06C9
        LDA #$03                        ; $06CC
        STA $E314                       ; $06CE
        LDX #$00                        ; $06D1
        STX $E870                       ; $06D3
        DEX                             ; $06D6
        STX $E871                       ; $06D7
        JSR start_tune                      ; $06DA
L_06DD:
        LDX #$00                        ; $06DD
        LDA $CF8F,X                     ; $06DF
        EOR $CFB1,X                     ; $06E2
        EOR $CF90,X                     ; $06E5
        EOR $CFAF,X                     ; $06E8
        EOR $CF91,X                     ; $06EB
        EOR $CFB0,X                     ; $06EE
        BEQ L_06FA                      ; $06F1
; The check of the bytes around $CF8F failed. Stay here and walk ($1D) forward.
checksum_hang:
        STA ($1D),Y                     ; $06F3
        INC $1E                         ; $06F5
        JMP checksum_hang                      ; $06F7
L_06FA:
        LDA #$09                        ; $06FA
        STA $4A                         ; $06FC
        LDA #$60                        ; $06FE
        STA $49                         ; $0700
        JSR show_screen                      ; $0702
        NOP                             ; $0705
        NOP                             ; $0706
        NOP                             ; $0707
        NOP                             ; $0708
        NOP                             ; $0709
        CLI                             ; $070A
        JSR title_loop                      ; $070B
        SEI                             ; $070E
        LDA $D011                       ; $070F
        AND #$EF                        ; $0712
        STA $D011                       ; $0714
        LDA #$00                        ; $0717
        STA $D015                       ; $0719
        STA $D000                       ; $071C
        STA $D001                       ; $071F
        STA $03                         ; $0722
        LDA #$01                        ; $0724
        STA $E30A                       ; $0726
        JSR reset_music                      ; $0729
        LDA $42C9                       ; $072C
        STA $42CB                       ; $072F
        LDA $42CA                       ; $0732
        STA $42CC                       ; $0735
        JSR between_rooms                      ; $0738
        LDA $D011                       ; $073B
        AND #$EF                        ; $073E
        STA $D011                       ; $0740
        LDX #$FD                        ; $0743
        TXS                             ; $0745
        JSR clear_work_ram                      ; $0746
        JSR init_hardware                      ; $0749
        JSR enter_room                      ; $074C
        LDA #$00                        ; $074F
        STA $1D                         ; $0751
        LDA #$F3                        ; $0753
        STA $1E                         ; $0755
        JSR show_screen                      ; $0757
        NOP                             ; $075A
        NOP                             ; $075B
        NOP                             ; $075C
        NOP                             ; $075D
        NOP                             ; $075E
        CLI                             ; $075F
        JMP frame                      ; $0760
; VIC bank $8000, multicolor on, sprites expanded in X. Then the row pointers, the score
; row, and the room tables.
init_hardware:
        LDA #$00                        ; $0763
        STA $8B                         ; $0765
        STA $8C                         ; $0767
        JSR arm_raster                      ; $0769
        JSR init_cia                    ; $076C
        LDA #$FF                        ; $076F
        STA $4B                         ; $0771
        LDY #$8B                        ; $0773
        LDA #$D8                        ; $0775
        JSR build_row_pointers                      ; $0777
        LDA #$00                        ; $077A
        STA $4338                       ; $077C
        STA $4360                       ; $077F
        STA $4361                       ; $0782
        LDA #$8C                        ; $0785
        STA $434C                       ; $0787
        LDA #$CE                      ; $078A
        STA $4374                       ; $078C
        STA $4375                       ; $078F
        JSR clear_playfield                      ; $0792
        LDA #$F8                        ; $0795
        JSR fill_color_ram                      ; $0797
        LDX #$27                        ; $079A
L_079C:
        LDA #$00                        ; $079C
        STA $D800,X                     ; $079E
        STA $D828,X                     ; $07A1
        LDA #$20                        ; $07A4
        STA $8C00,X                     ; $07A6
        LDA #$06                        ; $07A9
        STA $8C50,X                     ; $07AB
        CPX #$08                        ; $07AE
        BCS L_07BC                      ; $07B0
        LDA #$0F                        ; $07B2
        STA $8FF8,X                     ; $07B4
        LDA #$FF                        ; $07B7
        STA $8900,X                     ; $07B9
L_07BC:
        DEX                             ; $07BC
        BPL L_079C                      ; $07BD
        LDA #$00                        ; $07BF
        TAX                             ; $07C1
L_07C2:
        STA $8180,X                     ; $07C2
        STA $8280,X                     ; $07C5
        STA $8300,X                     ; $07C8
        INX                             ; $07CB
        BNE L_07C2                      ; $07CC
        LDX #$03                        ; $07CE
L_07D0:
        LDA $479B,X                     ; $07D0
        STA $D020,X                     ; $07D3
        DEX                             ; $07D6
        BPL L_07D0                      ; $07D7
        LDA #$01                        ; $07D9
        STA $DD00                       ; $07DB
        LDA #$38                        ; $07DE
        STA $D018                       ; $07E0
        STA $F2                         ; $07E3
        LDA #$18                        ; $07E5
        STA $D016                       ; $07E7
        LDA #$00                        ; $07EA
        STA $D015                       ; $07EC
        STA $D017                       ; $07EF
        STA $D01B                       ; $07F2
        STA $D01C                       ; $07F5
        LDA #$FF                        ; $07F8
        STA $D01D                       ; $07FA
        LDA #$80                        ; $07FD
        JSR L_315A                      ; $07FF
        JSR L_31C2                      ; $0802
        LDA #$11                        ; $0805
        STA $43BD                       ; $0807
        JSR timed_caption                      ; $080A
        JSR L_1ABB                      ; $080D
        JSR L_1AC7                      ; $0810
        JSR lives_to_bcd                      ; $0813
        LDA #$01                        ; $0816
        STA $60                         ; $0818
        STA $66                         ; $081A
        LDX #$04                        ; $081C
        LDA #$FF                        ; $081E
L_0820:
        STA $4F40,X                     ; $0820
        DEX                             ; $0823
        BPL L_0820                      ; $0824
        LDX #$00                        ; $0826
        LDA #$00                        ; $0828
        JMP wake_actor                  ; $082A
; Turn off CIA interrupts, enable the raster interrupt, and open the SID noise channel that
; the effects use. The vector is raster_irq and the compare starts at raster $FB.
arm_raster:
        LDA #$7F                        ; $082D
        STA $DD0D                       ; $082F
        STA $DC0D                       ; $0832
        LDA $DD0D                       ; $0835
        LDA $DC0D                       ; $0838
        CLD                             ; $083B
        LDA #$C1                        ; $083C
        STA $0408                       ; $083E
        LDA #$48                        ; $0841
        STA $0409                       ; $0843
        LDA #$01                        ; $0846
        STA $D01A                       ; $0848
        STA $D019                       ; $084B
        LDA $D011                       ; $084E
        AND #$7F                        ; $0851
        STA $D011                       ; $0853
        LDA #$FB                        ; $0856
        STA $D012                       ; $0858
        LDA #$FF                        ; $085B
        STA $D40E                       ; $085D
        STA $D40F                       ; $0860
        LDA #$80                        ; $0863
        STA $D412                       ; $0865
        RTS                             ; $0868
; A is the low byte and Y the high byte of the first row. Builds 13 pointers, each $50
; bytes (two character rows) after the last.
build_row_pointers:
        LDX #$00                        ; $0869
        STA $4338                       ; $086B
        STY $434C                       ; $086E
L_0871:
        LDA $4338,X                     ; $0871
        CLC                             ; $0874
        ADC #$50                        ; $0875
        STA $4339,X                     ; $0877
        LDA $434C,X                     ; $087A
        ADC #$00                        ; $087D
        STA $434D,X                     ; $087F
        INX                             ; $0882
        CPX #$0D                        ; $0883
        BCC L_0871                      ; $0885
        LDA $4338                       ; $0887
        CLC                             ; $088A
        ADC #$28                        ; $088B
        STA $4360                       ; $088D
        LDA $434C                       ; $0890
        ADC #$00                        ; $0893
        STA $4374                       ; $0895
        LDX #$00                        ; $0898
L_089A:
        LDA $4360,X                     ; $089A
        CLC                             ; $089D
        ADC #$50                        ; $089E
        STA $4361,X                     ; $08A0
        LDA $4374,X                     ; $08A3
        ADC #$00                        ; $08A6
        STA $4375,X                     ; $08A8
        INX                             ; $08AB
        CPX #$0D                        ; $08AC
        BCC L_089A                      ; $08AE
        RTS                             ; $08B0
; Wait on the title. A fresh key in $0109 is dispatched; otherwise this watches for the
; button.
title_loop:
        LDA $49                         ; $08B1
        ORA $4A                         ; $08B3
        BNE L_08BA                      ; $08B5
        JMP reset_actors                ; $08B7
L_08BA:
        LDA #$00                        ; $08BA
        STA $010E                       ; $08BC
        JSR read_stick                  ; $08BF
        LDA $010C                       ; $08C2
        BEQ L_08E0                      ; $08C5
        LDA #$00                        ; $08C7
        STA $010C                       ; $08C9
        LDA $0109                       ; $08CC
        JMP title_key                      ; $08CF
L_08D2:
        AND #$7F                        ; $08D2
        CMP #$03                        ; $08D4
        BEQ L_08FC                      ; $08D6
        CMP #$06                        ; $08D8
        BEQ L_0908                      ; $08DA
        CMP #$05                        ; $08DC
        BEQ L_092C                      ; $08DE
L_08E0:
        LDA $4A                         ; $08E0
        CMP #$08                        ; $08E2
        BNE L_08F5                      ; $08E4
        LDA $49                         ; $08E6
        CMP #$02                        ; $08E8
        BCS L_08F5                      ; $08EA
        LDA #$0E                        ; $08EC
        LDX #$0A                        ; $08EE
        LDY #$02                        ; $08F0
        JSR print_string                      ; $08F2
L_08F5:
        LDA $DC01                       ; $08F5
        AND #$10                        ; $08F8
        BNE title_loop                      ; $08FA
L_08FC:
        JSR wait_button_up                      ; $08FC
        LDA #$10                        ; $08FF
        STA $48                         ; $0901
L_0903:
        LDA $48                         ; $0903
        BNE L_0903                      ; $0905
        RTS                             ; $0907
L_0908:
        LDA $42CA                       ; $0908
        EOR #$01                        ; $090B
        STA $42CA                       ; $090D
        STA $42CC                       ; $0910
        JSR flip_menu_b                      ; $0913
L_0916:
        JSR wait_button_up                      ; $0916
        LDA #$09                        ; $0919
        STA $4A                         ; $091B
        LDA #$60                        ; $091D
        STA $49                         ; $091F
        LDA #$04                        ; $0921
        STA $48                         ; $0923
L_0925:
        LDA $48                         ; $0925
        BNE L_0925                      ; $0927
        JMP title_loop                      ; $0929
L_092C:
        LDA $42C9                       ; $092C
        EOR #$01                        ; $092F
        STA $42C9                       ; $0931
        STA $42CB                       ; $0934
        JSR flip_menu_a                      ; $0937
        JSR wait_button_up                      ; $093A
        LDA #$09                        ; $093D
        STA $4A                         ; $093F
        LDA #$60                        ; $0941
        STA $49                         ; $0943
        LDA #$04                        ; $0945
        STA $48                         ; $0947
        BNE L_0925                      ; $0949
; Stay here while either joystick button is still down.
wait_button_up:
        LDA $DC00                       ; $094B
        AND $DC01                       ; $094E
        AND #$10                        ; $0951
        BEQ wait_button_up                      ; $0953
        RTS                             ; $0955
; The title keys. $2A, $AA, $11 and $91 each print a line and set or clear $1383 or $1384.
title_key:
        CMP #$2A                        ; $0956
        BEQ L_0969                      ; $0958
        CMP #$AA                        ; $095A
        BEQ L_0979                      ; $095C
        CMP #$11                        ; $095E
        BEQ L_0988                      ; $0960
        CMP #$91                        ; $0962
        BEQ L_0998                      ; $0964
        JMP L_08D2                      ; $0966
L_0969:
        LDA #$08                        ; $0969
        LDX #$0A                        ; $096B
        LDY #$02                        ; $096D
        JSR print_string                      ; $096F
        LDA #$00                        ; $0972
        STA $1383                       ; $0974
        BEQ L_09A6                      ; $0977
L_0979:
        LDA #$0A                        ; $0979
        TAX                             ; $097B
        LDY #$02                        ; $097C
        JSR print_string                      ; $097E
        LDA #$00                        ; $0981
        STA $1384                       ; $0983
        BEQ L_09A6                      ; $0986
L_0988:
        LDA #$09                        ; $0988
        LDX #$0A                        ; $098A
        LDY #$02                        ; $098C
        JSR print_string                      ; $098E
        LDA #$01                        ; $0991
        STA $1383                       ; $0993
        BNE L_09A6                      ; $0996
L_0998:
        LDA #$0D                        ; $0998
        LDX #$0A                        ; $099A
        LDY #$02                        ; $099C
        JSR print_string                      ; $099E
        LDA #$01                        ; $09A1
        STA $1384                       ; $09A3
L_09A6:
        JMP L_0916                      ; $09A6
; Write A through all four pages of color RAM, $D800 to $DBFF.
fill_color_ram:
        LDX #$00                        ; $09A9
L_09AB:
        STA $D800,X                     ; $09AB
        STA $D900,X                     ; $09AE
        STA $DA00,X                     ; $09B1
        STA $DB00,X                     ; $09B4
        INX                             ; $09B7
        BNE L_09AB                      ; $09B8
        RTS                             ; $09BA
        .byte $04

; Blank the screen and load room $29. $42C9 and $42CA are the two title options; they
; change which slots the enemies use.
enter_room:
        LDA $D011                       ; $09BC
        AND #$EF                        ; $09BF
        STA $D011                       ; $09C1
        LDA $D015                       ; $09C4
        AND #$1F                        ; $09C7
        STA $47                         ; $09C9
        LDA #$00                        ; $09CB
        STA $D015                       ; $09CD
        LDX #$27                        ; $09D0
        LDA #$20                        ; $09D2
L_09D4:
        STA $8C00,X                     ; $09D4
        DEX                             ; $09D7
        BPL L_09D4                      ; $09D8
        LDX $29                         ; $09DA
        LDA $47A0,X                     ; $09DC
        LDX #$00                        ; $09DF
L_09E1:
        STA $D878,X                     ; $09E1
        STA $D900,X                     ; $09E4
        STA $DA00,X                     ; $09E7
        STA $DB00,X                     ; $09EA
        INX                             ; $09ED
        BNE L_09E1                      ; $09EE
        JSR L_423F                      ; $09F0
        JSR silence_sid                      ; $09F3
        LDA $29                         ; $09F6
        CMP #$0B                        ; $09F8
        BNE L_0A0E                      ; $09FA
        LDA $36                         ; $09FC
        ORA $38                         ; $09FE
        ORA $3A                         ; $0A00
        BNE L_0A0E                      ; $0A02
        LDA $47                         ; $0A04
        STA $D015                       ; $0A06
        LDX #$0D                        ; $0A09
        JMP enter_room_at                      ; $0A0B
L_0A0E:
        LDA $29                         ; $0A0E
        ASL A                           ; $0A10
        ADC $03                         ; $0A11
        TAX                             ; $0A13
        LDA $0300,X                     ; $0A14
        BEQ L_0A2A                      ; $0A17
        LSR $0300,X                     ; $0A19
        LDA #$07                        ; $0A1C
        LDX #$01                        ; $0A1E
        JSR add_score                      ; $0A20
        LDA #$D0                        ; $0A23
        LDX #$02                        ; $0A25
        JSR add_score                      ; $0A27
L_0A2A:
        LDA #$00                        ; $0A2A
        LDY #$83                        ; $0A2C
L_0A2E:
        STA $4F57,Y                     ; $0A2E
        DEY                             ; $0A31
        BPL L_0A2E                      ; $0A32
        STA $87                         ; $0A34
        STA $88                         ; $0A36
        STA $65                         ; $0A38
        LDA #$07                        ; $0A3A
        STA $4321                       ; $0A3C
        LDX $29                         ; $0A3F
        LDA $4C9E,X                     ; $0A41
        STA $7A                         ; $0A44
        BEQ L_0A4E                      ; $0A46
        LDA #$00                        ; $0A48
        STA $8B                         ; $0A4A
        STA $8C                         ; $0A4C
L_0A4E:
        LDA $130B,X                     ; $0A4E
        STA $5D                         ; $0A51
        LDA $1347,X                     ; $0A53
        STA $5E                         ; $0A56
        LDA $40                         ; $0A58
        BEQ L_0A62                      ; $0A5A
        LDA #$03                        ; $0A5C
        STA $5D                         ; $0A5E
        STA $5E                         ; $0A60
L_0A62:
        LDA $42CA                       ; $0A62
        BEQ L_0A6B                      ; $0A65
        LDA #$08                        ; $0A67
        STA $5E                         ; $0A69
L_0A6B:
        LDA #$01                        ; $0A6B
        STA hit                         ; $0A6D
        LDA #$04                        ; $0A6F
        STA timer                       ; $0A71
        LDA #$00                        ; $0A73
        STA $4FDE                       ; $0A75
        STA $4FDF                       ; $0A78
        STA $4FE0                       ; $0A7B
        STA state                       ; $0A7E
        STA $0106                       ; $0A80
        LDA $29                         ; $0A83
        ASL A                           ; $0A85
        TAX                             ; $0A86
        LDA $4AA8,X                     ; $0A87
        STA $06                         ; $0A8A
        LDA $4AA9,X                     ; $0A8C
        STA $07                         ; $0A8F
        LDY #$13                        ; $0A91
L_0A93:
        LDA ($06),Y                     ; $0A93
        STA $0153,Y                     ; $0A95
        DEY                             ; $0A98
        BPL L_0A93                      ; $0A99
        ; $D018 follows the room. Below $0A the value is $38, charset $A000.
        ; Room $12 is $3C, charset $B000. Every other room is $3A, charset
        ; $A800. $B800 is expanded at startup and never selected here.
        LDA #$38                        ; $0A9B
        LDX $29                         ; $0A9D
        CPX #$0A                        ; $0A9F
        BCC L_0AAD                      ; $0AA1
        CPX #$12                        ; $0AA3
        BEQ L_0AAB                      ; $0AA5
        LDA #$3A                        ; $0AA7
        BNE L_0AAD                      ; $0AA9
L_0AAB:
        LDA #$3C                        ; $0AAB
L_0AAD:
        STA $F2                         ; $0AAD
        LDA $4B34,X                     ; $0AAF
        STA $06                         ; $0AB2
        LDA $4B49,X                     ; $0AB4
        STA $07                         ; $0AB7
        LDA #$78                        ; $0AB9
        STA $04                         ; $0ABB
        STA $10                         ; $0ABD
        LDA #$A0                        ; $0ABF
        STA $08                         ; $0AC1
        STA $12                         ; $0AC3
        LDA #$8C                        ; $0AC5
        STA $05                         ; $0AC7
        STA $09                         ; $0AC9
        LDA #$D8                        ; $0ACB
        STA $11                         ; $0ACD
        STA $13                         ; $0ACF
        LDA #$27                        ; $0AD1
        STA $F3                         ; $0AD3
        LDY #$00                        ; $0AD5
; One command byte, then its payload. A byte below $80 is a run: the next
; source byte is written that many times, and a count of 0 writes 256 because
; the loop is DEX/BNE. A byte at or above $80 is a literal: the low seven bits
; are the count, $80 ends the stream, and each following source byte is one
; tile. A payload byte with bit 7 set stores color $F8 on both color rows and
; the tile is the low seven bits. Otherwise the color stays the room fill
; from $47A0,X, which was written across $D878 before this loop. Rooms 4, 7,
; 10, 13, 17, and 18 emit 442 cells. The screen holds 1000, so the last two
; land past it and are not part of the picture.
L_0AD7:
        LDA ($06),Y                     ; $0AD7
        BPL L_0B07                      ; $0AD9
        AND #$7F                        ; $0ADB
        BEQ L_0B30                      ; $0ADD
        TAX                             ; $0ADF
        JSR L_478D                      ; $0AE0
L_0AE3:
        LDA ($06),Y                     ; $0AE3
        BPL L_0AEA                      ; $0AE5
        JSR L_0B5F                      ; $0AE7
L_0AEA:
        STA ($04),Y                     ; $0AEA
        ORA #$80                        ; $0AEC
        STA ($08),Y                     ; $0AEE
        JSR L_4786                      ; $0AF0
        JSR L_478D                      ; $0AF3
        JSR L_4794                      ; $0AF6
        JSR L_0B51                      ; $0AF9
        JSR L_0B58                      ; $0AFC
        JSR L_485B                      ; $0AFF
        DEX                             ; $0B02
        BNE L_0AE3                      ; $0B03
        BEQ L_0AD7                      ; $0B05
L_0B07:
        TAX                             ; $0B07
        JSR L_478D                      ; $0B08
L_0B0B:
        LDA ($06),Y                     ; $0B0B
        BPL L_0B12                      ; $0B0D
        JSR L_0B5F                      ; $0B0F
L_0B12:
        STA ($04),Y                     ; $0B12
        ORA #$80                        ; $0B14
        STA ($08),Y                     ; $0B16
        JSR L_4786                      ; $0B18
        JSR L_4794                      ; $0B1B
        JSR L_0B51                      ; $0B1E
        JSR L_0B58                      ; $0B21
        JSR L_485B                      ; $0B24
        DEX                             ; $0B27
        BNE L_0B0B                      ; $0B28
        JSR L_478D                      ; $0B2A
        JMP L_0AD7                      ; $0B2D
L_0B30:
        JSR L_336E                      ; $0B30
        JSR show_screen                      ; $0B33
        NOP                             ; $0B36
        NOP                             ; $0B37
        NOP                             ; $0B38
        NOP                             ; $0B39
        NOP                             ; $0B3A
        LDA $47                         ; $0B3B
        STA $D015                       ; $0B3D
        JMP L_3A6F                      ; $0B40
L_0B43:
        INC $04                         ; $0B43
        BNE L_0B49                      ; $0B45
        INC $05                         ; $0B47
L_0B49:
        RTS                             ; $0B49

; $0B4A  7 bytes of data between routines

L_0B51:
        INC $10                         ; $0B51
        BNE L_0B57                      ; $0B53
        INC $11                         ; $0B55
L_0B57:
        RTS                             ; $0B57
L_0B58:
        INC $12                         ; $0B58
        BNE L_0B5E                      ; $0B5A
        INC $13                         ; $0B5C
L_0B5E:
        RTS                             ; $0B5E
L_0B5F:
        PHA                             ; $0B5F
        LDA #$F8                        ; $0B60
        STA ($10),Y                     ; $0B62
        STA ($12),Y                     ; $0B64
        PLA                             ; $0B66
        AND #$7F                        ; $0B67
        RTS                             ; $0B69

; $0B6A  36 bytes of data between routines

; Mark this actor present and give him a standing picture. A carries the extra active
; flags. X is the actor. $98,X starts at $0F, which is no direction.
wake_actor:
        ORA #$01                        ; $0B8E
        STA active,X                    ; $0B90
        ROL A                           ; $0B92
        ROL A                           ; $0B93
        ROL A                           ; $0B94
        AND #$03                        ; $0B95
        TAY                             ; $0B97
        STY $52                         ; $0B98
        LDA $59A8,Y                     ; $0B9A
        STA $EC,X                       ; $0B9D
        JSR L_0E21                      ; $0B9F
        LDY $52                         ; $0BA2
        LDA $59AB,Y                     ; $0BA4
        STA $EF,X                       ; $0BA7
        BEQ L_0BAE                      ; $0BA9
        JSR L_0E21                      ; $0BAB
L_0BAE:
        LDY $52                         ; $0BAE
        LDA #$00                        ; $0BB0
        STA state,X                     ; $0BB2
        STA timer,X                     ; $0BB4
        STA $0106,X                     ; $0BB6
        LDA #$0F                        ; $0BB9
        STA $98,X                       ; $0BBB
        LDA #$01                        ; $0BBD
        STA $9B,X                       ; $0BBF
        LDA #$01                        ; $0BC1
        STA hit,X                       ; $0BC3
        LDA #$01                        ; $0BC5
        STA facing,X                    ; $0BC7
        LDA #$02                        ; $0BC9
        STA $4F48,X                     ; $0BCB
        STA $4F4E,X                     ; $0BCE
        LDA #$03                        ; $0BD1
        STA $4F4B,X                     ; $0BD3
        STA $4F51,X                     ; $0BD6
        LDA #$24                        ; $0BD9
        STA $D1,X                       ; $0BDB
        STX $51                         ; $0BDD
        LDA $0104                       ; $0BDF
        BEQ L_0BE7                      ; $0BE2
        TXA                             ; $0BE4
        BNE L_0BFA                      ; $0BE5
L_0BE7:
        LDX $29                         ; $0BE7
        LDA $4BC2,X                     ; $0BE9
        TAY                             ; $0BEC
        LDA $4BAE,X                     ; $0BED
L_0BF0:
        LDX $51                         ; $0BF0
        STA xpos,X                      ; $0BF2
        TYA                             ; $0BF4
        STA ypos,X                      ; $0BF5
        JMP L_0C4D                      ; $0BF7
L_0BFA:
        LDA $8A,X                       ; $0BFA
        BNE L_0C3F                      ; $0BFC
        CPX #$01                        ; $0BFE
        BNE L_0C2A                      ; $0C00
        LDX $29                         ; $0C02
        LDA $136F,X                     ; $0C04
        BEQ L_0C17                      ; $0C07
        JSR noise_byte                      ; $0C09
        CMP #$C0                        ; $0C0C
        BCS L_0C17                      ; $0C0E
        LDA $136F,X                     ; $0C10
        BPL L_0BE7                      ; $0C13
        BMI L_0C1E                      ; $0C15
L_0C17:
        JSR noise_byte                      ; $0C17
        AND #$01                        ; $0C1A
        BEQ L_0BE7                      ; $0C1C
L_0C1E:
        LDX $29                         ; $0C1E
        LDA $4BEA,X                     ; $0C20
        TAY                             ; $0C23
        LDA $4BD6,X                     ; $0C24
        JMP L_0BF0                      ; $0C27
L_0C2A:
        LDX $29                         ; $0C2A
        LDA $1333,X                     ; $0C2C
        BEQ L_0C17                      ; $0C2F
        JSR noise_byte                      ; $0C31
        CMP #$C0                        ; $0C34
        BCS L_0C17                      ; $0C36
        LDA $136F,X                     ; $0C38
        BPL L_0BE7                      ; $0C3B
        BMI L_0C1E                      ; $0C3D
L_0C3F:
        STA xpos,X                      ; $0C3F
        LDA $8C,X                       ; $0C41
        STA ypos,X                      ; $0C43
        LDA $8E,X                       ; $0C45
        STA $D1,X                       ; $0C47
        LDA #$00                        ; $0C49
        STA $8A,X                       ; $0C4B
L_0C4D:
        LDA $276D,X                     ; $0C4D
        LDY $277D                       ; $0C50
        BEQ L_0C58                      ; $0C53
        CLC                             ; $0C55
        ADC #$05                        ; $0C56
L_0C58:
        TAY                             ; $0C58
        LDA $2763,Y                     ; $0C59
        LDY $276D,X                     ; $0C5C
        STA $8FF8,Y                     ; $0C5F
        INY                             ; $0C62
        CLC                             ; $0C63
        ADC #$01                        ; $0C64
        STA $8FF8,Y                     ; $0C66
        LDA xpos,X                      ; $0C69
        ASL A                           ; $0C6B
        PHA                             ; $0C6C
        LDY $276D,X                     ; $0C6D
        LDA $D010                       ; $0C70
        BCC L_0C7A                      ; $0C73
        ORA $2773,Y                     ; $0C75
        BNE L_0C7D                      ; $0C78
L_0C7A:
        AND $2778,Y                     ; $0C7A
L_0C7D:
        STA $D010                       ; $0C7D
        TYA                             ; $0C80
        ASL A                           ; $0C81
        TAY                             ; $0C82
        PLA                             ; $0C83
        STA $D000,Y                     ; $0C84
        LDA ypos,X                      ; $0C87
        CLC                             ; $0C89
        ADC #$1D                        ; $0C8A
        STA $D001,Y                     ; $0C8C
        LDA $2770,X                     ; $0C8F
        BMI L_0CBA                      ; $0C92
        LDA xpos,X                      ; $0C94
        ASL A                           ; $0C96
        PHA                             ; $0C97
        LDY $2770,X                     ; $0C98
        LDA $D010                       ; $0C9B
        BCC L_0CA5                      ; $0C9E
        ORA $2773,Y                     ; $0CA0
        BNE L_0CA8                      ; $0CA3
L_0CA5:
        AND $2778,Y                     ; $0CA5
L_0CA8:
        STA $D010                       ; $0CA8
        TYA                             ; $0CAB
        ASL A                           ; $0CAC
        TAY                             ; $0CAD
        PLA                             ; $0CAE
        STA $D000,Y                     ; $0CAF
        LDA ypos,X                      ; $0CB2
        CLC                             ; $0CB4
        ADC #$1D                        ; $0CB5
        STA $D001,Y                     ; $0CB7
L_0CBA:
        LDY $276D,X                     ; $0CBA
        LDA $D015                       ; $0CBD
        ORA $2773,Y                     ; $0CC0
        STA $D015                       ; $0CC3
        LDY $2770,X                     ; $0CC6
        BMI L_0CD4                      ; $0CC9
        LDA $D015                       ; $0CCB
        ORA $2773,Y                     ; $0CCE
        STA $D015                       ; $0CD1
L_0CD4:
        RTS                             ; $0CD4
L_0CD5:
        JSR blip_and_return                      ; $0CD5
        LDA $CF79                       ; $0CD8
        EOR $CF99                       ; $0CDB
        STA $1166                       ; $0CDE
        LDX #$01                        ; $0CE1
        LDA #$80                        ; $0CE3
        JSR wake_actor                  ; $0CE5
        LDA #$10                        ; $0CE8
        STA state,X                     ; $0CEA
        LDA #$2A                        ; $0CEC
        STA $4F48,X                     ; $0CEE
        LDA #$2B                        ; $0CF1
        STA $4F4B,X                     ; $0CF3
        LDA #$6C                        ; $0CF6
        STA $4F4E,X                     ; $0CF8
        LDA #$6D                        ; $0CFB
        STA $4F51,X                     ; $0CFD
        LDA #$0A                        ; $0D00
        STA timer,X                     ; $0D02
        LDA $D1,X                       ; $0D04
        CMP #$14                        ; $0D06
        BCC L_0D0E                      ; $0D08
        LDA #$14                        ; $0D0A
        STA $D1,X                       ; $0D0C
L_0D0E:
        RTS                             ; $0D0E
L_0D0F:
        LDA $5E                         ; $0D0F
        BNE L_0D3C                      ; $0D11
        LDX #$01                        ; $0D13
        LDA active,X                    ; $0D15
        AND #$01                        ; $0D17
        BNE L_0D3C                      ; $0D19
        JSR L_0CD5                      ; $0D1B
        LDX $29                         ; $0D1E
        LDA $135B,X                     ; $0D20
        STA $5E                         ; $0D23
        LDA $42CA                       ; $0D25
        BEQ L_0D2E                      ; $0D28
        LDA #$30                        ; $0D2A
        STA $5E                         ; $0D2C
L_0D2E:
        JSR L_10D0                      ; $0D2E
        BVS L_0D3D                      ; $0D31
        BCS L_0D3A                      ; $0D33
        LDA $40                         ; $0D35
        BNE L_0D3D                      ; $0D37
        RTS                             ; $0D39
L_0D3A:
        LSR $5E                         ; $0D3A
L_0D3C:
        RTS                             ; $0D3C
L_0D3D:
        LDA #$03                        ; $0D3D
        STA $5E                         ; $0D3F
        RTS                             ; $0D41
L_0D42:
        CPX #$01                        ; $0D42
        BNE L_0D72                      ; $0D44
L_0D46:
        LDA #$5B                        ; $0D46
        CLC                             ; $0D48
        ADC $29                         ; $0D49
        STA $0D56                       ; $0D4B
        LDA #$13                        ; $0D4E
        ADC #$00                        ; $0D50
        STA $0D57                       ; $0D52
        LDA $E000                       ; $0D55
        STA $5E                         ; $0D58
        LDA $42CA                       ; $0D5A
        BEQ L_0D63                      ; $0D5D
        LDA #$30                        ; $0D5F
        STA $5E                         ; $0D61
L_0D63:
        JSR L_10D0                      ; $0D63
        BVS L_0D3D                      ; $0D66
        BCS L_0D6F                      ; $0D68
        LDA $40                         ; $0D6A
        BNE L_0D3D                      ; $0D6C
        RTS                             ; $0D6E
L_0D6F:
        LSR $5E                         ; $0D6F
        RTS                             ; $0D71
L_0D72:
        CPX #$02                        ; $0D72
        BNE L_0D9E                      ; $0D74
L_0D76:
        LDA #$1F                        ; $0D76
        CLC                             ; $0D78
        ADC $29                         ; $0D79
        STA $0D86                       ; $0D7B
        LDA #$13                        ; $0D7E
        ADC #$00                        ; $0D80
        STA $0D87                       ; $0D82
        LDA $E000                       ; $0D85
        STA $5D                         ; $0D88
        JSR L_10D0                      ; $0D8A
        BVS L_0D99                      ; $0D8D
        BCS L_0D96                      ; $0D8F
        LDA $40                         ; $0D91
        BNE L_0D99                      ; $0D93
        RTS                             ; $0D95
L_0D96:
        LSR $5D                         ; $0D96
        RTS                             ; $0D98
L_0D99:
        LDA #$03                        ; $0D99
        STA $5D                         ; $0D9B
        RTS                             ; $0D9D
L_0D9E:
        JSR L_0D46                      ; $0D9E
        JMP L_0D76                      ; $0DA1
L_0DA4:
        LDA $5D                         ; $0DA4
        BEQ L_0DAA                      ; $0DA6
        DEC $5D                         ; $0DA8
L_0DAA:
        LDA $5E                         ; $0DAA
        BEQ L_0DB0                      ; $0DAC
        DEC $5E                         ; $0DAE
L_0DB0:
        RTS                             ; $0DB0
L_0DB1:
        LDA $5D                         ; $0DB1
        BNE L_0DF7                      ; $0DB3
        LDA $96                         ; $0DB5
        AND #$01                        ; $0DB7
        BEQ L_0DD7                      ; $0DB9
        LDA $40                         ; $0DBB
        BNE L_0DD7                      ; $0DBD
        LDA $42CA                       ; $0DBF
        BNE L_0DD7                      ; $0DC2
        JSR L_10D0                      ; $0DC4
        BVS L_0DD7                      ; $0DC7
        BCS L_0DD7                      ; $0DC9
        JSR noise_byte                      ; $0DCB
        CMP #$19                        ; $0DCE
        BCC L_0DD7                      ; $0DD0
        LDA #$10                        ; $0DD2
        STA $5D                         ; $0DD4
        RTS                             ; $0DD6
L_0DD7:
        LDX #$02                        ; $0DD7
        LDA active,X                    ; $0DD9
        AND #$01                        ; $0DDB
        BNE L_0DF7                      ; $0DDD
        JSR L_0DF8                      ; $0DDF
        LDX $29                         ; $0DE2
        LDA $131F,X                     ; $0DE4
        STA $5D                         ; $0DE7
        JSR L_10D0                      ; $0DE9
        BVS L_0D99                      ; $0DEC
        BCS L_0DF5                      ; $0DEE
        LDA $40                         ; $0DF0
        BNE L_0D99                      ; $0DF2
        RTS                             ; $0DF4
L_0DF5:
        LSR $5D                         ; $0DF5
L_0DF7:
        RTS                             ; $0DF7
L_0DF8:
        JSR blip_and_return                      ; $0DF8
        LDA #$40                        ; $0DFB
        JSR wake_actor                  ; $0DFD
        LDA #$82                        ; $0E00
        STA $4F4E,X                     ; $0E02
        LDA #$00                        ; $0E05
        STA $4F4B,X                     ; $0E07
        STA $4F51,X                     ; $0E0A
        LDA $D1,X                       ; $0E0D
        CMP #$07                        ; $0E0F
        BCC L_0E17                      ; $0E11
        LDA #$07                        ; $0E13
        STA $D1,X                       ; $0E15
L_0E17:
        LDA #$10                        ; $0E17
        STA timer,X                     ; $0E19
        JSR L_2DDD                      ; $0E1B
        JMP L_2E1C                      ; $0E1E
L_0E21:
        SEC                             ; $0E21
        SBC #$03                        ; $0E22
        TAY                             ; $0E24
        LDA $59AE,Y                     ; $0E25
        TAY                             ; $0E28
        TXA                             ; $0E29
        STA $4F40,Y                     ; $0E2A
        RTS                             ; $0E2D
L_0E2E:
        TXA                             ; $0E2E
        PHA                             ; $0E2F
        LDX #$04                        ; $0E30
L_0E32:
        LDA $0E3B,X                     ; $0E32
        DEX                             ; $0E35
        BPL L_0E32                      ; $0E36
        PLA                             ; $0E38
        TAX                             ; $0E39
        RTS                             ; $0E3A

; $0E3B  17 bytes of data between routines

; Flip bit 6 of the glyphs on the second menu row, then draw the word selected by $42CA.
flip_menu_b:
        LDX #$13                        ; $0E4C
L_0E4E:
        LDA $8E40,X                     ; $0E4E
        CMP #$4D                        ; $0E51
        BEQ L_0E5A                      ; $0E53
        EOR #$40                        ; $0E55
        STA $8E40,X                     ; $0E57
L_0E5A:
        DEX                             ; $0E5A
        BPL L_0E4E                      ; $0E5B
        LDX #$50                        ; $0E5D
        LDA $42CA                       ; $0E5F
        JMP draw_menu_word                      ; $0E62
; Flip bit 6 of the glyphs on the first menu row, then draw the word selected by $42C9.
flip_menu_a:
        LDX #$13                        ; $0E65
L_0E67:
        LDA $8DF0,X                     ; $0E67
        CMP #$4D                        ; $0E6A
        BEQ L_0E73                      ; $0E6C
        EOR #$40                        ; $0E6E
        STA $8DF0,X                     ; $0E70
L_0E73:
        DEX                             ; $0E73
        BPL L_0E67                      ; $0E74
        LDX #$00                        ; $0E76
        LDA $42C9                       ; $0E78
; X picks the row. A is the option. The four bytes written are the word for that choice.
draw_menu_word:
        BNE L_0E92                      ; $0E7B
        LDA #$42                        ; $0E7D
        STA $8DEF,X                     ; $0E7F
        LDA #$40                        ; $0E82
        STA $8DF8,X                     ; $0E84
        LDA #$56                        ; $0E87
        STA $8DFA,X                     ; $0E89
        LDA #$00                        ; $0E8C
        STA $8E04,X                     ; $0E8E
        RTS                             ; $0E91
L_0E92:
        LDA #$00                        ; $0E92
        STA $8DEF,X                     ; $0E94
        LDA #$42                        ; $0E97
        STA $8DF8,X                     ; $0E99
        LDA #$40                        ; $0E9C
        STA $8DFA,X                     ; $0E9E
        LDA #$56                        ; $0EA1
        STA $8E04,X                     ; $0EA3
        RTS                             ; $0EA6
; Blue screen, the two option lines, and a countdown in $48. Used between lives.
between_rooms:
        SEI                             ; $0EA7
        LDA $D011                       ; $0EA8
        AND #$EF                        ; $0EAB
        STA $D011                       ; $0EAD
        LDA #$00                        ; $0EB0
        STA $D015                       ; $0EB2
        LDA #$06                        ; $0EB5
        STA $D020                       ; $0EB7
        STA $D021                       ; $0EBA
        LDX #$00                        ; $0EBD
        LDA #$DC                        ; $0EBF
L_0EC1:
        STA $8C00,X                     ; $0EC1
        STA $8D00,X                     ; $0EC4
        STA $8E00,X                     ; $0EC7
        STA $8F00,X                     ; $0ECA
        INX                             ; $0ECD
        BNE L_0EC1                      ; $0ECE
        JSR silence_sid                      ; $0ED0
        LDA $03                         ; $0ED3
        ASL A                           ; $0ED5
        TAX                             ; $0ED6
        LDY #$00                        ; $0ED7
L_0ED9:
        LDA $4A7D,X                     ; $0ED9
        STA $4A6B,Y                     ; $0EDC
        LDA $4A81,X                     ; $0EDF
        STA $4A7B,Y                     ; $0EE2
        INX                             ; $0EE5
        INY                             ; $0EE6
        CPY #$02                        ; $0EE7
        BCC L_0ED9                      ; $0EE9
        LDX #$0F                        ; $0EEB
L_0EED:
        LDA $4A5D,X                     ; $0EED
        ORA #$80                        ; $0EF0
        STA $8D24,X                     ; $0EF2
        LDA $4A6D,X                     ; $0EF5
        ORA #$80                        ; $0EF8
        STA $8D4C,X                     ; $0EFA
        DEX                             ; $0EFD
        BPL L_0EED                      ; $0EFE
        LDA #$A4                        ; $0F00  low byte of between_raster
        STA $0408                       ; $0F02
        LDA #$14                        ; $0F05
        STA $0409                       ; $0F07
        LDA #$01                        ; $0F0A
        STA $D01A                       ; $0F0C
        STA $D019                       ; $0F0F
        LDA $D011                       ; $0F12
        AND #$7F                        ; $0F15
        STA $D011                       ; $0F17
        LDA #$63                        ; $0F1A
        STA $D012                       ; $0F1C
        LDA #$30                        ; $0F1F
        STA $D018                       ; $0F21
        LDA #$18                        ; $0F24
        STA $D016                       ; $0F26
        JSR show_screen                      ; $0F29
        NOP                             ; $0F2C
        NOP                             ; $0F2D
        NOP                             ; $0F2E
        NOP                             ; $0F2F
        NOP                             ; $0F30
        CLI                             ; $0F31
        LDA #$80                        ; $0F32
        STA $18                         ; $0F34
        STA $19                         ; $0F36
        LDA #$03                        ; $0F38
        STA $1A                         ; $0F3A
        LDA #$40                        ; $0F3C
        STA $48                         ; $0F3E
        LDA $42C9                       ; $0F40
        BEQ L_0F47                      ; $0F43
        ASL $48                         ; $0F45
L_0F47:
        LDA $48                         ; $0F47
        BEQ L_0F71                      ; $0F49
        LDA $18                         ; $0F4B
        BEQ L_0F54                      ; $0F4D
        DEC $18                         ; $0F4F
        JMP L_0F69                      ; $0F51
L_0F54:
        LDA $19                         ; $0F54
        BEQ L_0F5F                      ; $0F56
        DEC $18                         ; $0F58
        DEC $19                         ; $0F5A
        JMP L_0F69                      ; $0F5C
L_0F5F:
        LDA $1A                         ; $0F5F
        BEQ L_0F69                      ; $0F61
        DEC $18                         ; $0F63
        DEC $19                         ; $0F65
        DEC $1A                         ; $0F67
L_0F69:
        LDA $18                         ; $0F69
        ORA $19                         ; $0F6B
        ORA $1A                         ; $0F6D
        BNE L_0F47                      ; $0F6F
L_0F71:
        RTS                             ; $0F71
; Zero the working zero page, the nibble tables at $4EED, and the per-room slots at $0300.
clear_work_ram:
        LDA #$00                        ; $0F72
        TAX                             ; $0F74
L_0F75:
        CPX #$F3                        ; $0F75
        BCS L_0F7B                      ; $0F77
        STA $03,X                       ; $0F79
L_0F7B:
        CPX #$FB                        ; $0F7B
        BCS L_0F82                      ; $0F7D
        STA $4EED,X                     ; $0F7F
L_0F82:
        INX                             ; $0F82
        BNE L_0F75                      ; $0F83
        STA $89                         ; $0F85
        STA $8A                         ; $0F87
        LDX #$07                        ; $0F89
        LDA #$01                        ; $0F8B
L_0F8D:
        STA $014B,X                     ; $0F8D
        STA $0100,X                     ; $0F90
        DEX                             ; $0F93
        BPL L_0F8D                      ; $0F94
        JSR silence_sid                      ; $0F96
        LDA #$05                        ; $0F99
        STA $4E95                       ; $0F9B
        STA $4E96                       ; $0F9E
        JSR copy_room_defaults                      ; $0FA1
        LDX #$27                        ; $0FA4
        LDA #$01                        ; $0FA6
L_0FA8:
        STA $0300,X                     ; $0FA8
        DEX                             ; $0FAB
        BPL L_0FA8                      ; $0FAC
        LDA #$00                        ; $0FAE
        STA $0300                       ; $0FB0
        STA $0301                       ; $0FB3
        JSR L_33CA                      ; $0FB6
        JSR fill_0340                      ; $0FB9
        LDA #$04                        ; $0FBC
        STA $28                         ; $0FBE
        LDA $42CA                       ; $0FC0
        BEQ L_0FC9                      ; $0FC3
        LDA #$09                        ; $0FC5
        STA $28                         ; $0FC7
L_0FC9:
        LDX #$02                        ; $0FC9
L_0FCB:
        LDA $438B,X                     ; $0FCB
        STA $4392,X                     ; $0FCE
        STA $4395,X                     ; $0FD1
        DEX                             ; $0FD4
        BPL L_0FCB                      ; $0FD5
        LDX #$1B                        ; $0FD7
L_0FD9:
        LDA $28,X                       ; $0FD9
        STA $439F,X                     ; $0FDB
        DEX                             ; $0FDE
        BPL L_0FD9                      ; $0FDF
        RTS                             ; $0FE1
; Fill $0340-$03A0 with the value in A. Called with 1.
fill_0340:
        LDA #$01                        ; $0FE2
        LDX #$60                        ; $0FE4
L_0FE6:
        STA $0340,X                     ; $0FE6
        DEX                             ; $0FE9
        BPL L_0FE6                      ; $0FEA
        RTS                             ; $0FEC
L_0FED:
        LDA #$40                        ; $0FED
        STA $04                         ; $0FEF
        LDA #$03                        ; $0FF1
        STA $05                         ; $0FF3
        LDA #$00                        ; $0FF5
        STA $4E                         ; $0FF7
L_0FF9:
        ASL A                           ; $0FF9
        TAX                             ; $0FFA
        JSR L_1007                      ; $0FFB
        INC $4E                         ; $0FFE
        LDA $4E                         ; $1000
        CMP #$14                        ; $1002
        BCC L_0FF9                      ; $1004
        RTS                             ; $1006
L_1007:
        LDA $4CE2,X                     ; $1007
        STA $0A                         ; $100A
        LDA $4CE3,X                     ; $100C
        STA $0B                         ; $100F
        LDY #$00                        ; $1011
L_1013:
        LDA ($0A),Y                     ; $1013
        BMI L_102A                      ; $1015
        PHA                             ; $1017
        LDA ($04),Y                     ; $1018
        TAX                             ; $101A
        PLA                             ; $101B
        STA ($04),Y                     ; $101C
        TXA                             ; $101E
        STA ($0A),Y                     ; $101F
        JSR L_3A7C                      ; $1021
        JSR L_0B43                      ; $1024
        JMP L_1013                      ; $1027
L_102A:
        RTS                             ; $102A

; $102B  16 bytes of data between routines

L_103B:
        LDX #$13                        ; $103B
L_103D:
        LDA $47B4,X                     ; $103D
        STA $0153,X                     ; $1040
        DEX                             ; $1043
        BPL L_103D                      ; $1044
        LDA #$00                        ; $1046
        STA $82                         ; $1048
        RTS                             ; $104A

; $104B  106 bytes of data between routines

; Play the effect for Y = $82, restoring A, X and Y.
blip_and_return:
        PHA                             ; $10B5
        TXA                             ; $10B6
        PHA                             ; $10B7
        TYA                             ; $10B8
        PHA                             ; $10B9
        LDY #$82                        ; $10BA
        JSR play_sound                  ; $10BC
        PLA                             ; $10BF
        TAY                             ; $10C0
        PLA                             ; $10C1
        TAX                             ; $10C2
        PLA                             ; $10C3
        RTS                             ; $10C4
; Copy the 20 default bytes at $4CCE into $2A.
copy_room_defaults:
        LDX #$13                        ; $10C5
L_10C7:
        LDA $4CCE,X                     ; $10C7
        STA $2A,X                       ; $10CA
        DEX                             ; $10CC
        BPL L_10C7                      ; $10CD
        RTS                             ; $10CF
L_10D0:
        LDA $88                         ; $10D0
        CMP #$20                        ; $10D2
        BCS L_10DA                      ; $10D4
        CMP #$19                        ; $10D6
        CLV                             ; $10D8
        RTS                             ; $10D9
L_10DA:
        LDA #$FF                        ; $10DA
        BIT $10DF                       ; $10DC
        RTS                             ; $10DF

; $10E0  81 bytes of data between routines

; Zero $8200-$827F and $8340-$83BF.
clear_sprite_ram:
        PHA                             ; $1131
        TXA                             ; $1132
        PHA                             ; $1133
        LDA #$00                        ; $1134
        LDX #$7F                        ; $1136
L_1138:
        STA $8200,X                     ; $1138
        STA $8340,X                     ; $113B
        DEX                             ; $113E
        BPL L_1138                      ; $113F
        PLA                             ; $1141
        TAX                             ; $1142
        PLA                             ; $1143
        RTS                             ; $1144
; Zero $8280-$82BF and $83C0-$83FF.
clear_upper_sprites:
        PHA                             ; $1145
        TXA                             ; $1146
        PHA                             ; $1147
        LDA #$00                        ; $1148
        LDX #$3F                        ; $114A
L_114C:
        STA $8280,X                     ; $114C
        STA $83C0,X                     ; $114F
        DEX                             ; $1152
        BPL L_114C                      ; $1153
        PLA                             ; $1155
        TAX                             ; $1156
        PLA                             ; $1157
        RTS                             ; $1158
; One noise byte. While $0104 is 0 the result is 1, and flicker_lamps then
; rewrites a steady row every frame. In play, $16 advances and ($16),Y is
; returned. Y belongs to the caller.
noise_byte:
        LDA $0104                       ; $1159
        BNE L_1161                      ; $115C
        LDA #$01                        ; $115E
        RTS                             ; $1160
L_1161:
        INC $16                         ; $1161
        LDA ($16),Y                     ; $1163
        RTS                             ; $1165

; $1166  544 bytes of data between routines


; Keyboard and joystick. $DC00 is port 2, $DC01 is port 1. Either port may be read; the
; keyboard scan uses both.
init_cia:
        LDX #$FF                        ; $1386
        STX $0109                       ; $1388
        INX                             ; $138B
        STX $010C                       ; $138C
        STX $DC02                       ; $138F
        STX $DC03                       ; $1392
        LDA #$7F                        ; $1395
        STA $DC0D                       ; $1397
        STA $DD0D                       ; $139A
        RTS                             ; $139D
read_stick:
        LDA $010C                       ; $139E
        BNE L_13AD                      ; $13A1
        LDA $DC00                       ; $13A3
        AND $DC01                       ; $13A6
        EOR #$FF                        ; $13A9
        BEQ L_13AE                      ; $13AB
L_13AD:
        RTS                             ; $13AD
L_13AE:
        LDX #$FF                        ; $13AE
        STX $DC03                       ; $13B0
        STX $010A                       ; $13B3
        INX                             ; $13B6
        STX $010B                       ; $13B7
        STX $DC01                       ; $13BA
        LDA $DC00                       ; $13BD
        CMP #$FF                        ; $13C0
        BEQ L_13F5                      ; $13C2
        STA $010D                       ; $13C4
        STX $DC03                       ; $13C7
        DEX                             ; $13CA
        STX $DC02                       ; $13CB
        LDA #$FD                        ; $13CE
        STA $DC00                       ; $13D0
        LDA $DC01                       ; $13D3
        BPL L_13E4                      ; $13D6
        LDA #$BF                      ; $13D8
        STA $DC00                       ; $13DA
        LDA $DC01                       ; $13DD
        AND #$10                        ; $13E0
        BNE L_13E7                      ; $13E2
L_13E4:
        INC $010B                       ; $13E4
L_13E7:
        LDA $010E                       ; $13E7
        BNE L_13F2                      ; $13EA
        JSR scan_keyboard               ; $13EC
        JMP L_13F5                      ; $13EF
L_13F2:
        JSR scan_stick_only             ; $13F2
L_13F5:
        LDA #$00                        ; $13F5
        STA $DC02                       ; $13F7
        STA $DC03                       ; $13FA
        LDA $DC00                       ; $13FD
        AND $DC01                       ; $1400
        EOR #$FF                        ; $1403
        BEQ L_1408                      ; $1405
        RTS                             ; $1407
L_1408:
        LDA $0109                       ; $1408
        AND #$7F                        ; $140B
        CMP $010A                       ; $140D
        BEQ L_142B                      ; $1410
        LDA $010B                       ; $1412
        BEQ L_1419                      ; $1415
        LDA #$80                        ; $1417
L_1419:
        ORA $010A                       ; $1419
        STA $0109                       ; $141C
        LDA $010A                       ; $141F
        CMP #$FF                        ; $1422
        BEQ L_142B                      ; $1424
        LDA #$01                        ; $1426
        STA $010C                       ; $1428
L_142B:
        RTS                             ; $142B
scan_keyboard:
        LDX #$07                        ; $142C
L_142E:
        ASL $010D                       ; $142E
        BCC L_143A                      ; $1431
L_1433:
        DEX                             ; $1433
        BPL L_142E                      ; $1434
        STX $010A                       ; $1436
        RTS                             ; $1439
L_143A:
        LDY #$07                        ; $143A
L_143C:
        LDA $1494,X                     ; $143C
        STA $DC00                       ; $143F
        LDA $DC01                       ; $1442
        ORA $1494,Y                     ; $1445
        EOR #$FF                        ; $1448
        BNE L_1451                      ; $144A
L_144C:
        DEY                             ; $144C
        BPL L_143C                      ; $144D
        BMI L_1433                      ; $144F
L_1451:
        TYA                             ; $1451
        CLC                             ; $1452
        ADC $149C,X                     ; $1453
        CMP #$0F                        ; $1456
        BEQ L_144C                      ; $1458
        CMP #$34                        ; $145A
        BEQ L_144C                      ; $145C
        STA $010A                       ; $145E
        RTS                             ; $1461
scan_stick_only:
        LDY #$00                        ; $1462
L_1464:
        LDA $1488,Y                     ; $1464
        STA $DC00                       ; $1467
        LDA $DC01                       ; $146A
        INY                             ; $146D
        CMP $1488,Y                     ; $146E
        BEQ L_147E                      ; $1471
        INY                             ; $1473
        CPY #$08                        ; $1474
        BNE L_1464                      ; $1476
        LDA #$6F                        ; $1478
        STA $010A                       ; $147A
        RTS                             ; $147D
L_147E:
        TYA                             ; $147E
        LSR A                           ; $147F
        TAY                             ; $1480
        LDA $1490,Y                     ; $1481
        STA $010A                       ; $1484
        RTS                             ; $1487

; $1488  28 bytes of data between routines

; between_rooms points the raster here, at line $63. $D022 steps through
; dark grey, grey, light grey, and white. That flash is this screen's color
; cycle. flicker_lamps is the routine that changes the lamp glyphs.
between_raster:
        LDX #$6D                        ; $14A4
        LDA #$6E                        ; $14A6
L_14A8:
        CMP $D012                       ; $14A8
        BEQ L_14B7                      ; $14AB
        CPX $D012                       ; $14AD
        BNE L_14A8                      ; $14B0
        LDX #$0A                        ; $14B2
L_14B4:
        DEX                             ; $14B4
        BNE L_14B4                      ; $14B5
L_14B7:
        LDA #$FB                        ; $14B7
        STA $D022                       ; $14B9
        JSR raster_wait                 ; $14BC
        JSR raster_wait                 ; $14BF
        LDA #$FC                        ; $14C2
        STA $D022                       ; $14C4
        JSR raster_wait                 ; $14C7
        JSR raster_wait                 ; $14CA
        LDA #$FF                        ; $14CD
        STA $D022                       ; $14CF
        JSR raster_wait                 ; $14D2
        JSR raster_wait                 ; $14D5
        LDA #$F1                        ; $14D8
        STA $D022                       ; $14DA
        JSR raster_wait                 ; $14DD
        LDA #$FF                        ; $14E0
        STA $D022                       ; $14E2
        JSR raster_wait                 ; $14E5
        JSR raster_wait                 ; $14E8
        LDA #$FC                        ; $14EB
        STA $D022                       ; $14ED
        JSR raster_wait                 ; $14F0
        JSR raster_wait                 ; $14F3
        LDA #$FB                        ; $14F6
        STA $D022                       ; $14F8
        LDA #$30                        ; $14FB
        STA $D018                       ; $14FD
        LDA #$18                        ; $1500
        STA $D016                       ; $1502
        LDA #$01                        ; $1505
        STA $D01A                       ; $1507
        STA $D019                       ; $150A
        LDA $D011                       ; $150D
        AND #$7F                        ; $1510
        STA $D011                       ; $1512
        LDA #$63                        ; $1515
        STA $D012                       ; $1517
        LDA $48                         ; $151A
        BEQ L_1520                      ; $151C
        DEC $48                         ; $151E
L_1520:
        PLA                             ; $1520
        TAY                             ; $1521
        PLA                             ; $1522
        TAX                             ; $1523
        PLA                             ; $1524
        RTI                             ; $1525

; Burn about ten iterations. The between-lives flash pauses on each $D022 color.
raster_wait:
        LDX #$0A                        ; $1526
L_1528:
        DEX                             ; $1528
        BNE L_1528                      ; $1529
        RTS                             ; $152B

; game_over points the raster here. Charset $8000 and multicolor stay on until
; raster $82, where game_over_late takes over. $48 still counts down.
game_over_raster:
        LDA $48                         ; $152C
        BEQ L_1532                      ; $152E
        DEC $48                         ; $1530
L_1532:
        LDA $49                         ; $1532
        BEQ L_1542                      ; $1534
        DEC $49                         ; $1536
        BNE L_1542                      ; $1538
        LDA $4A                         ; $153A
        BEQ L_1542                      ; $153C
        DEC $4A                         ; $153E
        DEC $49                         ; $1540
L_1542:
        LDA #$30                        ; $1542
        STA $D018                       ; $1544
        LDA #$18                        ; $1547
        STA $D016                       ; $1549
        JSR $E006                       ; $154C
        LDA #$74                        ; $154F
        STA $0408                       ; $1551
        LDA #$15                        ; $1554
        STA $0409                       ; $1556
        LDA #$01                        ; $1559
        STA $D01A                       ; $155B
        STA $D019                       ; $155E
        LDA $D011                       ; $1561
        AND #$7F                        ; $1564
        STA $D011                       ; $1566
        LDA #$82                        ; $1569
        STA $D012                       ; $156B
        PLA                             ; $156E
        TAY                             ; $156F
        PLA                             ; $1570
        TAX                             ; $1571
        PLA                             ; $1572
        RTI                             ; $1573

; Raster $82 on the game-over screen. Charset $8800 is the score font, and
; $D016 drops to $16, so the wide dots are off. The vector returns to
; game_over_raster at raster $FD.
game_over_late:
        LDX #$04                        ; $1574
L_1576:
        DEX                             ; $1576
        BNE L_1576                      ; $1577
        LDA #$32                        ; $1579
        STA $D018                       ; $157B
        LDA #$16                        ; $157E
        STA $D016                       ; $1580
        LDA #$2C                        ; $1583
        STA $0408                       ; $1585
        LDA #$15                        ; $1588
        STA $0409                       ; $158A
        LDA #$01                        ; $158D
        STA $D01A                       ; $158F
        STA $D019                       ; $1592
        LDA $D011                       ; $1595
        AND #$7F                        ; $1598
        STA $D011                       ; $159A
        LDA #$FD                        ; $159D
        STA $D012                       ; $159F
        PLA                             ; $15A2
        TAY                             ; $15A3
        PLA                             ; $15A4
        TAX                             ; $15A5
        PLA                             ; $15A6
        RTI                             ; $15A7

; raster_irq calls this once per frame, from the interrupt at raster $FB.
; The color splits do not call it, so $48 loses one count per frame. frame
; waits until that count is 0.
; While $82 is negative, the twelve band colors copied to $0153 are replaced
; from the table at $47B4: (byte EOR $83) AND $84. $84 is $F7, which clears
; bit 3 of the color. $83 is the slow counter in $80. flicker_lamps is the
; routine that rewrites the lamp glyph bytes.
; In play, the stick nibble is stored at $98 and the fire button at $9B.
; $9B is 0 while fire is held.
irq_service:
        CLD                             ; $15A8
        LDA #$00                        ; $15A9
        STA $44                         ; $15AB
        LDX $29                         ; $15AD
        JSR $EB03                       ; $15AF
        JSR $EF73                       ; $15B2
        JSR $F073                       ; $15B5
        INC $16                         ; $15B8
        JSR $25E2                       ; $15BA
        LDA $4EEC                       ; $15BD
        BNE L_15CD                      ; $15C0
        LDA #$02                        ; $15C2
        STA $4F45                       ; $15C4
        STA $4F46                       ; $15C7
        STA $4F47                       ; $15CA
L_15CD:
        DEC $4EEC                       ; $15CD
        LDA $F2                         ; $15D0
        CMP #$38                        ; $15D2
        BNE L_15DA                      ; $15D4
        LDA #$A0                        ; $15D6
        BNE L_15EC                      ; $15D8
L_15DA:
        CMP #$3A                        ; $15DA
        BNE L_15E2                      ; $15DC
        LDA #$A8                        ; $15DE
        BNE L_15EC                      ; $15E0
L_15E2:
        CMP #$3C                        ; $15E2
        BNE L_15EA                      ; $15E4
        LDA #$B0                        ; $15E6
        BNE L_15EC                      ; $15E8
L_15EA:
        LDA #$B8                        ; $15EA
L_15EC:
        STA $7C                         ; $15EC
        LDA $1383                       ; $15EE
        BEQ L_15F7                      ; $15F1
        LDA #$00                        ; $15F3
        BEQ L_15F9                      ; $15F5
L_15F7:
        LDA #$2C                        ; $15F7
L_15F9:
        STA $8C2A                       ; $15F9
        LDA $42CA                       ; $15FC
        BNE L_1606                      ; $15FF
        LDA $42C9                       ; $1601
        BEQ L_1611                      ; $1604
L_1606:
        LDA $1384                       ; $1606
        BEQ L_160F                      ; $1609
        LDA #$00                        ; $160B
        BEQ L_1611                      ; $160D
L_160F:
        LDA #$2C                        ; $160F
L_1611:
        STA $8C4D                       ; $1611
        LDA $0104                       ; $1614
        BEQ L_1628                      ; $1617
        LDA $0105                       ; $1619
        BMI L_1628                      ; $161C
        INC $87                         ; $161E
        BNE L_1628                      ; $1620
        LDA $88                         ; $1622
        BMI L_1628                      ; $1624
        INC $88                         ; $1626
L_1628:
        INC $7F                         ; $1628
        BNE L_1645                      ; $162A
        LDA $82                         ; $162C
        BMI L_163F                      ; $162E
        INC $82                         ; $1630
        BPL L_163F                      ; $1632
        LDA #$86                        ; $1634
        STA $0105                       ; $1636
        JSR L_1A42                      ; $1639
        JSR $102B                       ; $163C
L_163F:
        INC $80                         ; $163F
        BNE L_1645                      ; $1641
        INC $81                         ; $1643
L_1645:
        LDA #$FE                        ; $1645
        LDX #$00                        ; $1647
        LDY $82                         ; $1649
        BPL L_1653                      ; $164B
        STA $82                         ; $164D
        LDX $80                         ; $164F
        LDA #$F7                        ; $1651
L_1653:
        STA $84                         ; $1653
        STX $83                         ; $1655
        LDA $82                         ; $1657
        BPL L_166A                      ; $1659
        LDX #$13                        ; $165B
L_165D:
        LDA $47B4,X                     ; $165D
        EOR $83                         ; $1660
        AND $84                         ; $1662
        STA $0153,X                     ; $1664
        DEX                             ; $1667
        BPL L_165D                      ; $1668
L_166A:
        NOP                             ; $166A
        NOP                             ; $166B
        NOP                             ; $166C
        NOP                             ; $166D
        NOP                             ; $166E
        NOP                             ; $166F
        NOP                             ; $1670
        NOP                             ; $1671
        LDA $48                         ; $1672
        BEQ L_1678                      ; $1674
        DEC $48                         ; $1676
L_1678:
        LDA $49                         ; $1678
        BEQ L_1681                      ; $167A
        DEC $49                         ; $167C
        JMP L_1689                      ; $167E
L_1681:
        LDA $4A                         ; $1681
        BEQ L_1689                      ; $1683
        DEC $4A                         ; $1685
        DEC $49                         ; $1687
L_1689:
        LDA $DC02                       ; $1689
        STA $2005                       ; $168C
        LDA $DC03                       ; $168F
        STA $2006                       ; $1692
        LDA #$00                        ; $1695
        STA $DC02                       ; $1697
        STA $DC03                       ; $169A
        LDA $0104                       ; $169D
        BEQ L_1703                      ; $16A0
        LDA $DC01                       ; $16A2
        LDX $03                         ; $16A5
        BEQ L_16AC                      ; $16A7
        LDA $DC00                       ; $16A9
L_16AC:
        AND #$0F                        ; $16AC
        LDY $1383,X                     ; $16AE
        BNE L_16BD                      ; $16B1
        PHA                             ; $16B3
        AND #$03                        ; $16B4
        TAY                             ; $16B6
        PLA                             ; $16B7
        LSR A                           ; $16B8
        LSR A                           ; $16B9
        ORA $4A85,Y                     ; $16BA
L_16BD:
        STA $98                         ; $16BD
        LDA $03                         ; $16BF
        EOR #$01                        ; $16C1
        TAX                             ; $16C3
        LDA $DC00,X                     ; $16C4
        LSR A                           ; $16C7
        LSR A                           ; $16C8
        LSR A                           ; $16C9
        LSR A                           ; $16CA
        AND #$01                        ; $16CB
        STA $9B                         ; $16CD
        LDA $42CA                       ; $16CF
        BEQ L_1703                      ; $16D2
        LDA $03                         ; $16D4
        EOR #$01                        ; $16D6
        TAX                             ; $16D8
        PHP                             ; $16D9
        LDA $DC01                       ; $16DA
        PLP                             ; $16DD
        BEQ L_16E3                      ; $16DE
        LDA $DC00                       ; $16E0
L_16E3:
        AND #$0F                        ; $16E3
        LDY $1383,X                     ; $16E5
        BNE L_16F4                      ; $16E8
        PHA                             ; $16EA
        AND #$03                        ; $16EB
        TAY                             ; $16ED
        PLA                             ; $16EE
        LSR A                           ; $16EF
        LSR A                           ; $16F0
        ORA $4A85,Y                     ; $16F1
L_16F4:
        STA $99                         ; $16F4
        LDX $03                         ; $16F6
        LDA $DC00,X                     ; $16F8
        LSR A                           ; $16FB
        LSR A                           ; $16FC
        LSR A                           ; $16FD
        LSR A                           ; $16FE
        AND #$01                        ; $16FF
        STA $9C                         ; $1701
L_1703:
        LDA $02                         ; $1703
        CMP #$32                        ; $1705
        BEQ L_1717                      ; $1707
        STA $05                         ; $1709
L_170B:
        STA ($04),Y                     ; $170B
        DEY                             ; $170D
        BNE L_170B                      ; $170E
        DEC $05                         ; $1710
        DEC $0539                       ; $1712
        BNE L_170B                      ; $1715
L_1717:
        LDA $2005                       ; $1717
        STA $DC02                       ; $171A
        LDA $2006                       ; $171D
        STA $DC03                       ; $1720
        LDA $0104                       ; $1723
        BEQ L_173B                      ; $1726
        LDA $0105                       ; $1728
        BMI L_173F                      ; $172B
        LDA $98                         ; $172D
        ORA $99                         ; $172F
        CMP #$0F                        ; $1731
        BNE L_173B                      ; $1733
        LDA $9B                         ; $1735
        AND $9C                         ; $1737
        BNE L_173F                      ; $1739
L_173B:
        LDA #$00                        ; $173B
        STA $82                         ; $173D
L_173F:
        LDA $7A                         ; $173F
        BNE L_179D                      ; $1741
        LDA $0105                       ; $1743
        BMI L_179D                      ; $1746
        LDA $0104                       ; $1748
        BEQ L_179D                      ; $174B
        LDA $42CA                       ; $174D
        BEQ L_179D                      ; $1750
        LDA $99                         ; $1752
        CMP #$0F                        ; $1754
        BNE L_1797                      ; $1756
        LDA $9C                         ; $1758
        BEQ L_1797                      ; $175A
        INC $89                         ; $175C
        BNE L_179D                      ; $175E
        INC $8A                         ; $1760
        LDA $8A                         ; $1762
        CMP #$07                        ; $1764
        BCC L_179D                      ; $1766
        LDA #$00                        ; $1768
        STA $42CA                       ; $176A
        LDA #$20                        ; $176D
        STA $88                         ; $176F
        JSR $424F                       ; $1771
        LDA $28                         ; $1774
        CMP #$05                        ; $1776
        BCC L_177F                      ; $1778
        SBC #$05                        ; $177A
        JMP L_1781                      ; $177C
L_177F:
        LDA #$00                        ; $177F
L_1781:
        STA $28                         ; $1781
        LDA $439F                       ; $1783
        CMP #$05                        ; $1786
        BCC L_178F                      ; $1788
        SBC #$05                        ; $178A
        JMP L_1791                      ; $178C
L_178F:
        LDA #$00                        ; $178F
L_1791:
        STA $439F                       ; $1791
        JSR lives_to_bcd                ; $1794
L_1797:
        LDA #$00                        ; $1797
        STA $89                         ; $1799
        STA $8A                         ; $179B
L_179D:
        RTS                             ; $179D


; For each active actor, release the pose hold and then choose the picture. X = 0 is Bruce.
step_actors:
        JSR enemy_turn                      ; $179E
        LDA $0104                       ; $17A1
        BNE L_17BC                      ; $17A4
        LDY #$00                        ; $17A6
        LDA ($1D),Y                     ; $17A8
        AND #$0F                        ; $17AA
        STA $98                         ; $17AC
        LDA ($1D),Y                     ; $17AE
        LSR A                           ; $17B0
        LSR A                           ; $17B1
        LSR A                           ; $17B2
        LSR A                           ; $17B3
        STA $9B                         ; $17B4
        INC $1D                         ; $17B6
        BNE L_17BC                      ; $17B8
        INC $1E                         ; $17BA
L_17BC:
        LDX #$02                        ; $17BC
L_17BE:
        LDA active,X                    ; $17BE
        AND #$01                        ; $17C0
        BEQ L_17DE                      ; $17C2
        LDA $29                         ; $17C4
        CMP #$13                        ; $17C6
        BNE L_17D8                      ; $17C8
        LDA $88                         ; $17CA
        CMP #$02                        ; $17CC
        BCS L_17D4                      ; $17CE
        LDA #$0E                        ; $17D0
        BNE L_17D6                      ; $17D2
L_17D4:
        LDA #$0F                        ; $17D4
L_17D6:
        STA $98                         ; $17D6
L_17D8:
        JSR release_hold                      ; $17D8
        JSR choose_pose                      ; $17DB
L_17DE:
        DEX                             ; $17DE
        BPL L_17BE                      ; $17DF
        RTS                             ; $17E1
; A selects a string from the table at $43BE. Copies until the $A0 terminator. The plain
; byte goes to ($04), the same byte with bit 7 set goes to ($08).
print_string:
        PHA                             ; $17E2
        JSR point_at_row                      ; $17E3
        PLA                             ; $17E6
        ASL A                           ; $17E7
        TAX                             ; $17E8
        STY $06                         ; $17E9
        LDA $43BE,X                     ; $17EB
        SEC                             ; $17EE
        SBC $06                         ; $17EF
        STA $06                         ; $17F1
        LDA $43BF,X                     ; $17F3
        SBC #$00                        ; $17F6
        STA $07                         ; $17F8
L_17FA:
        LDA ($06),Y                     ; $17FA
        SEC                             ; $17FC
        SBC #$20                        ; $17FD
        CMP #$80                        ; $17FF
        BEQ L_1817                      ; $1801
        AND #$7F                        ; $1803
        STA ($04),Y                     ; $1805
        ORA #$80                        ; $1807
        STA ($08),Y                     ; $1809
        INY                             ; $180B
        BNE L_17FA                      ; $180C
        INC $05                         ; $180E
        INC $07                         ; $1810
        INC $09                         ; $1812
        JMP L_17FA                      ; $1814
L_1817:
        RTS                             ; $1817
; Silence the SID. If $0104 is 0, reopen the room. Otherwise one life is spent and the
; between-lives screen runs.
lose_life:
        JSR silence_sid                      ; $1818
        LDA $0104                       ; $181B
        BNE L_1826                      ; $181E
        STA $09BB                       ; $1820
        JMP open_room                   ; $1823
L_1826:
        DEC $28                         ; $1826
        BMI game_over                      ; $1828
; Count $28 down. Two-player mode swaps the saved block at $439F before the next room.
between_lives:
        LDA #$24                        ; $182A
        STA $D1                         ; $182C
        STA $D2                         ; $182E
        STA $D3                         ; $1830
        LDA #$00                        ; $1832
        STA $8B                         ; $1834
        STA $8C                         ; $1836
        JSR L_423F                      ; $1838
        LDA $42C9                       ; $183B
        BEQ L_184B                      ; $183E
        LDA $439F                       ; $1840
        BMI L_184B                      ; $1843
        LDA $03                         ; $1845
        EOR #$01                        ; $1847
        STA $03                         ; $1849
L_184B:
        JSR between_rooms                      ; $184B
        LDA $D011                       ; $184E
        AND #$EF                        ; $1851
        STA $D011                       ; $1853
        LDA $42C9                       ; $1856
        BEQ L_1886                      ; $1859
        LDA $439F                       ; $185B
        BMI L_1886                      ; $185E
        LDX #$1B                        ; $1860
L_1862:
        LDA $28,X                       ; $1862
        LDY $439F,X                     ; $1864
        STA $439F,X                     ; $1867
        TYA                             ; $186A
        STA $28,X                       ; $186B
        DEX                             ; $186D
        BPL L_1862                      ; $186E
        JSR L_0FED                      ; $1870
        LDX #$02                        ; $1873
L_1875:
        LDA $4392,X                     ; $1875
        PHA                             ; $1878
        LDA $4395,X                     ; $1879
        STA $4392,X                     ; $187C
        PLA                             ; $187F
        STA $4395,X                     ; $1880
        DEX                             ; $1883
        BPL L_1875                      ; $1884
L_1886:
        JSR init_hardware                      ; $1886
        LDX $29                         ; $1889
        JMP enter_room_at                      ; $188B
; Lives are gone. Blank the screen, print the score digits, wait, then open the room again.
game_over:
        JSR L_423F                      ; $188E
        JSR clear_playfield                      ; $1891
        SEI                             ; $1894
        LDA $D011                       ; $1895
        AND #$EF                        ; $1898
        STA $D011                       ; $189A
        LDA #$00                        ; $189D
        STA $D015                       ; $189F
        LDA #$2C                        ; $18A2  low byte of game_over_raster
        STA $0408                       ; $18A4
        LDA #$15                        ; $18A7
        STA $0409                       ; $18A9
        LDA #$01                        ; $18AC
        STA $D01A                       ; $18AE
        STA $D019                       ; $18B1
        LDA $D011                       ; $18B4
        AND #$7F                        ; $18B7
        STA $D011                       ; $18B9
        LDA #$FB                        ; $18BC
        STA $D012                       ; $18BE
        LDA #$06                        ; $18C1
        STA $D020                       ; $18C3
        STA $D021                       ; $18C6
        LDA #$0F                        ; $18C9
        STA $D022                       ; $18CB
        STA $D023                       ; $18CE
        LDX #$08                        ; $18D1
        STX $D016                       ; $18D3
        JSR fill_color_ram                      ; $18D6
        LDX #$00                        ; $18D9
L_18DB:
        LDA #$F1                        ; $18DB
        STA $D9E0,X                     ; $18DD
        STA $DAE0,X                     ; $18E0
        LDA #$DC                        ; $18E3
        STA $8C00,X                     ; $18E5
        STA $8C90,X                     ; $18E8
        INX                             ; $18EB
        BNE L_18DB                      ; $18EC
        LDX #$11                        ; $18EE
L_18F0:
        LDA $42CD,X                     ; $18F0
        ORA #$80                        ; $18F3
        STA $8C33,X                     ; $18F5
        LDA $42DF,X                     ; $18F8
        ORA #$80                        ; $18FB
        STA $8C5B,X                     ; $18FD
        DEX                             ; $1900
        BPL L_18F0                      ; $1901
        LDA $03                         ; $1903
        ASL A                           ; $1905
        TAX                             ; $1906
        LDY #$00                        ; $1907
L_1909:
        LDA $4A7D,X                     ; $1909
        STA $4A6B,Y                     ; $190C
        LDA $4A81,X                     ; $190F
        STA $4A7B,Y                     ; $1912
        DEY                             ; $1915
        BPL L_1909                      ; $1916
        LDX #$0F                        ; $1918
L_191A:
        LDA $4A5D,X                     ; $191A
        ORA #$80                        ; $191D
        STA $8CD4,X                     ; $191F
        LDA $4A6D,X                     ; $1922
        ORA #$80                        ; $1925
        STA $8CFC,X                     ; $1927
        DEX                             ; $192A
        BPL L_191A                      ; $192B
        JSR stash_scores                      ; $192D
        LDA #$4E                        ; $1930
        STA $04                         ; $1932
        LDA #$76                        ; $1934
        STA $06                         ; $1936
        LDA #$8D                        ; $1938
        STA $05                         ; $193A
        STA $07                         ; $193C
        LDY #$00                        ; $193E
        LDX #$01                        ; $1940
L_1942:
        LDA $439B,X                     ; $1942
        LSR A                           ; $1945
        LSR A                           ; $1946
        LSR A                           ; $1947
        LSR A                           ; $1948
        JSR draw_digit                      ; $1949
        INY                             ; $194C
        LDA $439B,X                     ; $194D
        AND #$0F                        ; $1950
        JSR draw_digit                      ; $1952
        INY                             ; $1955
        INX                             ; $1956
        CPX #$04                        ; $1957
        BCC L_1942                      ; $1959
        JSR show_screen                      ; $195B
        NOP                             ; $195E
        NOP                             ; $195F
        NOP                             ; $1960
        NOP                             ; $1961
        NOP                             ; $1962
        CLI                             ; $1963
        LDA #$FE                        ; $1964
        STA $48                         ; $1966
L_1968:
        LDA $48                         ; $1968
        BNE L_1968                      ; $196A
        LDA $42C9                       ; $196C
        BEQ L_1979                      ; $196F
        LDA $439F                       ; $1971
        BMI L_1979                      ; $1974
        JMP between_lives                      ; $1976
L_1979:
        LDA #$40                        ; $1979
        STA $48                         ; $197B
L_197D:
        LDA $48                         ; $197D
        BNE L_197D                      ; $197F
        JMP open_room                   ; $1981
; A is one hex digit. It becomes four screen bytes, two on the top row ($04) and two on the
; row under it ($06).
draw_digit:
        AND #$0F                        ; $1984
        ASL A                           ; $1986
        ASL A                           ; $1987
        ADC #$34                        ; $1988
        ORA #$80                        ; $198A
        STA ($04),Y                     ; $198C
        CLC                             ; $198E
        ADC #$01                        ; $198F
        STA ($06),Y                     ; $1991
        CLC                             ; $1993
        ADC #$01                        ; $1994
        INY                             ; $1996
        STA ($04),Y                     ; $1997
        CLC                             ; $1999
        ADC #$01                        ; $199A
        STA ($06),Y                     ; $199C
        RTS                             ; $199E
; Shift $43BC left three places into the pair $43BB/$43BC.
scale_by_eight:
        LDA #$00                        ; $199F
        STA $43BB                       ; $19A1
        CLC                             ; $19A4
        ROL $43BC                       ; $19A5
        ROL $43BB                       ; $19A8
        ROL $43BC                       ; $19AB
        ROL $43BB                       ; $19AE
        ROL $43BC                       ; $19B1
        ROL $43BB                       ; $19B4
        RTS                             ; $19B7
L_19B8:
        LDA $0104                       ; $19B8
        BNE L_19EB                      ; $19BB
        LDA $DC01                       ; $19BD
        AND #$10                        ; $19C0
        BEQ L_19DD                      ; $19C2
        LDA #$01                        ; $19C4
        STA $010E                       ; $19C6
        JSR read_stick                  ; $19C9
        LDA $010C                       ; $19CC
        BEQ L_19EB                      ; $19CF
        LDA #$00                        ; $19D1
        STA $010C                       ; $19D3
        LDA $0109                       ; $19D6
        CMP #$04                        ; $19D9
        BNE L_19EB                      ; $19DB
L_19DD:
        JSR wait_button_up                      ; $19DD
        LDA #$06                        ; $19E0
        STA $48                         ; $19E2
L_19E4:
        LDA $48                         ; $19E4
        BNE L_19E4                      ; $19E6
        JMP start                       ; $19E8
L_19EB:
        RTS                             ; $19EB
L_19EC:
        LDA $0105                       ; $19EC
        PHA                             ; $19EF
        AND #$80                        ; $19F0
        STA $0105                       ; $19F2
        PLA                             ; $19F5
        AND #$06                        ; $19F6
        BEQ L_1A07                      ; $19F8
        SEC                             ; $19FA
        SBC #$01                        ; $19FB
        ORA $0105                       ; $19FD
        STA $0105                       ; $1A00
        LDA #$FF                        ; $1A03
        STA $4B                         ; $1A05
L_1A07:
        RTS                             ; $1A07
L_1A08:
        LDA $0105                       ; $1A08
        AND #$06                        ; $1A0B
        BNE L_1A51                      ; $1A0D
        LDA $0105                       ; $1A0F
        BPL L_1A51                      ; $1A12
        LDA $03                         ; $1A14
        EOR #$01                        ; $1A16
        AND #$01                        ; $1A18
        TAX                             ; $1A1A
        LDA $DC00,X                     ; $1A1B
        AND #$10                        ; $1A1E
        BNE L_1A51                      ; $1A20
L_1A22:
        LDA $0105                       ; $1A22
        EOR #$80                        ; $1A25
        ORA #$06                        ; $1A27
        STA $0105                       ; $1A29
        BMI L_1A42                      ; $1A2C
        JSR L_1BB6                      ; $1A2E
        LDA $82                         ; $1A31
        BPL L_1A38                      ; $1A33
        JSR L_103B                      ; $1A35
L_1A38:
        LDA #$00                        ; $1A38
        STA $82                         ; $1A3A
        JSR L_1ABB                      ; $1A3C
        JMP L_31C2                      ; $1A3F
L_1A42:
        LDA #$11                        ; $1A42
        STA $43BD                       ; $1A44
        JSR timed_caption                      ; $1A47
        LDA #$51                        ; $1A4A
        STA $82                         ; $1A4C
        JMP L_1BA4                      ; $1A4E
L_1A51:
        RTS                             ; $1A51
        .byte $A2, $02, $D0, $05

; Clear 13 character rows, using the pointers built at startup.
clear_playfield:
        LDX #$00                        ; $1A56
        STX $0100                       ; $1A58
L_1A5B:
        JSR point_at_row                      ; $1A5B
        JSR clear_row                      ; $1A5E
        INX                             ; $1A61
        CPX #$0D                        ; $1A62
        BCC L_1A5B                      ; $1A64
        RTS                             ; $1A66
; X is a row number. ($04) and ($08) become that row's two pointers. If $0100 is set, ($08)
; is forced to $CE00.
point_at_row:
        LDA $4338,X                     ; $1A67
        STA $04                         ; $1A6A
        LDA $434C,X                     ; $1A6C
        STA $05                         ; $1A6F
        LDA $4360,X                     ; $1A71
        STA $08                         ; $1A74
        LDA $4374,X                     ; $1A76
        STA $09                         ; $1A79
        LDA $0100                       ; $1A7B
        BNE L_1A81                      ; $1A7E
        RTS                             ; $1A80
L_1A81:
        LDA #$00                        ; $1A81
        STA $08                         ; $1A83
        LDA #$CE                      ; $1A85
        STA $09                         ; $1A87
        RTS                             ; $1A89
; Write 40 zero bytes through ($04) and ($08).
clear_row:
        LDY #$27                        ; $1A8A
        LDA #$00                        ; $1A8C
L_1A8E:
        STA ($04),Y                     ; $1A8E
        STA ($08),Y                     ; $1A90
        DEY                             ; $1A92
        BPL L_1A8E                      ; $1A93
        RTS                             ; $1A95
; $43BD counts down. At $10 and at 0 a caption string is printed on row $2C.
timed_caption:
        DEC $43BD                       ; $1A96
        BNE L_1AA8                      ; $1A99
        LDA #$20                        ; $1A9B
        STA $43BD                       ; $1A9D
        LDA #$00                        ; $1AA0
        TAX                             ; $1AA2
        LDY #$2C                        ; $1AA3
        JMP print_string                      ; $1AA5
L_1AA8:
        LDA $43BD                       ; $1AA8
        CMP #$10                        ; $1AAB
        BNE L_1ABA                      ; $1AAD
        LDX $03                         ; $1AAF
        INX                             ; $1AB1
        TXA                             ; $1AB2
        LDX #$00                        ; $1AB3
        LDY #$2C                        ; $1AB5
        JMP print_string                      ; $1AB7
L_1ABA:
        RTS                             ; $1ABA
L_1ABB:
        LDA #$03                        ; $1ABB
        STA $0100                       ; $1ABD
        LDX #$00                        ; $1AC0
        LDY #$38                        ; $1AC2
        JMP L_1BAD                      ; $1AC4
L_1AC7:
        LDA #$05                        ; $1AC7
        LDX #$00                        ; $1AC9
        LDY #$44                        ; $1ACB
        JMP print_string                      ; $1ACD

; $1AD0  29 bytes of data between routines

; Divide the life counter $28 by ten. The tens go in the high nibble and the remainder in
; the low nibble, then the digits are drawn.
lives_to_bcd:
        LDA #$00                        ; $1AED
        STA $439B                       ; $1AEF
        LDA $28                         ; $1AF2
        STA $4398                       ; $1AF4
L_1AF7:
        SEC                             ; $1AF7
        SBC #$0A                        ; $1AF8
        BCC L_1B02                      ; $1AFA
        INC $439B                       ; $1AFC
        JMP L_1AF7                      ; $1AFF
L_1B02:
        CLC                             ; $1B02
        ADC #$0A                        ; $1B03
        STA $4398                       ; $1B05
        LDA $439B                       ; $1B08
        ASL A                           ; $1B0B
        ASL A                           ; $1B0C
        ASL A                           ; $1B0D
        ASL A                           ; $1B0E
        ORA $4398                       ; $1B0F
        LDY #$4A                        ; $1B12
        JMP draw_bcd_digits                      ; $1B14
; Draw both nibbles of A at screen column Y.
draw_split_digits:
        PHA                             ; $1B17
        JSR high_nibble                      ; $1B18
        ORA #$10                        ; $1B1B
        STA $8C00,Y                     ; $1B1D
        PLA                             ; $1B20
        AND #$0F                        ; $1B21
        ORA #$10                        ; $1B23
        STA $8C01,Y                     ; $1B25
        RTS                             ; $1B28
; Keys during play. $04 jumps back to the cold start. $0D toggles $1385. $3C and $1F are
; the other two.
in_game_key:
        LDA #$01                        ; $1B29
        STA $010E                       ; $1B2B
        JSR read_stick                  ; $1B2E
        LDA $010C                       ; $1B31
        BNE L_1B37                      ; $1B34
        RTS                             ; $1B36
L_1B37:
        LDA #$00                        ; $1B37
        STA $010C                       ; $1B39
        LDA $0109                       ; $1B3C
        AND #$7F                        ; $1B3F
        STA $4B                         ; $1B41
        CMP #$04                        ; $1B43
        BNE L_1B4A                      ; $1B45
        JMP start                       ; $1B47
L_1B4A:
        CMP #$0D                        ; $1B4A
        BNE L_1B5C                      ; $1B4C
        LDA $1385                       ; $1B4E
        EOR #$01                        ; $1B51
        STA $1385                       ; $1B53
        LDA #$00                        ; $1B56
        STA $09BB                       ; $1B58
        RTS                             ; $1B5B
L_1B5C:
        CMP #$3C                        ; $1B5C
        BEQ L_1B65                      ; $1B5E
        CMP #$1F                        ; $1B60
        BEQ L_1B68                      ; $1B62
        RTS                             ; $1B64
L_1B65:
        JMP L_1A22                      ; $1B65
L_1B68:
        LDA #$20                        ; $1B68
        CMP $8C00                       ; $1B6A
        BNE L_1B7B                      ; $1B6D
        LDX #$27                        ; $1B6F
L_1B71:
        LDA $4707,X                     ; $1B71
        STA $8C00,X                     ; $1B74
        DEX                             ; $1B77
        BPL L_1B71                      ; $1B78
        RTS                             ; $1B7A
L_1B7B:
        LDX #$27                        ; $1B7B
L_1B7D:
        STA $8C00,X                     ; $1B7D
        DEX                             ; $1B80
        BPL L_1B7D                      ; $1B81
        RTS                             ; $1B83
; Draw both digits of A at column Y. A value past 9 is moved to the later glyphs.
draw_bcd_digits:
        PHA                             ; $1B84
        JSR high_nibble                      ; $1B85
        ORA #$10                        ; $1B88
        CMP #$1A                        ; $1B8A
        BCC L_1B91                      ; $1B8C
        CLC                             ; $1B8E
        ADC #$07                        ; $1B8F
L_1B91:
        STA $8C00,Y                     ; $1B91
        PLA                             ; $1B94
        AND #$0F                        ; $1B95
        ORA #$10                        ; $1B97
        CMP #$1A                        ; $1B99
        BCC L_1BA0                      ; $1B9B
        CLC                             ; $1B9D
        ADC #$07                        ; $1B9E
L_1BA0:
        STA $8C01,Y                     ; $1BA0
        RTS                             ; $1BA3
L_1BA4:
        LDA #$0B                        ; $1BA4
        STA $0100                       ; $1BA6
        LDX #$01                        ; $1BA9
        LDY #$0F                        ; $1BAB
L_1BAD:
        JSR print_string                      ; $1BAD
        LDA #$00                        ; $1BB0
        STA $0100                       ; $1BB2
        RTS                             ; $1BB5
L_1BB6:
        LDA #$0C                        ; $1BB6
        STA $0100                       ; $1BB8
        LDX #$01                        ; $1BBB
        LDY #$0F                        ; $1BBD
        JMP L_1BAD                      ; $1BBF
; One frame. flicker_lamps runs first, then the overlap, then scan_specials,
; then the three actors. Wait until the raster count in $48 reaches 0.
; irq_service decrements $48 once per frame, from the interrupt at raster $FB.
frame:
        LDA #$03                        ; $1BC2
        STA $48                         ; $1BC4
        JSR L_19B8                      ; $1BC6
        LDA $0104                       ; $1BC9
        BEQ L_1BDF                      ; $1BCC
        JSR in_game_key                      ; $1BCE
        JSR L_19EC                      ; $1BD1
        JSR L_1A08                      ; $1BD4
        LDA $0105                       ; $1BD7
        BPL L_1BDF                      ; $1BDA
        JMP wait_for_tick                      ; $1BDC
L_1BDF:
        LDA $D2                         ; $1BDF
        AND #$BF                      ; $1BE1
        STA $D2                         ; $1BE3
        LDA $D3                         ; $1BE5
        AND #$87                        ; $1BE7
        STA $D3                         ; $1BE9
        JSR timed_caption                      ; $1BEB
        JSR flicker_lamps                      ; $1BEE
        LDA $7A                         ; $1BF1
        BNE L_1BFE                      ; $1BF3
        JSR L_0DA4                      ; $1BF5
        JSR L_0D0F                      ; $1BF8
        JSR L_0DB1                      ; $1BFB
; $7B flips every logic frame. The room hook sees 1 on the skipped frame
; and 0 on the frame it runs, so it advances every other tick.
L_1BFE:
        LDA $7B                         ; $1BFE
        BNE L_1C08                      ; $1C00
        LDA #$01                        ; $1C02
        STA $7B                         ; $1C04
        BNE L_1C0A                      ; $1C06
L_1C08:
        DEC $7B                         ; $1C08
L_1C0A:
        JSR L_3A6F                      ; $1C0A
        JSR overlap_actors              ; $1C0D
        JSR scan_specials                      ; $1C10
        LDA $CF7A                       ; $1C13
        EOR $CF9A                       ; $1C16
        STA $2007                       ; Folded into the byte score_blip tests.
        LDX $29                         ; $1C1C
        LDA $4C9E,X                     ; $1C1E
        BNE L_1C26                      ; $1C21
        JSR read_sprite_hits                      ; $1C23
L_1C26:
        JSR refresh_top                      ; $1C26
        JSR step_actors                      ; $1C29
        JSR blit_sprites                      ; $1C2C
        JSR check_bruce_down                      ; $1C2F
; The raster interrupt decrements $48. Spin until that count is 0, then start the next
; frame.
wait_for_tick:
        LDA $48                         ; $1C32
        BNE wait_for_tick                      ; $1C34
        JMP frame                      ; $1C36
; xpos and ypos of actor X, shifted down to a character cell in $E6,X and $E9,X.
; Room $0D adds 4 to ypos before the row shift only. The edge test at $30D4
; still sees the real ypos. scan_specials uses this same pair of cells.
cell_of_actor:
        LDA xpos,X                      ; $1C39
        SEC                             ; $1C3B
        SBC #$0C                        ; $1C3C
        LSR A                           ; $1C3E
        LSR A                           ; $1C3F
        STA $E6,X                       ; $1C40
        LDA $29                         ; $1C42
        CMP #$0D                        ; $1C44
        BNE L_1C4F                      ; $1C46
        LDA ypos,X                      ; $1C48
        CLC                             ; $1C4A
        ADC #$04                        ; $1C4B
        BNE L_1C51                      ; $1C4D
L_1C4F:
        LDA ypos,X                      ; $1C4F
L_1C51:
        SEC                             ; $1C51
        SBC #$0C                        ; $1C52
        LSR A                           ; $1C54
        LSR A                           ; $1C55
        LSR A                           ; $1C56
        LSR A                           ; $1C57
        STA $E9,X                       ; $1C58
        RTS                             ; $1C5A
; The pose holds for $0106,X frames. While that count is running, the rest of the actor is
; skipped.
release_hold:
        LDA #$08                        ; $1C5B
        STA $86                         ; $1C5D
        LDA $0106,X                     ; $1C5F
        BEQ L_1C68                      ; $1C62
        DEC $0106,X                     ; $1C64
        RTS                             ; $1C67
L_1C68:
        STA $4FDB,X                     ; $1C68
        LDA $4F57,X                     ; $1C6B
        ORA $4F5A,X                     ; $1C6E
        ORA $4F5D,X                     ; $1C71
        ORA $4F60,X                     ; $1C74
        BEQ L_1C8F                      ; $1C77
        LDA #$27                        ; $1C79
        STA $4E                         ; $1C7B
        TXA                             ; $1C7D
        CLC                             ; $1C7E
        ADC #$75                        ; $1C7F
        TAY                             ; $1C81
L_1C82:
        LDA $4F57,Y                     ; $1C82
        STA $4F63,Y                     ; $1C85
        DEY                             ; $1C88
        DEY                             ; $1C89
        DEY                             ; $1C8A
        DEC $4E                         ; $1C8B
        BPL L_1C82                      ; $1C8D
L_1C8F:
        LDA #$00                        ; $1C8F
        STA $4F57,X                     ; $1C91
        STA $4F5A,X                     ; $1C94
        STA $4F5D,X                     ; $1C97
        STA $4F60,X                     ; $1C9A
        LDA state,X                     ; $1C9D
        BEQ L_1CAD                      ; $1C9F
        CMP $C2,X                       ; $1CA1
        BNE L_1CAD                      ; $1CA3
        LDA $CB,X                       ; $1CA5
        BEQ L_1CB1                      ; $1CA7
        DEC $CB,X                       ; $1CA9
        BPL L_1CB1                      ; $1CAB
L_1CAD:
        LDA #$06                        ; $1CAD
        STA $CB,X                       ; $1CAF
L_1CB1:
        LDA state,X                     ; $1CB1
        STA $C2,X                       ; $1CB3
        CMP #$19                        ; $1CB5
        BNE L_1CBC                      ; $1CB7
        JMP L_1D77                      ; $1CB9
L_1CBC:
        CMP #$18                        ; $1CBC
        BNE push_out                    ; $1CBE
        LDA timer,X                     ; $1CC0
        BEQ push_out                    ; $1CC2
        JMP L_24B7                      ; $1CC4

; Bit 3 of hit,X is the solid flag. While it is set, step back out: down, then up, then
; right, then left.
push_out:
        LDA hit,X                       ; $1CC7
        AND #$08                        ; Bit 3, the solid flag.
        BNE L_1CD0                      ; $1CCB
        JMP actor_tick                  ; $1CCD
L_1CD0:
        LDA #$00                        ; $1CD0
        STA $C8,X                       ; $1CD2
L_1CD4:
        LDA #$01                        ; $1CD4
        STA $85                         ; $1CD6
L_1CD8:
        DEC $4F63,X                     ; $1CD8
        BMI L_1CE5                      ; $1CDB
        LSR $85                         ; $1CDD
        JSR move_down                   ; $1CDF
        JMP L_1CD8                      ; $1CE2
L_1CE5:
        DEC $4F66,X                     ; $1CE5
        BMI L_1CF0                      ; $1CE8
        JSR move_up                     ; $1CEA
        JMP L_1CE5                      ; $1CED
L_1CF0:
        DEC $4F69,X                     ; $1CF0
        BMI L_1CFD                      ; $1CF3
        LSR $85                         ; $1CF5
        JSR move_right                  ; $1CF7
        JMP L_1CF0                      ; $1CFA
L_1CFD:
        DEC $4F6C,X                     ; $1CFD
        BMI L_1D0A                      ; $1D00
        LSR $85                         ; $1D02
        JSR move_left                   ; $1D04
        JMP L_1CFD                      ; $1D07
L_1D0A:
        LDA #$23                        ; $1D0A
        STA $4E                         ; $1D0C
        TXA                             ; $1D0E
        TAY                             ; $1D0F
L_1D10:
        LDA $4F6F,Y                     ; $1D10
        STA $4F57,Y                     ; $1D13
        INY                             ; $1D16
        INY                             ; $1D17
        INY                             ; $1D18
        DEC $4E                         ; $1D19
        BPL L_1D10                      ; $1D1B
        LDA #$00                        ; $1D1D
        STA $0106,X                     ; $1D1F
        STA $4FC3,X                     ; $1D22
        STA $4FC6,X                     ; $1D25
        STA $4FC9,X                     ; $1D28
        STA $4FCC,X                     ; $1D2B
        STA $4FCF,X                     ; $1D2E
        STA $4FD2,X                     ; $1D31
        STA $4FD5,X                     ; $1D34
        STA $4FD8,X                     ; $1D37
        DEC $86                         ; $1D3A
        BMI L_1D66                      ; $1D3C
        LDA hit,X                       ; $1D3E
        AND #$04                        ; $1D40
        BNE L_1D66                      ; $1D42
        LDA $85                         ; $1D44
        BEQ L_1D53                      ; $1D46
        LDA $C2,X                       ; $1D48
        CMP #$1A                        ; $1D4A
        BEQ L_1D66                      ; $1D4C
        LDA #$00                        ; $1D4E
        JMP L_1C68                      ; $1D50
L_1D53:
        LDA $86                         ; $1D53
        CMP #$07                        ; $1D55
        BEQ L_1D66                      ; $1D57
        LDA facing,X                    ; $1D59
        BNE L_1D63                      ; $1D5B
        JSR move_right                  ; $1D5D
        JMP L_1D66                      ; $1D60
L_1D63:
        JSR move_left                   ; $1D63
L_1D66:
        LDA #$18                        ; $1D66
        STA state,X                     ; $1D68
        LDA #$40                        ; $1D6A
        STA timer,X                     ; $1D6C
        RTS                             ; $1D6E

; One actor, for the X register passed in. The state timer gates the scripted mover.
actor_tick:
        STA $C8,X                       ; $1D6F
        LDA #$FF                        ; $1D71
        STA $C5,X                       ; $1D73
        LDA #$00                        ; $1D75
L_1D77:
        LDY timer,X                     ; A timer of 0 skips the mover. The last count has already run.
        BEQ L_1D7E                      ; $1D79
        JMP move_for_state              ; Timer still running: perform this state's step.
L_1D7E:
        LDA $C2,X                       ; $1D7E
; The remembered state is $0E, the fall. The first airborne frame is L_1DC4:
; $B0,X becomes 2 and he drops one pixel. Later frames drop two. While the
; shifted $B0,X is still set and the upper body ($A7) shows ground, one xpos
; step is added toward his facing. Bits 0 or 2 of the full hit land him on
; that pixel.
        CMP #$0E                        ; $1D80
        BNE on_ground                   ; $1D82
        TXA                             ; $1D84
        BEQ L_1D99                      ; $1D85
        LDA hit,X                       ; $1D87
        AND #$05                        ; $1D89
        BEQ L_1DB9                      ; $1D8B
        LDA facing,X                    ; $1D8D
        CMP #$01                        ; $1D8F
        BNE L_1D96                      ; $1D91
        JMP move_right                  ; $1D93
L_1D96:
        JMP move_left                   ; $1D96
L_1D99:
        LDA $A7,X                       ; $1D99
        AND #$01                        ; $1D9B
        BEQ L_1DB3                      ; $1D9D
        LSR $B0,X                       ; $1D9F
        BEQ L_1DB9                      ; $1DA1
        LDA facing,X                    ; $1DA3
        BEQ L_1DAD                      ; $1DA5
        JSR move_right                  ; $1DA7
        JMP L_1DB9                      ; $1DAA
L_1DAD:
        JSR move_left                   ; $1DAD
        JMP L_1DB9                      ; $1DB0
L_1DB3:
        LDA hit,X                       ; $1DB3
        AND #$05                        ; $1DB5
        BNE L_1DFC                      ; $1DB7
L_1DB9:
        JSR move_down                   ; $1DB9
L_1DBC:
        JSR move_down                   ; $1DBC
        LDA #$0E                        ; $1DBF
        JMP set_state                   ; $1DC1
L_1DC4:
        LDA #$02                        ; $1DC4
        STA $B0,X                       ; $1DC6
        BNE L_1DBC                      ; $1DC8

; hit,X bits 0 and 2 mean the ground is holding him. Either one cancels the fall.
; Once he is on the ground, $98,X picks the move: $0E up, $0D down, $0B left, $07 right.
on_ground:
        LDA hit,X                       ; $1DCA
        AND #$05                        ; Bits 0 and 2. Set means do not fall.
        BEQ L_1DC4                      ; $1DCE
        LDA $98,X                       ; $1DD0
; $0E: up. Bit 2 climbs. Two calls and the fall through into move_up are three
; pixels, and $CB,X at 0 adds a fourth. A refused pixel clears the state.
; With bit 2 clear, bit 3 refuses the standing jump, and a ypos below $2E
; clears the state. Otherwise state 5 starts, with timer 15.
        CMP #$0E                        ; $0E: climb up.
        BNE L_1E13                      ; $1DD4
        LDA ypos,X                      ; $1DD6
        CMP #$2E                        ; $1DD8
        BCC L_1E0C                      ; $1DDA
        JSR score_blip                  ; $1DDC
        LDA $CB,X                       ; $1DDF
        BNE L_1DE6                      ; $1DE1
        JSR L_1DEC                      ; $1DE3
L_1DE6:
        JSR L_1DEC                      ; $1DE6
        JSR L_1DEC                      ; $1DE9
L_1DEC:
        LDA hit,X                       ; $1DEC
        AND #$04                        ; $1DEE
        BEQ L_1E08                      ; $1DF0

; One step up. ypos decreases. State becomes 1 (climb up) until the caller stores its own
; state on top.
move_up:
        LDA ypos,X                      ; $1DF2
        CMP #$2E                        ; $1DF4
        BCC L_1DFC                      ; $1DF6
        LDA $C8,X                       ; $1DF8
        AND #$01                        ; $1DFA
L_1DFC:
        BNE L_1E0C                      ; $1DFC
        DEC ypos,X                      ; $1DFE
        INC $4F57,X                     ; $1E00
        LDA #$01                        ; $1E03
        JMP set_state                   ; $1E05
L_1E08:
        LDA hit,X                       ; $1E08
        AND #$08                        ; $1E0A
L_1E0C:
        BNE stop_move                   ; $1E0C
        LDA #$05                        ; $1E0E
        JMP set_state_15                ; $1E10
L_1E13:
; $0D: down. Bit 2 climbs through move_down, three pixels, or four when $CB,X
; is 0. With bit 2 clear and bit 0 set, Bruce ducks.
        CMP #$0D                        ; $0D: climb down.
        BNE L_1E59                      ; $1E15
        LDA hit,X                       ; $1E17
        AND #$04                        ; $1E19
        BNE L_1E23                      ; $1E1B
        LDA hit,X                       ; $1E1D
        AND #$01                        ; $1E1F
        BNE L_1E49                      ; $1E21
L_1E23:
        LDA ypos,X                      ; $1E23
        CMP #$C7                        ; $1E25
        BCS stop_move                   ; $1E27
        JSR score_blip                  ; $1E29
        LDA $CB,X                       ; $1E2C
        BNE L_1E33                      ; $1E2E
        JSR move_down                   ; $1E30
L_1E33:
        JSR move_down                   ; $1E33
        JSR move_down                   ; $1E36

; One step down. ypos increases. State becomes 2 (climb down) until the caller stores its
; own state on top. A ypos of $C7 or more refuses the step and clears the state.
move_down:
        LDA ypos,X                      ; $1E39
        CMP #$C7                        ; $1E3B
        BCS stop_move                   ; $1E3D
        INC ypos,X                      ; $1E3F
        INC $4F5A,X                     ; $1E41
        LDA #$02                        ; $1E44
        JMP set_state                   ; $1E46
L_1E49:
        TXA                             ; $1E49
        BEQ L_1E4F                      ; $1E4A
        JMP L_1EFA                      ; $1E4C
L_1E4F:
        LDA facing                      ; $1E4F
        BEQ L_1E56                      ; $1E51
        JMP L_1F1D                      ; $1E53
L_1E56:
        JMP L_1EEB                      ; $1E56
L_1E59:
; $0B: left. Two calls and the fall through into move_left are two xpos steps.
; $CB,X at 0 adds a third. When the previous state is already 3, $CB,X is
; below 4, and $9B,X is 0, the step is the flying kick instead.
        CMP #$0B                        ; $0B: step left. A low pose-repeat count starts the kick.
        BNE L_1E9A                      ; $1E5B
        LDA xpos,X                      ; $1E5D
        CMP #$0D                        ; $1E5F
        BCC stop_move                   ; $1E61
        JSR score_blip                  ; $1E63
        LDA $CB,X                       ; $1E66
        BNE L_1E6D                      ; $1E68
        JSR L_1E70                      ; $1E6A
L_1E6D:
        JSR L_1E70                      ; $1E6D
L_1E70:
        LDA $C2,X                       ; $1E70
        CMP #$03                        ; $1E72
        BNE move_left                   ; $1E74
        LDA $CB,X                       ; $1E76
        CMP #$04                        ; $1E78
        BCS move_left                   ; $1E7A
        LDA $9B,X                       ; $1E7C
        BNE move_left                   ; $1E7E
        JMP begin_kick_left             ; $1E80
stop_move:
        JMP clear_state                 ; $1E83

; One step left, if xpos is still above $0D. Facing becomes 0 and the state becomes 3 until
; the caller replaces it.
move_left:
        LDA xpos,X                      ; $1E86
        CMP #$0D                        ; $1E88
        BCC stop_move                   ; $1E8A
        DEC xpos,X                      ; $1E8C
        INC $4F5D,X                     ; $1E8E
        LDA #$00                        ; $1E91
        STA facing,X                    ; $1E93
        LDA #$03                        ; $1E95
        JMP set_state                   ; $1E97
L_1E9A:
; $07: right. The same two or three xpos steps. The kick replaces the step when
; the previous state is already 4, $CB,X is below 4, and fire is held.
        CMP #$07                        ; $07: step right. A low pose-repeat count starts the kick.
        BNE L_1ED8                      ; $1E9C
        LDA xpos,X                      ; $1E9E
        CMP #$A3                        ; $1EA0
        BCS stop_move                   ; $1EA2
        JSR score_blip                  ; $1EA4
        LDA $CB,X                       ; $1EA7
        BNE L_1EAE                      ; $1EA9
        JSR L_1EB1                      ; $1EAB
L_1EAE:
        JSR L_1EB1                      ; $1EAE
L_1EB1:
        LDA $C2,X                       ; $1EB1
        CMP #$04                        ; $1EB3
        BNE move_right                  ; $1EB5
        LDA $CB,X                       ; $1EB7
        CMP #$04                        ; $1EB9
        BCS move_right                  ; $1EBB
        LDA $9B,X                       ; $1EBD
        BNE move_right                  ; $1EBF
        JMP begin_kick_right            ; $1EC1

; One step right, if xpos is still below $A3. Facing becomes 1 and the state becomes 4
; until the caller replaces it.
move_right:
        LDA xpos,X                      ; $1EC4
        CMP #$A3                        ; $1EC6
        BCS stop_move                   ; $1EC8
        INC xpos,X                      ; $1ECA
        INC $4F60,X                     ; $1ECC
        LDA #$01                        ; $1ECF
        STA facing,X                    ; $1ED1
        LDA #$04                        ; $1ED3
        JMP set_state                   ; $1ED5
L_1ED8:
; $09: down and left. A ladder clears the state. On the ground Bruce ducks,
; state $1A, facing left, unless xpos has reached $A2.
        CMP #$09                        ; $1ED8
        BNE L_1F0A                      ; $1EDA
        TXA                             ; $1EDC
        BNE L_1EFA                      ; $1EDD
        LDA hit,X                       ; $1EDF
        AND #$04                        ; $1EE1
L_1EE3:
        BNE stop_move                   ; $1EE3
        LDA hit,X                       ; $1EE5
        AND #$01                        ; $1EE7
        BEQ L_1F07                      ; $1EE9
L_1EEB:
        LDA xpos,X                      ; $1EEB
        CMP #$A2                        ; $1EED
        BCS L_1F07                      ; $1EEF
        LDA #$00                        ; $1EF1
        STA facing,X                    ; $1EF3
        LDA #$1A                        ; $1EF5
        JMP set_state                   ; $1EF7
L_1EFA:
        CMP #$01                        ; $1EFA
        BNE L_1F07                      ; $1EFC
        LDA hit,X                       ; $1EFE
        AND #$04                        ; $1F00
        BNE L_1F07                      ; $1F02
        JMP L_2244                      ; $1F04
L_1F07:
        JMP clear_state                 ; $1F07
L_1F0A:
; $05: down and right. The duck faces right unless xpos is below $10.
        CMP #$05                        ; $1F0A
        BNE L_1F2C                      ; $1F0C
        TXA                             ; $1F0E
        BNE L_1EFA                      ; $1F0F
        LDA hit,X                       ; $1F11
        AND #$04                        ; $1F13
        BNE L_1EE3                      ; $1F15
        LDA hit,X                       ; $1F17
        AND #$01                        ; $1F19
        BEQ L_1F07                      ; $1F1B
L_1F1D:
        LDA xpos,X                      ; $1F1D
        CMP #$10                        ; $1F1F
        BCC L_1F07                      ; $1F21
        LDA #$01                        ; $1F23
        STA facing,X                    ; $1F25
        LDA #$1A                        ; $1F27
        JMP set_state                   ; $1F29
L_1F2C:
        PHA                             ; $1F2C
        LDA hit,X                       ; $1F2D
        AND #$08                        ; $1F2F
        BEQ L_1F37                      ; $1F31
        PLA                             ; $1F33
        JMP try_punch                   ; $1F34
L_1F37:
        PLA                             ; $1F37
        CMP #$0A                        ; $1F38
        BNE L_1F4E                      ; $1F3A
        LDA hit,X                       ; $1F3C
        AND #$04                        ; $1F3E
        BNE L_1EE3                      ; $1F40
        LDA #$00                        ; $1F42
        STA facing,X                    ; $1F44
        JSR score_blip                  ; $1F46
        LDA #$06                        ; $1F49
        JMP set_state_15                ; $1F4B
L_1F4E:
        CMP #$06                        ; $1F4E
        BNE try_punch                   ; $1F50
        LDA hit,X                       ; $1F52
        AND #$04                        ; $1F54
        BNE L_1EE3                      ; $1F56
        LDA #$01                        ; $1F58
        STA facing,X                    ; $1F5A
        JSR score_blip                  ; $1F5C
        LDA #$07                        ; $1F5F
        JMP set_state_15                ; $1F61

; Flying kick to the left. Facing 0, state $0C, timer 12.
begin_kick_left:
        LDA #$00                        ; $1F64
        STA facing,X                    ; $1F66
        LDA #$0C                        ; $1F68
        JMP set_state_12                ; $1F6A

; Flying kick to the right. Facing 1, state $0D, timer 12.
begin_kick_right:
        LDA #$01                        ; $1F6D
        STA facing,X                    ; $1F6F
        LDA #$0D                        ; $1F71
        JMP set_state_12                ; $1F73

; Fire with no direction. State $11, then begin_punch forces the timer to 4.
try_punch:
        CMP #$0F                        ; $1F76
        BNE clear_state                 ; $1F78
        LDA $9B,X                       ; $1F7A
        BNE clear_state                 ; $1F7C
L_1F7E:
        LDA hit,X                       ; $1F7E
        AND #$04                        ; $1F80
        BNE clear_state                 ; $1F82
        LDA #$11                        ; $1F84
        JMP begin_punch                 ; $1F86

; State 0 and timer 0. If $C2,X is 0, $4FDB,X is incremented first.
; The picture hold from the state table is still stored.
clear_state:
        LDA $C2,X                       ; $1F89
        BNE L_1F90                      ; $1F8B
        INC $4FDB,X                     ; $1F8D
L_1F90:
        LDA #$00                        ; $1F90
set_state:
        STA state,X                     ; $1F92
        TAY                             ; $1F94
        LDA $4A8C,Y                     ; $1F95
        STA $0106,X                     ; $1F98
        LDA #$00                        ; $1F9B
        STA timer,X                     ; $1F9D
        RTS                             ; $1F9F

; Store A as the state and start the timer at 15. The standing jump and both sideways jumps
; use this.
set_state_15:
        STA state,X                     ; $1FA0
        TAY                             ; $1FA2
        LDA $4A8C,Y                     ; $1FA3
        STA $0106,X                     ; $1FA6
        LDA #$0F                        ; $1FA9
        STA timer,X                     ; $1FAB
        RTS                             ; $1FAD

; Store A as the state and start the timer at 12. Both flying kicks use this.
set_state_12:
        STA state,X                     ; $1FAE
        TAY                             ; $1FB0
        LDA $4A8C,Y                     ; $1FB1
        STA $0106,X                     ; $1FB4
        LDA #$0C                        ; $1FB7
        STA timer,X                     ; $1FB9
        RTS                             ; $1FBB

; A is already $11. set_state_12 records that state, then the timer is replaced with 4.
; That is the punch.
begin_punch:
        JSR set_state_12                ; $1FBC
        LDA #$04                        ; $1FBF
        STA timer,X                     ; $1FC1
        RTS                             ; $1FC3
; Play the effect for Y = $80, then add one to the score digits at $0CD9
; unless $1166 is set. $0CD9 is compared with $99, so the digits are stored in BCD.
; A second counter at $1C14 moves only when the check byte $2007 is zero.
score_blip:
        PHA                             ; $1FC4
        TXA                             ; $1FC5
        PHA                             ; $1FC6
        TYA                             ; $1FC7
        PHA                             ; $1FC8
        LDY #$80                        ; $1FC9
        JSR play_sound                  ; $1FCB
        LDA $1166                       ; $1FCE
        BNE L_1FE5                      ; $1FD1
        INC $0CD9                       ; $1FD3
        INC $0CDC                       ; $1FD6
        LDA $0CD9                       ; $1FD9
        CMP #$99                        ; $1FDC
        BCC L_1FE5                      ; $1FDE
        LDA #$9A                        ; $1FE0
        STA $1159                       ; $1FE2
L_1FE5:
        LDA $2007                       ; $1FE5
        BNE L_1FFF                      ; $1FE8
        INC $1C14                       ; $1FEA
        INC $1C17                       ; $1FED
        LDA $1C14                       ; $1FF0
        CMP #$9A                        ; $1FF3
        BCC L_1FFF                      ; $1FF5
        LDX #$04                        ; $1FF7
        LDA #$9A                        ; $1FF9
        STA $1FFE                       ; $1FFB
        CLC                             ; $1FFE
L_1FFF:
        PLA                             ; $1FFF
        TAY                             ; $2000
        PLA                             ; $2001
        TAX                             ; $2002
        PLA                             ; $2003
        RTS                             ; $2004
        .byte $00, $00, $04


; Scripted motion. Y is timer,X from before this frame's decrement. It is copied to $52,
; which the state compares use. keep_state then decrements $52 and writes it back to
; timer,X.
move_for_state:
        STY $52                         ; $2008
        LDA state,X                     ; $200A
        CMP #$05                        ; $200C
        BNE L_2048                      ; $200E
        LDA hit,X                       ; $2010
        AND #$04                        ; $2012
        BNE L_2080                      ; $2014
        CPY #$0D                        ; State 5, standing jump. The compares are on the timer in Y.
        BCS L_202E                      ; $2018
        CPY #$02                        ; $201A
        BCC L_202E                      ; $201C
        CPY #$08                        ; $201E
        BCS L_2033                      ; $2020
        LDA hit,X                       ; $2022
        AND #$01                        ; $2024
        BNE L_2080                      ; $2026
        JSR move_down                   ; Timers 2 through 7: two steps down.
        JSR move_down                   ; $202B
L_202E:
        LDA #$05                        ; $202E
        JMP keep_state                  ; $2030
L_2033:
        CPY #$0C                        ; $2033
        BNE L_203D                      ; $2035
        JSR move_up                     ; Timer 12: four steps up.
        JSR move_up                     ; $203A
L_203D:
        JSR move_up                     ; $203D
        JSR move_up                     ; $2040
        LDA #$05                        ; $2043
        JMP keep_state                  ; $2045
L_2048:
        CMP #$0C                        ; State $0C, flying kick left. One step sideways on every count.
        BNE L_208D                      ; $204A
        LDA hit,X                       ; $204C
        AND #$0C                        ; $204E
        BNE L_2080                      ; $2050
        CPY #$0C                        ; $2052
        BNE L_2061                      ; $2054
        STX $51                         ; $2056
        LDY #$84                        ; $2058
        JSR play_sound                  ; $205A
        LDX $51                         ; $205D
        LDY $52                         ; $205F
L_2061:
        CPY #$0A                        ; $2061
        BCC L_2070                      ; $2063
        JSR move_up                     ; $2065
        JSR move_left                   ; $2068
        LDA #$0C                        ; $206B
        JMP keep_state                  ; $206D
L_2070:
        CPY #$03                        ; $2070
        BCC L_207C                      ; $2072
        JSR move_left                   ; $2074
        LDA #$0C                        ; $2077
        JMP keep_state                  ; $2079
L_207C:
        LDA hit,X                       ; $207C
        AND #$01                        ; $207E
L_2080:
        BNE L_20F3                      ; $2080
        JSR move_down                   ; $2082
        JSR move_left                   ; $2085
        LDA #$0C                        ; $2088
        JMP keep_state                  ; $208A
L_208D:
        CMP #$0D                        ; State $0D, flying kick right.
        BNE L_20D2                      ; $208F
        LDA hit,X                       ; $2091
        AND #$0C                        ; $2093
        BNE L_20F3                      ; $2095
        CPY #$0C                        ; $2097
        BNE L_20A6                      ; $2099
        STX $51                         ; $209B
        LDY #$84                        ; $209D
        JSR play_sound                  ; $209F
        LDX $51                         ; $20A2
        LDY $52                         ; $20A4
L_20A6:
        CPY #$0A                        ; $20A6
        BCC L_20B5                      ; $20A8
        JSR move_up                     ; $20AA
        JSR move_right                  ; $20AD
        LDA #$0D                        ; $20B0
        JMP keep_state                  ; $20B2
L_20B5:
        CPY #$03                        ; $20B5
        BCC L_20C1                      ; $20B7
        JSR move_right                  ; $20B9
        LDA #$0D                        ; $20BC
        JMP keep_state                  ; $20BE
L_20C1:
        LDA hit,X                       ; $20C1
        AND #$01                        ; $20C3
        BNE L_20F3                      ; $20C5
        JSR move_down                   ; $20C7
        JSR move_right                  ; $20CA
        LDA #$0D                        ; $20CD
        JMP keep_state                  ; $20CF
L_20D2:
        CMP #$06                        ; State 6, sideways jump left. Two steps sideways on every count.
        BNE L_2120                      ; $20D4
        LDA hit,X                       ; $20D6
        AND #$04                        ; $20D8
        BNE L_20F3                      ; $20DA
        CPY #$0A                        ; $20DC
        BCC L_20F6                      ; $20DE
        BEQ L_20E5                      ; $20E0
        JSR move_up                     ; $20E2
L_20E5:
        JSR move_up                     ; $20E5
        JSR move_left                   ; $20E8
        JSR move_left                   ; $20EB
        LDA #$06                        ; $20EE
        JMP keep_state                  ; $20F0
L_20F3:
        JMP land                        ; $20F3
L_20F6:
        CPY #$07                        ; $20F6
        BCC L_2105                      ; $20F8
        JSR move_left                   ; $20FA
        JSR move_left                   ; $20FD
        LDA #$06                        ; $2100
        JMP keep_state                  ; $2102
L_2105:
        LDA hit,X                       ; $2105
        AND #$01                        ; $2107
        BNE L_20F3                      ; $2109
        JSR move_left                   ; $210B
        JSR move_left                   ; $210E
        CPY #$06                        ; $2111
        BEQ L_2118                      ; $2113
        JSR move_down                   ; $2115
L_2118:
        JSR move_down                   ; $2118
        LDA #$06                        ; $211B
        JMP keep_state                  ; $211D
L_2120:
        CMP #$07                        ; State 7, sideways jump right.
        BNE L_216B                      ; $2122
        LDA hit,X                       ; $2124
        AND #$04                        ; $2126
        BNE L_20F3                      ; $2128
        CPY #$0A                        ; $212A
        BCC L_2141                      ; $212C
        BEQ L_2133                      ; $212E
        JSR move_up                     ; $2130
L_2133:
        JSR move_up                     ; $2133
        JSR move_right                  ; $2136
        JSR move_right                  ; $2139
        LDA #$07                        ; $213C
        JMP keep_state                  ; $213E
L_2141:
        CPY #$07                        ; $2141
        BCC L_2150                      ; $2143
        JSR move_right                  ; $2145
        JSR move_right                  ; $2148
        LDA #$07                        ; $214B
        JMP keep_state                  ; $214D
L_2150:
        LDA hit,X                       ; $2150
        AND #$01                        ; $2152
        BNE L_20F3                      ; $2154
        JSR move_right                  ; $2156
        JSR move_right                  ; $2159
        CPY #$06                        ; $215C
        BEQ L_2163                      ; $215E
        JSR move_down                   ; $2160
L_2163:
        JSR move_down                   ; $2163
        LDA #$07                        ; $2166
        JMP keep_state                  ; $2168
L_216B:
        CMP #$19                        ; $216B
        BNE L_2180                      ; $216D
        CPX #$00                        ; $216F
        BNE L_2177                      ; $2171
        CPY #$01                        ; $2173
        BEQ L_217A                      ; $2175
L_2177:
        JMP keep_state                  ; $2177
L_217A:
        JMP lose_life                      ; $217A
L_217D:
        JMP land                        ; $217D
L_2180:
        CMP #$17                        ; $2180
        BEQ L_2187                      ; $2182
        JMP L_2235                      ; $2184
L_2187:
        LDA hit,X                       ; $2187
        AND #$04                        ; $2189
        BEQ L_217D                      ; $218B
        LDA #$01                        ; $218D
        STA $AA,X                       ; $218F
        TXA                             ; $2191
        BEQ L_21A9                      ; $2192
        LDA $42CA                       ; $2194
        BNE L_21A9                      ; $2197
        LDA facing,X                    ; $2199
        CMP #$01                        ; $219B
        BNE L_21A5                      ; $219D
        LDA #$07                        ; $219F
        STA $98,X                       ; $21A1
        BNE L_21A9                      ; $21A3
L_21A5:
        LDA #$0B                        ; $21A5
        STA $98,X                       ; $21A7
L_21A9:
        LDA $AD,X                       ; $21A9
        BEQ L_21B2                      ; $21AB
        DEC $AD,X                       ; $21AD
        JMP L_21EE                      ; $21AF
L_21B2:
        LDA $98,X                       ; $21B2
        CMP #$0E                        ; $21B4
        BNE L_21C1                      ; $21B6
        JSR move_up                     ; $21B8
        JSR move_up                     ; $21BB
        JMP L_21E5                      ; $21BE
L_21C1:
        CMP #$0D                        ; $21C1
        BNE L_21CE                      ; $21C3
        JSR move_down                   ; $21C5
        JSR move_down                   ; $21C8
        JMP L_21E5                      ; $21CB
L_21CE:
        CMP #$0B                        ; $21CE
        BNE L_21DB                      ; $21D0
        JSR move_left                   ; $21D2
        JSR move_left                   ; $21D5
        JMP L_21E5                      ; $21D8
L_21DB:
        CMP #$07                        ; $21DB
        BNE L_21EE                      ; $21DD
        JSR move_right                  ; $21DF
        JSR move_right                  ; $21E2
L_21E5:
        LSR $AA,X                       ; $21E5
        LDA #$04                        ; $21E7
        STA $AD,X                       ; $21E9
        JSR score_blip                  ; $21EB
L_21EE:
        LDA $60                         ; $21EE
        CMP #$01                        ; $21F0
        BNE L_2207                      ; $21F2
        LDA $65                         ; $21F4
        BEQ L_2201                      ; $21F6
        JSR move_down                   ; $21F8
        JSR move_down                   ; $21FB
        JMP L_2207                      ; $21FE
L_2201:
        JSR move_up                     ; $2201
        JSR move_up                     ; $2204
L_2207:
        LDA #$0F                        ; $2207
        STA $98,X                       ; $2209
        LDA #$01                        ; $220B
        STA $9B,X                       ; $220D
        INC $52                         ; $220F
        LDA $4F57,X                     ; $2211
        SEC                             ; $2214
        SBC $4F5A,X                     ; $2215
        BPL L_2228                      ; $2218
        EOR #$FF                        ; $221A
        ADC #$01                        ; $221C
        STA $4F5A,X                     ; $221E
        LDA #$00                        ; $2221
        STA $4F57,X                     ; $2223
        BEQ L_2230                      ; $2226
L_2228:
        STA $4F57,X                     ; $2228
        LDA #$00                        ; $222B
        STA $4F5A,X                     ; $222D
L_2230:
        LDA #$17                        ; $2230
        JMP keep_state                  ; $2232
L_2235:
        CMP #$10                        ; $2235
        BNE L_226A                      ; $2237
        LDA timer,X                     ; $2239
        CMP #$01                        ; $223B
        BEQ L_2244                      ; $223D
        LDA #$10                        ; $223F
        JMP keep_state                  ; $2241
L_2244:
        LDA xpos,X                      ; $2244
        CMP xpos                        ; $2246
        BCC L_2265                      ; $2248
        LDA #$00                        ; $224A
L_224C:
        STA facing,X                    ; $224C
        LDA #$12                        ; $224E
        STA state,X                     ; $2250
        LDA #$10                        ; $2252
        STA timer,X                     ; $2254
        LDA #$00                        ; $2256
        STA $0106,X                     ; $2258
        TXA                             ; $225B
        PHA                             ; $225C
        LDY #$81                        ; $225D
        JSR play_sound                  ; $225F
        PLA                             ; $2262
        TAX                             ; $2263
        RTS                             ; $2264
L_2265:
        LDA #$01                        ; $2265
        JMP L_224C                      ; $2267
L_226A:
        CMP #$11                        ; $226A
        BNE L_22BD                      ; $226C
        LDA hit,X                       ; $226E
        AND #$0C                        ; $2270
        BNE L_22C5                      ; $2272
        CPY #$08                        ; $2274
        BNE L_2283                      ; $2276
        STX $51                         ; $2278
        LDY #$85                        ; $227A
        JSR play_sound                  ; $227C
        LDX $51                         ; $227F
        LDY $52                         ; $2281
L_2283:
        CPY #$01                        ; $2283
        BEQ L_228C                      ; $2285
        LDA #$11                        ; $2287
        JMP keep_state                  ; $2289
L_228C:
        LDA #$00                        ; $228C
        STA state,X                     ; $228E
        LDA #$06                        ; $2290
        STA timer,X                     ; $2292
        LDA #$00                        ; $2294
        STA $0106,X                     ; $2296
        TXA                             ; $2299
        BNE L_22A6                      ; $229A
        JSR L_10D0                      ; $229C
        BVS L_22B5                      ; $229F
        BCS L_22B5                      ; $22A1
        LSR timer,X                     ; $22A3
        RTS                             ; $22A5
L_22A6:
        JSR L_10D0                      ; $22A6
        BVS L_22B8                      ; $22A9
        BCS L_22B2                      ; $22AB
        LDA $40                         ; $22AD
        BNE L_22B8                      ; $22AF
        RTS                             ; $22B1
L_22B2:
        LSR timer,X                     ; $22B2
        RTS                             ; $22B4
L_22B5:
        ASL timer,X                     ; $22B5
        RTS                             ; $22B7
L_22B8:
        LDA #$01                        ; $22B8
        STA timer,X                     ; $22BA
        RTS                             ; $22BC
L_22BD:
        CMP #$16                        ; $22BD
        BNE L_22F3                      ; $22BF
        LDA hit,X                       ; $22C1
        AND #$08                        ; $22C3
L_22C5:
        BEQ L_22CA                      ; $22C5
        JMP land                        ; $22C7
L_22CA:
        CPY #$0C                        ; $22CA
        BNE L_22D9                      ; $22CC
        STX $51                         ; $22CE
        LDY #$85                        ; $22D0
        JSR play_sound                  ; $22D2
        LDX $51                         ; $22D5
        LDY $52                         ; $22D7
L_22D9:
        CPY #$01                        ; $22D9
        BEQ L_22F0                      ; $22DB
        LDA facing,X                    ; $22DD
        CMP #$01                        ; $22DF
        BNE L_22E8                      ; $22E1
        INC $4F60,X                     ; $22E3
        BNE L_22EB                      ; $22E6
L_22E8:
        INC $4F5D,X                     ; $22E8
L_22EB:
        LDA #$16                        ; $22EB
        JMP keep_state                  ; $22ED
L_22F0:
        JMP L_228C                      ; $22F0
L_22F3:
        CMP #$14                        ; $22F3
        BEQ L_22FA                      ; $22F5
        JMP L_23CD                      ; $22F7
L_22FA:
        LDA hit,X                       ; $22FA
        AND #$01                        ; $22FC
        BEQ L_2372                      ; $22FE
        JSR L_2DDD                      ; $2300
        LDA $62                         ; $2303
        AND #$7F                        ; $2305
        CMP #$10                        ; $2307
        BCS L_2362                      ; $2309
        LDA facing,X                    ; $230B
        LDY $63                         ; $230D
        BMI L_2313                      ; $230F
        EOR #$01                        ; $2311
L_2313:
        CMP #$01                        ; $2313
        BEQ L_237B                      ; $2315
        TYA                             ; $2317
        AND #$7F                        ; $2318
        CMP #$21                        ; $231A
        BCC L_2327                      ; $231C
        LDA facing,X                    ; $231E
        EOR #$01                        ; $2320
        STA facing,X                    ; $2322
        JMP L_2352                      ; $2324
L_2327:
        LDA xpos                        ; $2327
        LDY $63                         ; $2329
        BPL L_2332                      ; $232B
        SEC                             ; $232D
        SBC #$0F                        ; $232E
        BNE L_2335                      ; $2330
L_2332:
        CLC                             ; $2332
        ADC #$0F                        ; $2333
L_2335:
        SEC                             ; $2335
        SBC xpos,X                      ; $2336
        BCS L_233E                      ; $2338
        EOR #$FF                        ; $233A
        ADC #$01                        ; $233C
L_233E:
        CMP #$05                        ; $233E
        BCS L_2344                      ; $2340
        LDA #$05                        ; $2342
L_2344:
        STA timer,X                     ; $2344
        LDA #$15                        ; $2346
        STA state,X                     ; $2348
        LDA #$00                        ; $234A
        STA $0106,X                     ; $234C
        STA $C8,X                       ; $234F
        RTS                             ; $2351
L_2352:
        LDA facing,X                    ; $2352
        CMP #$01                        ; $2354
        BNE L_236C                      ; $2356
        JSR L_1EAE                      ; $2358
L_235B:
        INC $52                         ; $235B
        LDA #$14                        ; $235D
        JMP keep_state                  ; $235F
L_2362:
        LDA $62                         ; $2362
        BMI L_2352                      ; $2364
        JSR L_2E1C                      ; $2366
        JMP L_228C                      ; $2369
L_236C:
        JSR L_1E6D                      ; $236C
        JMP L_235B                      ; $236F
L_2372:
        LDA hit,X                       ; $2372
        AND #$04                        ; $2374
        BNE L_2352                      ; $2376
L_2378:
        JMP L_1DC4                      ; $2378
L_237B:
        LDA active,X                    ; $237B
        AND #$81                        ; $237D
        CMP #$81                        ; $237F
        BNE L_23B8                      ; $2381
        LDA $63                         ; $2383
        AND #$7F                        ; $2385
        CMP #$05                        ; $2387
L_2389:
        BCC L_2327                      ; $2389
        CMP #$0C                        ; $238B
        BCS L_239C                      ; $238D
        JSR L_1F7E                      ; $238F
        LDA state,X                     ; $2392
        CMP #$11                        ; $2394
        BNE L_2399                      ; $2396
        RTS                             ; $2398
L_2399:
        JMP L_228C                      ; $2399
L_239C:
        CMP #$13                        ; $239C
        BCS L_2352                      ; $239E
        LDA facing,X                    ; $23A0
        CMP #$01                        ; $23A2
        BEQ L_23B4                      ; $23A4
        LDA #$0C                        ; $23A6
L_23A8:
        STA state,X                     ; $23A8
        LDA #$0C                        ; $23AA
        STA timer,X                     ; $23AC
        LDA #$00                        ; $23AE
        STA $0106,X                     ; $23B0
        RTS                             ; $23B3
L_23B4:
        LDA #$0D                        ; $23B4
        BNE L_23A8                      ; $23B6
L_23B8:
        LDA $63                         ; $23B8
        AND #$7F                        ; $23BA
        CMP #$0B                        ; $23BC
        BCS L_2352                      ; $23BE
        CMP #$05                        ; $23C0
        BCC L_2389                      ; $23C2
        LDA #$16                        ; $23C4
        STA state,X                     ; $23C6
        LDA #$13                        ; $23C8
        STA timer,X                     ; $23CA
        RTS                             ; $23CC
L_23CD:
        CMP #$15                        ; $23CD
        BNE L_2407                      ; $23CF
        LDA hit,X                       ; $23D1
        AND #$01                        ; $23D3
        BEQ L_2378                      ; $23D5
        LDA timer,X                     ; $23D7
        CMP #$01                        ; $23D9
        BNE L_23F1                      ; $23DB
        LDA facing,X                    ; $23DD
        EOR #$01                        ; $23DF
        STA facing,X                    ; $23E1
        LDA #$14                        ; $23E3
        STA state,X                     ; $23E5
        LDA #$00                        ; $23E7
        STA $0106,X                     ; $23E9
        LDA #$80                        ; $23EC
        STA timer,X                     ; $23EE
        RTS                             ; $23F0
L_23F1:
        LDA facing,X                    ; $23F1
        CMP #$01                        ; $23F3
        BNE L_23FF                      ; $23F5
        JSR L_1EAE                      ; $23F7
        LDA #$15                        ; $23FA
        JMP keep_state                  ; $23FC
L_23FF:
        JSR L_1E6D                      ; $23FF
        LDA #$15                        ; $2402
        JMP keep_state                  ; $2404
L_2407:
        CMP #$12                        ; $2407
        BNE L_240E                      ; $2409
        JMP keep_state                  ; $240B
L_240E:
        CMP #$0F                        ; $240E
        BNE L_2440                      ; $2410
        TXA                             ; $2412
        BEQ L_2419                      ; $2413
        CPY #$02                        ; $2415
        BCC L_243D                      ; $2417
L_2419:
        LDA facing,X                    ; $2419
        CMP #$01                        ; $241B
        BNE L_242E                      ; $241D
        JSR move_left                   ; $241F
        JSR move_left                   ; $2422
        LDA #$01                        ; $2425
        STA facing,X                    ; $2427
        LDA #$0F                        ; $2429
        JMP keep_state                  ; $242B
L_242E:
        JSR move_right                  ; $242E
        JSR move_right                  ; $2431
        LDA #$00                        ; $2434
        STA facing,X                    ; $2436
        LDA #$0F                        ; $2438
        JMP keep_state                  ; $243A
L_243D:
        JMP L_228C                      ; $243D
L_2440:
        CMP #$13                        ; $2440
        BNE L_24B3                      ; $2442
        TXA                             ; $2444
        BEQ L_244F                      ; $2445
        LDA $D1,X                       ; $2447
        BMI L_244F                      ; $2449
        CPY #$02                        ; $244B
        BCC L_24B0                      ; $244D
L_244F:
        LDA hit,X                       ; $244F
        AND #$05                        ; $2451
        BNE L_2458                      ; $2453
        JSR move_down                   ; $2455
L_2458:
        LDA facing,X                    ; $2458
        CMP #$01                        ; $245A
        BNE L_248D                      ; $245C
        CPY #$0A                        ; $245E
        BNE L_247E                      ; $2460
        CPX #$02                        ; $2462
        BNE L_247E                      ; $2464
        JSR move_left                   ; $2466
        JSR move_left                   ; $2469
        JSR move_left                   ; $246C
        JSR move_left                   ; $246F
        JSR move_left                   ; $2472
        JSR move_left                   ; $2475
        JSR move_left                   ; $2478
        JSR move_left                   ; $247B
L_247E:
        JSR move_left                   ; $247E
        JSR move_left                   ; $2481
        LDA #$01                        ; $2484
        STA facing,X                    ; $2486
        LDA #$13                        ; $2488
        JMP keep_state                  ; $248A
L_248D:
        CPY #$0A                        ; $248D
        BNE L_24A1                      ; $248F
        CPX #$02                        ; $2491
        BNE L_24A1                      ; $2493
        JSR move_right                  ; $2495
        JSR move_right                  ; $2498
        JSR move_right                  ; $249B
        JSR move_right                  ; $249E
L_24A1:
        JSR move_right                  ; $24A1
        JSR move_right                  ; $24A4
        LDA #$00                        ; $24A7
        STA facing,X                    ; $24A9
        LDA #$13                        ; $24AB
        JMP keep_state                  ; $24AD
L_24B0:
        JMP L_228C                      ; $24B0
L_24B3:
        CMP #$18                        ; $24B3
        BNE keep_state                  ; $24B5
L_24B7:
        LDA hit,X                       ; $24B7
        AND #$08                        ; $24B9
        BNE L_24C6                      ; $24BB
        LDA #$01                        ; $24BD
        STA $52                         ; $24BF
        LDA #$18                        ; $24C1
        JMP keep_state                  ; $24C3
L_24C6:
        TXA                             ; $24C6
        PHA                             ; $24C7
        LDA #$00                        ; $24C8
        LDY #$27                        ; $24CA
L_24CC:
        ORA $4F63,X                     ; $24CC
        INX                             ; $24CF
        INX                             ; $24D0
        INX                             ; $24D1
        DEY                             ; $24D2
        BPL L_24CC                      ; $24D3
        TAY                             ; $24D5
        PLA                             ; $24D6
        TAX                             ; $24D7
        TYA                             ; $24D8
        BNE L_24ED                      ; $24D9
        LDA #$00                        ; $24DB
        STA $51                         ; $24DD
        LDX $29                         ; $24DF
        STX $4E                         ; $24E1
        LDA $4BC2,X                     ; $24E3
        TAY                             ; $24E6
        LDA $4BAE,X                     ; $24E7
        JMP L_2F8E                      ; $24EA
L_24ED:
        LDA facing,X                    ; $24ED
        STA $4C                         ; $24EF
        JMP L_1CD4                      ; $24F1

; A is the state to keep. $52 still holds the timer from the start of move_for_state. It is
; decremented here and stored back to timer,X.
keep_state:
        STA state,X                     ; $24F4
        TAY                             ; $24F6
        LDA $4A8C,Y                     ; $24F7
        STA $0106,X                     ; $24FA
        DEC $52                         ; Decrement after the state has used the old value.
        LDA $52                         ; $24FF
        STA timer,X                     ; $2501
        RTS                             ; $2503

; Leave the script. State 0, timer 0. A sideways jump that has started to fall uses this
; when it meets the ground.
land:
        LDA #$00                        ; $2504
        JMP set_state                   ; $2506
play_sound:
        LDA #$15                        ; $2509
        STA $D418                       ; $250B
        LDA $EB00                       ; $250E
        BNE L_2521                      ; $2511
L_2513:
        STY $EB00                       ; $2513
        TYA                             ; $2516
        AND #$0F                        ; $2517
        TAX                             ; $2519
        LDA $2572,X                     ; $251A
        STA $2581                       ; $251D
        RTS                             ; $2520
L_2521:
        LDA $EF70                       ; $2521
        BNE L_2534                      ; $2524
L_2526:
        STY $EF70                       ; $2526
        TYA                             ; $2529
        AND #$0F                        ; $252A
        TAX                             ; $252C
        LDA $2572,X                     ; $252D
        STA $2582                       ; $2530
        RTS                             ; $2533
L_2534:
        LDA $F070                       ; $2534
        BNE L_2547                      ; $2537
L_2539:
        STY $F070                       ; $2539
        TYA                             ; $253C
        AND #$0F                        ; $253D
        TAX                             ; $253F
        LDA $2572,X                     ; $2540
        STA $2583                       ; $2543
        RTS                             ; $2546
L_2547:
        TYA                             ; $2547
        AND #$0F                        ; $2548
        TAX                             ; $254A
        LDA $2572,X                     ; $254B
        CMP $2581                       ; $254E
        BCS L_2513                      ; $2551
        CMP $2582                       ; $2553
        BCS L_2526                      ; $2556
        CMP $2583                       ; $2558
        BCS L_2539                      ; $255B
        RTS                             ; $255D
; Clear the three effect slots and write zero across the SID registers.
silence_sid:
        LDA #$00                        ; $255E
        STA $EB00                       ; $2560
        STA $EF70                       ; $2563
        STA $F070                       ; $2566
        LDX #$20                        ; $2569
L_256B:
        STA $D400,X                     ; $256B
        DEX                             ; $256E
        BPL L_256B                      ; $256F
        RTS                             ; $2571

; $2572  18 bytes of data between routines

; For each active actor, copy his two sprite images into the hardware image slots.
blit_sprites:
        LDX #$02                        ; $2584
L_2586:
        STX $51                         ; $2586
        LDA active,X                    ; $2588
        AND #$01                        ; $258A
        BEQ L_25D7                      ; $258C
        LDA $276D,X                     ; $258E
        JSR L_2704                      ; $2591
        LDA $4F4E,X                     ; $2594
        TAX                             ; $2597
        JSR L_2719                      ; $2598
        PHA                             ; $259B
        TXA                             ; $259C
        LDX $51                         ; $259D
        STA $D4,X                       ; $259F
        PLA                             ; $25A1
        STA $DA,X                       ; $25A2
        TYA                             ; $25A4
        ASL A                           ; $25A5
        ADC $E0,X                       ; $25A6
        TAY                             ; $25A8
        LDA $D4,X                       ; $25A9
        JSR copy_sprite_image                      ; $25AB
        LDX $51                         ; $25AE
        CPX #$02                        ; $25B0
        BEQ L_25D7                      ; $25B2
        LDA $2770,X                     ; $25B4
        BMI L_25D7                      ; $25B7
        JSR L_2704                      ; $25B9
        LDA $4F4E,X                     ; $25BC
        TAX                             ; $25BF
        INX                             ; $25C0
        JSR L_2719                      ; $25C1
        PHA                             ; $25C4
        TXA                             ; $25C5
        LDX $51                         ; $25C6
        STA $D7,X                       ; $25C8
        PLA                             ; $25CA
        STA $DD,X                       ; $25CB
        TYA                             ; $25CD
        ASL A                           ; $25CE
        ADC $E0,X                       ; $25CF
        TAY                             ; $25D1
        LDA $D7,X                       ; $25D2
        JSR copy_sprite_image                      ; $25D4
L_25D7:
        LDX $51                         ; $25D7
        DEX                             ; $25D9
        BPL L_2586                      ; $25DA
        LDA #$00                        ; $25DC
        STA $277E                       ; $25DE
        RTS                             ; $25E1

; $25E2  190 bytes of data between routines

; Copy one image from the library at $6667 into the sprite buffer at ($0E). A zero in A
; means the buffer is cleared instead.
copy_sprite_image:
        BNE L_26D7                      ; $26A0
        LDA #$3F                        ; $26A2
        STA $277F                       ; $26A4
        LDA $E0,X                       ; $26A7
        TAX                             ; $26A9
L_26AA:
        LDA $6667,X                     ; $26AA
        STA ($0E),Y                     ; $26AD
        DEC $277F                       ; $26AF
        DEX                             ; $26B2
        BMI L_26C6                      ; $26B3
        DEY                             ; $26B5
        LDA #$00                        ; $26B6
        STA ($0E),Y                     ; $26B8
        DEC $277F                       ; $26BA
        DEY                             ; $26BD
        STA ($0E),Y                     ; $26BE
        DEC $277F                       ; $26C0
        DEY                             ; $26C3
        BPL L_26AA                      ; $26C4
L_26C6:
        LDA $277F                       ; $26C6
        EOR #$3F                        ; $26C9
        TAY                             ; $26CB
        LDA #$00                        ; $26CC
L_26CE:
        STA ($0E),Y                     ; $26CE
        INY                             ; $26D0
        DEC $277F                       ; $26D1
        BPL L_26CE                      ; $26D4
        RTS                             ; $26D6
L_26D7:
        LDA #$3F                        ; $26D7
        STA $277F                       ; $26D9
        LDA $E0,X                       ; $26DC
        ASL A                           ; $26DE
        TAX                             ; $26DF
        DEX                             ; $26E0
        DEY                             ; $26E1
        DEY                             ; $26E2
L_26E3:
        LDA $6667,X                     ; $26E3
        STA ($0E),Y                     ; $26E6
        DEC $277F                       ; $26E8
        DEX                             ; $26EB
        DEY                             ; $26EC
        LDA $6667,X                     ; $26ED
        STA ($0E),Y                     ; $26F0
        DEC $277F                       ; $26F2
        DEX                             ; $26F5
        BMI L_26C6                      ; $26F6
        DEY                             ; $26F8
        LDA #$00                        ; $26F9
        STA ($0E),Y                     ; $26FB
        DEC $277F                       ; $26FD
        DEY                             ; $2700
        BPL L_26E3                      ; $2701
        BRK                             ; $2703
L_2704:
        ASL A                           ; $2704
        LDY $277D                       ; $2705
        BEQ L_270D                      ; $2708
        CLC                             ; $270A
        ADC #$0A                        ; $270B
L_270D:
        TAY                             ; $270D
        LDA $274F,Y                     ; $270E
        STA $0E                         ; $2711
        LDA $2750,Y                     ; $2713
        STA $0F                         ; $2716
        RTS                             ; $2718
L_2719:
        LDA $59BD,X                     ; $2719
        STA $26AB                       ; $271C
        STA $26E4                       ; $271F
        STA $26EE                       ; $2722
        LDA $5A8F,X                     ; $2725
        STA $26AC                       ; $2728
        STA $26E5                       ; $272B
        STA $26EF                       ; $272E
        LDY $5B61,X                     ; $2731
        LDA #$02                        ; $2734
        CMP $51                         ; $2736
        BNE L_273E                      ; $2738
        LDA #$F0                        ; $273A
        BNE L_2741                      ; $273C
L_273E:
        LDA $5C33,X                     ; $273E
L_2741:
        PHA                             ; $2741
        LDA $5CBF,X                     ; $2742
        PHA                             ; $2745
        LDX $51                         ; $2746
        TYA                             ; $2748
        STA $E0,X                       ; $2749
        PLA                             ; $274B
        TAX                             ; $274C
        PLA                             ; $274D
L_274E:
        RTS                             ; $274E

; $274F  49 bytes of data between routines

; Pick the library number from the state and the facing, then store_pose records it. Runs
; once per actor per step.
choose_pose:
        LDA $4FDB,X                     ; $2780
        BNE L_274E                      ; $2783
        INC $4FDB,X                     ; $2785
        LDA $4F48,X                     ; $2788
        STA $4F54,X                     ; $278B
        LDY facing,X                    ; $278E
        LDA state,X                     ; $2790
        CMP #$0E                        ; $2792  fall. Bit 3 is library 8, the legs together. Otherwise library $44.
        BNE L_27A6                      ; $2794
        LDA hit,X                       ; $2796
        AND #$08                        ; $2798
        BEQ L_27A1                      ; $279A
        LDY #$08                        ; $279C
        JMP store_pose                  ; $279E
L_27A1:
        LDY #$44                        ; $27A1
        JMP store_pose                  ; $27A3
L_27A6:
        CMP #$1A                        ; $27A6  duck. Facing right is library $42, facing left is library $40.
        BNE L_27B8                      ; $27A8
        CPY #$01                        ; $27AA
        BNE L_27B3                      ; $27AC
        LDY #$42                        ; $27AE
        JMP store_pose                  ; $27B0
L_27B3:
        LDY #$40                        ; $27B3
        JMP store_pose                  ; $27B5
L_27B8:
        CMP #$19                        ; $27B8
        BNE L_27C1                      ; $27BA
        LDY #$08                        ; $27BC
        JMP store_pose                  ; $27BE
L_27C1:
        CMP #$18                        ; $27C1
        BNE L_27DA                      ; $27C3
        LDY $4F54,X                     ; $27C5
        CPY #$44                        ; $27C8
        BEQ L_27D5                      ; $27CA
        LDA $D4,X                       ; $27CC
        ORA $D7,X                       ; $27CE
        BNE L_27D5                      ; $27D0
        JMP store_pose                  ; $27D2
L_27D5:
        LDY #$08                        ; $27D5
        JMP store_pose                  ; $27D7
L_27DA:
        CMP #$1B                        ; $27DA
        BNE L_2800                      ; $27DC
        LDA timer,X                     ; $27DE
        CPY #$00                        ; $27E0
        BEQ L_27F2                      ; $27E2
        CMP #$04                        ; $27E4
        BCC L_27ED                      ; $27E6
        LDY #$38                        ; $27E8
        JMP store_pose                  ; $27EA
L_27ED:
        LDY #$3A                        ; $27ED
        JMP store_pose                  ; $27EF
L_27F2:
        CMP #$04                        ; $27F2
        BCC L_27FB                      ; $27F4
        LDY #$3C                        ; $27F6
        JMP store_pose                  ; $27F8
L_27FB:
        LDY #$3E                        ; $27FB
        JMP store_pose                  ; $27FD
L_2800:
        CMP #$13                        ; $2800
        BNE L_2826                      ; $2802
        LDA timer,X                     ; $2804
        CMP #$0A                        ; $2806
        BCC L_2818                      ; $2808
        CPY #$00                        ; $280A
        BNE L_2813                      ; $280C
        LDY #$3C                        ; $280E
        JMP store_pose                  ; $2810
L_2813:
        LDY #$38                        ; $2813
        JMP store_pose                  ; $2815
L_2818:
        CPY #$00                        ; $2818
        BNE L_2821                      ; $281A
        LDY #$3E                        ; $281C
        JMP store_pose                  ; $281E
L_2821:
        LDY #$3A                        ; $2821
        JMP store_pose                  ; $2823
L_2826:
        CMP #$10                        ; $2826
        BNE L_282F                      ; $2828
        LDY #$2A                        ; $282A
        JMP store_pose                  ; $282C
L_282F:
        CMP #$11                        ; $282F
        BNE L_2841                      ; $2831
        CPY #$01                        ; $2833
        BNE L_283C                      ; $2835
        LDY #$06                        ; $2837
        JMP store_pose                  ; $2839
L_283C:
        LDY #$04                        ; $283C
        JMP store_pose                  ; $283E
L_2841:
        CMP #$12                        ; $2841
        BNE L_2853                      ; $2843
        CPY #$01                        ; $2845
        BNE L_284E                      ; $2847
        LDY #$2E                        ; $2849
        JMP store_pose                  ; $284B
L_284E:
        LDY #$2C                        ; $284E
        JMP store_pose                  ; $2850
L_2853:
        CMP #$16                        ; $2853
        BNE L_2879                      ; $2855
        LDA timer,X                     ; $2857
        CMP #$0B                        ; $2859
        BCC L_286B                      ; $285B
        CPY #$01                        ; $285D
        BNE L_2866                      ; $285F
        LDY #$34                        ; $2861
        JMP store_pose                  ; $2863
L_2866:
        LDY #$30                        ; $2866
        JMP store_pose                  ; $2868
L_286B:
        CPY #$01                        ; $286B
        BNE L_2874                      ; $286D
        LDY #$36                        ; $286F
        JMP store_pose                  ; $2871
L_2874:
        LDY #$32                        ; $2874
        JMP store_pose                  ; $2876
L_2879:
        CMP #$0F                        ; $2879
        BNE pose_kick_left              ; $287B
        CPY #$01                        ; $287D
        BNE L_2886                      ; $287F
        LDY #$02                        ; $2881
        JMP store_pose                  ; $2883
L_2886:
        LDY #$00                        ; $2886
        JMP store_pose                  ; $2888

; Picture for flying kick left. Timer 10 or more is library $16 (tucked). The rest is
; library $18 (leg out).
pose_kick_left:
        CMP #$0C                        ; $288B
        BNE pose_kick_right             ; $288D
        LDA timer,X                     ; $288F
        CMP #$0A                        ; $2891
        BCC L_289A                      ; $2893
        LDY #$16                        ; $2895
        JMP store_pose                  ; $2897
L_289A:
        LDY #$18                        ; $289A
        JMP store_pose                  ; $289C

; Picture for flying kick right. Libraries $1A and $1C.
pose_kick_right:
        CMP #$0D                        ; $289F
        BNE L_28B3                      ; $28A1
        LDA timer,X                     ; $28A3
        CMP #$0A                        ; $28A5
        BCC L_28AE                      ; $28A7
        LDY #$1A                        ; $28A9
        JMP store_pose                  ; $28AB
L_28AE:
        LDY #$1C                        ; $28AE
        JMP store_pose                  ; $28B0
L_28B3:
        CMP #$14                        ; $28B3
        BNE L_28BD                      ; $28B5
        CPY #$01                        ; $28B7
        BEQ L_28E9                      ; $28B9
        BNE L_28CB                      ; $28BB
L_28BD:
        CMP #$15                        ; $28BD
        BNE L_28C7                      ; $28BF
        CPY #$01                        ; $28C1
        BEQ L_28E9                      ; $28C3
        BNE L_28CB                      ; $28C5
L_28C7:
        CMP #$03                        ; $28C7
        BNE L_28E5                      ; $28C9
L_28CB:
        LDA hit,X                       ; $28CB
        AND #$04                        ; $28CD
        BEQ L_28D4                      ; $28CF
        JMP L_2985                      ; $28D1
L_28D4:
        LDY $4F48,X                     ; $28D4
        CPY #$0E                        ; $28D7
        BEQ L_28E0                      ; $28D9
        LDY #$0E                        ; $28DB
        JMP store_pose                  ; $28DD
L_28E0:
        LDY #$10                        ; $28E0
        JMP store_pose                  ; $28E2
L_28E5:
        CMP #$04                        ; $28E5
        BNE pose_jump                   ; $28E7
L_28E9:
        LDA hit,X                       ; $28E9
        AND #$04                        ; $28EB
        BEQ L_28F2                      ; $28ED
        JMP L_2985                      ; $28EF
L_28F2:
        LDY $4F48,X                     ; $28F2
        CPY #$12                        ; $28F5
        BEQ L_28FE                      ; $28F7
        LDY #$12                        ; $28F9
        JMP store_pose                  ; $28FB
L_28FE:
        LDY #$14                        ; $28FE
        JMP store_pose                  ; $2900

; Picture for the standing jump, state 5. Timer 15 is library 8 (legs together). 12 through
; 14 are library $1E. 2 through 11 are library $20. Timer 1 is library $1E again, and 0 is
; library 8.
pose_jump:
        CMP #$05                        ; $2903
        BNE pose_side_left              ; $2905
        LDY timer,X                     ; $2907
        CPY #$0F                        ; $2909
        BCC L_2912                      ; $290B
        LDY #$08                        ; $290D
        JMP store_pose                  ; $290F
L_2912:
        CPY #$0C                        ; $2912
        BCC L_291B                      ; $2914
        LDY #$1E                        ; $2916
        JMP store_pose                  ; $2918
L_291B:
        CPY #$02                        ; $291B
        BCC L_2924                      ; $291D
        LDY #$20                        ; $291F
        JMP store_pose                  ; $2921
L_2924:
        CPY #$01                        ; $2924
        BCC L_292D                      ; $2926
        LDY #$1E                        ; $2928
        JMP store_pose                  ; $292A
L_292D:
        LDY #$08                        ; $292D
        JMP store_pose                  ; $292F

; Picture for sideways jump left, state 6. Timer 12 or more, and timers 0, 1 and 2, are
; library $22. The counts between are library $24.
pose_side_left:
        CMP #$06                        ; $2932
        BNE pose_side_right             ; $2934
        LDY timer,X                     ; $2936
        CPY #$0C                        ; $2938
        BCC L_2941                      ; $293A
L_293C:
        LDY #$22                        ; $293C
        JMP store_pose                  ; $293E
L_2941:
        CPY #$03                        ; $2941
        BCC L_293C                      ; $2943
        LDY #$24                        ; $2945
        JMP store_pose                  ; $2947

; Picture for sideways jump right, state 7. Libraries $26 and $28.
pose_side_right:
        CMP #$07                        ; $294A
        BNE L_2962                      ; $294C
        LDY timer,X                     ; $294E
        CPY #$0C                        ; $2950
        BCC L_2959                      ; $2952
L_2954:
        LDY #$26                        ; $2954
        JMP store_pose                  ; $2956
L_2959:
        CPY #$03                        ; $2959
        BCC L_2954                      ; $295B
        LDY #$28                        ; $295D
        JMP store_pose                  ; $295F
L_2962:
        CMP #$17                        ; $2962
        BNE L_2970                      ; $2964
        LDY $4F54,X                     ; $2966
        LDA $AA,X                       ; $2969
        BEQ L_2985                      ; $296B
        JMP store_pose                  ; $296D
L_2970:
        CMP #$00                        ; $2970
        BNE L_2985                      ; $2972
        LDA hit,X                       ; $2974
        AND #$04                        ; $2976
        BEQ L_2996                      ; $2978
        LDY $4F54,X                     ; $297A
        CPY #$0A                        ; $297D
        BEQ L_298E                      ; $297F
        CPY #$0C                        ; $2981
        BEQ L_298E                      ; $2983
L_2985:
        LDY $4F54,X                     ; $2985
        CPY #$0A                        ; $2988
        BEQ L_2991                      ; $298A
        LDY #$0A                        ; $298C
L_298E:
        JMP store_pose                  ; $298E
L_2991:
        LDY #$0C                        ; $2991
        JMP store_pose                  ; $2993
L_2996:
        CPY #$01                        ; $2996
        BEQ L_299F                      ; $2998
        LDY #$00                        ; $299A
        JMP store_pose                  ; $299C
L_299F:
        LDY #$02                        ; $299F

; Y is the library number. It is stored for this actor, and the following library is stored
; beside it. $95,X selects which of four copies of the library table to use.
store_pose:
        TYA                             ; $29A1
        STA $4F48,X                     ; $29A2
        CLC                             ; $29A5
        ADC #$01                        ; $29A6
        STA $4F4B,X                     ; $29A8
        LDA active,X                    ; $29AB
        ROL A                           ; $29AD
        ROL A                           ; $29AE
        ROL A                           ; $29AF
        AND #$03                        ; $29B0
        TAY                             ; $29B2
        LDA $59A5,Y                     ; $29B3
        CLC                             ; $29B6
        ADC $4F48,X                     ; $29B7
        STA $4F4E,X                     ; $29BA
        ADC #$01                        ; $29BD
        STA $4F51,X                     ; $29BF
        RTS                             ; $29C2
; Clear $B3 and $BC, then read the VIC sprite-sprite register $D01E. A hit is kept in $67.
read_sprite_hits:
        LDA #$00                        ; $29C3
        LDX #$02                        ; $29C5
L_29C7:
        STA $B3,X                       ; $29C7
        STA $BC,X                       ; $29C9
        DEX                             ; $29CB
        BPL L_29C7                      ; $29CC
        LDA $D01E                       ; $29CE
        BNE L_29D4                      ; $29D1
        RTS                             ; $29D3
L_29D4:
        STA $67                         ; $29D4
        AND #$03                        ; $29D6
        BNE L_29DD                      ; $29D8
        JMP L_2A07                      ; $29DA
L_29DD:
        LDA $67                         ; $29DD
        AND #$0C                        ; $29DF
        BNE L_29E6                      ; $29E1
        JMP L_29F2                      ; $29E3
L_29E6:
        LDA $B3                         ; $29E6
        ORA #$08                        ; $29E8
        STA $B3                         ; $29EA
        LDA $B4                         ; $29EC
        ORA #$02                        ; $29EE
        STA $B4                         ; $29F0
L_29F2:
        LDA $67                         ; $29F2
        AND #$30                        ; $29F4
        BNE L_29FB                      ; $29F6
        JMP L_2A07                      ; $29F8
L_29FB:
        LDA $B3                         ; $29FB
        ORA #$10                        ; $29FD
        STA $B3                         ; $29FF
        LDA $B5                         ; $2A01
        ORA #$02                        ; $2A03
        STA $B5                         ; $2A05
L_2A07:
        LDA $67                         ; $2A07
        AND #$0C                        ; $2A09
        BEQ L_2A1F                      ; $2A0B
        LDA $67                         ; $2A0D
        AND #$30                        ; $2A0F
        BEQ L_2A1F                      ; $2A11
        LDA $B4                         ; $2A13
        ORA #$10                        ; $2A15
        STA $B4                         ; $2A17
        LDA $B5                         ; $2A19
        ORA #$08                        ; $2A1B
        STA $B5                         ; $2A1D
L_2A1F:
        LDX #$02                        ; $2A1F
L_2A21:
        LDA $B3,X                       ; $2A21
        STA $4C                         ; $2A23
        LDY #$00                        ; $2A25
L_2A27:
        LSR $4C                         ; $2A27
        BCS L_2A36                      ; $2A29
L_2A2B:
        INY                             ; $2A2B
        CPY #$05                        ; $2A2C
        BCC L_2A27                      ; $2A2E
        DEX                             ; $2A30
        BPL L_2A21                      ; $2A31
        JMP L_2B4E                      ; $2A33
L_2A36:
        STY $52                         ; $2A36
        LDA state,X                     ; $2A38
        CMP #$18                        ; $2A3A
        BEQ L_2A2B                      ; $2A3C
        CMP #$1A                        ; $2A3E
        BEQ L_2A2B                      ; $2A40
        CMP #$0F                        ; $2A42
        BEQ L_2A2B                      ; $2A44
        CMP #$13                        ; $2A46
        BEQ L_2A2B                      ; $2A48
        CMP #$19                        ; $2A4A
        BEQ L_2A2B                      ; $2A4C
        CMP #$1B                        ; $2A4E
        BEQ L_2A2B                      ; $2A50
        LDA $4F40,Y                     ; $2A52
        BMI L_2A76                      ; $2A55
        TAY                             ; $2A57
        LDA $00BF,Y                     ; $2A58
        CMP #$1B                        ; $2A5B
        BEQ L_2A76                      ; $2A5D
        CMP #$16                        ; $2A5F
        BEQ L_2A7B                      ; $2A61
        CMP #$11                        ; $2A63
        BEQ L_2AA5                      ; $2A65
        CMP #$0D                        ; $2A67
        BEQ L_2AD4                      ; $2A69
        CMP #$0C                        ; $2A6B
        BEQ L_2AD4                      ; $2A6D
        CMP #$0E                        ; $2A6F
        BNE L_2A76                      ; $2A71
        JMP L_2AFA                      ; $2A73
L_2A76:
        LDY $52                         ; $2A76
        JMP L_2A2B                      ; $2A78
L_2A7B:
        LDA $00CE,Y                     ; $2A7B
        CMP #$08                        ; $2A7E
        BCS L_2A76                      ; $2A80
        LDA #$04                        ; $2A82
        STA $61                         ; $2A84
        JSR L_2C54                      ; $2A86
        BCC L_2A76                      ; $2A89
        BMI L_2A76                      ; $2A8B
        TXA                             ; $2A8D
        PHA                             ; $2A8E
        TYA                             ; $2A8F
        PHA                             ; $2A90
        LDY #$87                        ; $2A91
        JSR play_sound                  ; $2A93
        PLA                             ; $2A96
        TAY                             ; $2A97
        PLA                             ; $2A98
        TAX                             ; $2A99
        JSR L_2B3E                      ; $2A9A
        LDA $BC,X                       ; $2A9D
        ORA #$02                        ; $2A9F
        STA $BC,X                       ; $2AA1
        BNE L_2A76                      ; $2AA3
L_2AA5:
        LDA #$06                        ; $2AA5
        STA $61                         ; $2AA7
        JSR L_2C54                      ; $2AA9
        BCC L_2A76                      ; $2AAC
        BMI L_2A76                      ; $2AAE
        LDA $00A1,Y                     ; $2AB0
        SEC                             ; $2AB3
        SBC ypos,X                      ; $2AB4
        BPL L_2ABC                      ; $2AB6
        EOR #$FF                        ; $2AB8
        ADC #$01                        ; $2ABA
L_2ABC:
        CMP #$16                        ; $2ABC
        BCS L_2A76                      ; $2ABE
        JSR L_2E29                      ; $2AC0
        JSR L_2B3E                      ; $2AC3
        LDA $BC,X                       ; $2AC6
        ORA #$04                        ; $2AC8
        CPY #$00                        ; $2ACA
        BNE L_2AD0                      ; $2ACC
        ORA #$80                        ; $2ACE
L_2AD0:
        STA $BC,X                       ; $2AD0
        BNE L_2A76                      ; $2AD2
L_2AD4:
        LDA $00CE,Y                     ; $2AD4
        CMP #$0A                        ; $2AD7
        BCS L_2B3B                      ; $2AD9
        LDA #$06                        ; $2ADB
        STA $61                         ; $2ADD
        JSR L_2C54                      ; $2ADF
        BCC L_2B3B                      ; $2AE2
        BMI L_2B3B                      ; $2AE4
        JSR L_2E29                      ; $2AE6
        JSR L_2B3E                      ; $2AE9
        LDA $BC,X                       ; $2AEC
        ORA #$08                        ; $2AEE
        CPY #$00                        ; $2AF0
        BNE L_2AF6                      ; $2AF2
        ORA #$80                        ; $2AF4
L_2AF6:
        STA $BC,X                       ; $2AF6
        BNE L_2B3B                      ; $2AF8
L_2AFA:
        LDA ypos,X                      ; $2AFA
        SEC                             ; $2AFC
        SBC $00A1,Y                     ; $2AFD
        CMP #$0A                        ; $2B00
        BMI L_2B3B                      ; $2B02
        BCC L_2B3B                      ; $2B04
        LDA $4F45,Y                     ; $2B06
        BEQ L_2B3B                      ; $2B09
        SEC                             ; $2B0B
        SBC #$01                        ; $2B0C
        STA $4F45,Y                     ; $2B0E
        LDA $BC,X                       ; $2B11
        AND #$10                        ; $2B13
        BNE L_2B3B                      ; $2B15
        LDA $00BC,Y                     ; $2B17
        AND #$10                        ; $2B1A
        BNE L_2B3B                      ; $2B1C
        LDA state,X                     ; $2B1E
        CMP #$0C                        ; $2B20
        BEQ L_2B3B                      ; $2B22
        CMP #$0D                        ; $2B24
        BEQ L_2B3B                      ; $2B26
        CMP #$11                        ; $2B28
        BEQ L_2B3B                      ; $2B2A
        JSR L_2E29                      ; $2B2C
        LDA $BC,X                       ; $2B2F
        ORA #$10                        ; $2B31
        CPY #$00                        ; $2B33
        BNE L_2B39                      ; $2B35
        ORA #$80                        ; $2B37
L_2B39:
        STA $BC,X                       ; $2B39
L_2B3B:
        JMP L_2A76                      ; $2B3B
L_2B3E:
        LDA $00BC,Y                     ; $2B3E
        ORA #$01                        ; $2B41
        STA $00BC,Y                     ; $2B43
        LDA $00A4,Y                     ; $2B46
        EOR #$01                        ; $2B49
        STA facing,X                    ; $2B4B
        RTS                             ; $2B4D
L_2B4E:
        LDX #$02                        ; $2B4E
L_2B50:
        STX $51                         ; $2B50
        LDA $BC,X                       ; $2B52
        AND #$01                        ; $2B54
        BEQ L_2B9A                      ; $2B56
        LDA #$00                        ; $2B58
        STA state,X                     ; $2B5A
        LDA #$04                        ; $2B5C
        STA $0106,X                     ; $2B5E
        LDA #$18                        ; $2B61
        STA timer,X                     ; $2B63
        INC $4FDB,X                     ; $2B65
        TXA                             ; $2B68
        BEQ L_2B83                      ; $2B69
        JSR L_10D0                      ; $2B6B
        BVS L_2B7F                      ; $2B6E
        BCS L_2B79                      ; $2B70
        LDA $40                         ; $2B72
        BNE L_2B7F                      ; $2B74
        JMP L_2B9A                      ; $2B76
L_2B79:
        LDA #$0A                        ; $2B79
        STA timer,X                     ; $2B7B
        BNE L_2B9A                      ; $2B7D
L_2B7F:
        LDA #$02                        ; $2B7F
        STA timer,X                     ; $2B81
L_2B83:
        JSR L_10D0                      ; $2B83
        BVS L_2B96                      ; $2B86
        BCS L_2B90                      ; $2B88
        LDA #$03                        ; $2B8A
        STA timer,X                     ; $2B8C
        BNE L_2B9A                      ; $2B8E
L_2B90:
        LDA #$08                        ; $2B90
        STA timer,X                     ; $2B92
        BNE L_2B9A                      ; $2B94
L_2B96:
        LDA #$0A                        ; $2B96
        STA timer,X                     ; $2B98
L_2B9A:
        LDA $BC,X                       ; $2B9A
        AND #$02                        ; $2B9C
        BEQ L_2BB1                      ; $2B9E
        LDA $D1,X                       ; $2BA0
        SEC                             ; $2BA2
        SBC #$02                        ; $2BA3
        STA $D1,X                       ; $2BA5
        BMI L_2BEF                      ; $2BA7
        LDA #$0F                        ; $2BA9
        STA state,X                     ; $2BAB
        LDA #$03                        ; $2BAD
        STA timer,X                     ; $2BAF
L_2BB1:
        LDA $BC,X                       ; $2BB1
        AND #$04                        ; $2BB3
        BEQ L_2BD5                      ; $2BB5
        LDA $BC,X                       ; $2BB7
        BPL L_2BC4                      ; $2BB9
        LDA #$64                        ; $2BBB
        LDX #$02                        ; $2BBD
        JSR add_score                      ; $2BBF
        LDX $51                         ; $2BC2
L_2BC4:
        LDA $D1,X                       ; $2BC4
        SEC                             ; $2BC6
        SBC #$03                        ; $2BC7
        STA $D1,X                       ; $2BC9
        BMI L_2C24                      ; $2BCB
        LDA #$0F                        ; $2BCD
        STA state,X                     ; $2BCF
        LDA #$04                        ; $2BD1
        STA timer,X                     ; $2BD3
L_2BD5:
        LDA $BC,X                       ; $2BD5
        AND #$08                        ; $2BD7
        BEQ L_2BF9                      ; $2BD9
        LDA $BC,X                       ; $2BDB
        BPL L_2BE8                      ; $2BDD
        LDA #$4B                        ; $2BDF
        LDX #$02                        ; $2BE1
        JSR add_score                      ; $2BE3
        LDX $51                         ; $2BE6
L_2BE8:
        LDA $D1,X                       ; $2BE8
        SEC                             ; $2BEA
        SBC #$07                        ; $2BEB
        STA $D1,X                       ; $2BED
L_2BEF:
        BMI L_2C24                      ; $2BEF
        LDA #$13                        ; $2BF1
        STA state,X                     ; $2BF3
        LDA #$0C                        ; $2BF5
        STA timer,X                     ; $2BF7
L_2BF9:
        LDA $BC,X                       ; $2BF9
        AND #$10                        ; $2BFB
        BEQ L_2C1D                      ; $2BFD
        LDA $BC,X                       ; $2BFF
        BPL L_2C0C                      ; $2C01
        LDA #$32                        ; $2C03
        LDX #$02                        ; $2C05
        JSR add_score                      ; $2C07
        LDX $51                         ; $2C0A
L_2C0C:
        LDA $D1,X                       ; $2C0C
        SEC                             ; $2C0E
        SBC #$05                        ; $2C0F
        STA $D1,X                       ; $2C11
        BMI L_2C24                      ; $2C13
        LDA #$1B                        ; $2C15
        STA state,X                     ; $2C17
        LDA #$08                        ; $2C19
        STA timer,X                     ; $2C1B
L_2C1D:
        DEX                             ; $2C1D
        BMI L_2C23                      ; $2C1E
        JMP L_2B50                      ; $2C20
L_2C23:
        RTS                             ; $2C23
L_2C24:
        LDA $BC,X                       ; $2C24
        AND #$80                        ; $2C26
        BEQ L_2C47                      ; $2C28
        STX $51                         ; $2C2A
        CPX #$01                        ; $2C2C
        BNE L_2C40                      ; $2C2E
        LDA #$01                        ; $2C30
        TAX                             ; $2C32
        JSR add_score                      ; $2C33
        LDA #$C2                        ; $2C36
        LDX #$02                        ; $2C38
        JSR add_score                      ; $2C3A
        JMP L_2C47                      ; $2C3D
L_2C40:
        LDA #$C8                        ; $2C40
        LDX #$02                        ; $2C42
        JSR add_score                      ; $2C44
L_2C47:
        LDX $51                         ; $2C47
        LDA #$13                        ; $2C49
        STA state,X                     ; $2C4B
        LDA #$0C                        ; $2C4D
        STA timer,X                     ; $2C4F
        JMP L_2BF9                      ; $2C51
L_2C54:
        LDA $009E,Y                     ; $2C54
        SEC                             ; $2C57
        SBC xpos,X                      ; $2C58
        PHA                             ; $2C5A
        PHP                             ; $2C5B
        LDA $00A4,Y                     ; $2C5C
        CMP #$01                        ; $2C5F
        BNE L_2C6B                      ; $2C61
        PLP                             ; $2C63
        PLA                             ; $2C64
        EOR #$FF                        ; $2C65
        ADC #$01                        ; $2C67
        PHA                             ; $2C69
        PHP                             ; $2C6A
L_2C6B:
        PLP                             ; $2C6B
        PLA                             ; $2C6C
        CMP $61                         ; $2C6D
        RTS                             ; $2C6F

; $2C70  229 bytes of data between routines

; If Bruce (X = 0) has $D1 negative, he has lost a life. The other actors call $3001
; instead.
check_bruce_down:
        LDX #$02                        ; $2D55
L_2D57:
        LDA active,X                    ; $2D57
        AND #$01                        ; $2D59
        BEQ L_2D75                      ; $2D5B
        LDA state,X                     ; $2D5D
        CMP #$13                        ; $2D5F
        BEQ L_2D67                      ; $2D61
        CMP #$19                        ; $2D63
        BNE L_2D6B                      ; $2D65
L_2D67:
        LDA timer,X                     ; $2D67
        BNE L_2D75                      ; $2D69
L_2D6B:
        LDA $D1,X                       ; $2D6B
        BPL L_2D75                      ; $2D6D
        TXA                             ; $2D6F
        BEQ L_2D79                      ; $2D70
        JSR L_3001                      ; $2D72
L_2D75:
        DEX                             ; $2D75
        BPL L_2D57                      ; $2D76
        RTS                             ; $2D78
L_2D79:
        JMP lose_life                      ; $2D79
; The other fighters. An actor whose active flags say he should move this frame is stepped
; here.
enemy_turn:
        LDX #$02                        ; $2D7C
L_2D7E:
        LDA $D1,X                       ; $2D7E
        BMI L_2DBC                      ; $2D80
        LDA $42CA                       ; $2D82
        BNE L_2D8F                      ; $2D85
        LDA active,X                    ; $2D87
        AND #$81                        ; $2D89
        CMP #$81                        ; $2D8B
        BEQ L_2D97                      ; $2D8D
L_2D8F:
        LDA active,X                    ; $2D8F
        AND #$41                        ; $2D91
        CMP #$41                        ; $2D93
        BNE L_2DBC                      ; $2D95
L_2D97:
        LDA state,X                     ; $2D97
        CMP #$00                        ; $2D99
        BNE L_2DBC                      ; $2D9B
        LDA timer,X                     ; $2D9D
        BNE L_2DBC                      ; $2D9F
        LDA hit,X                       ; $2DA1
        AND #$05                        ; $2DA3
        BEQ L_2DBC                      ; $2DA5
        LDA $C2,X                       ; $2DA7
        CMP #$0D                        ; $2DA9
        BEQ L_2DC0                      ; $2DAB
        CMP #$0C                        ; $2DAD
        BEQ L_2DC0                      ; $2DAF
        LDA #$14                        ; $2DB1
        STA state,X                     ; $2DB3
        STA timer,X                     ; $2DB5
        LDA #$00                        ; $2DB7
        STA $0106,X                     ; $2DB9
L_2DBC:
        DEX                             ; $2DBC
        BPL L_2D7E                      ; $2DBD
        RTS                             ; $2DBF
L_2DC0:
        JSR L_228C                      ; $2DC0
        LDA #$18                        ; $2DC3
        STA timer,X                     ; $2DC5
        JSR L_10D0                      ; $2DC7
        BVS L_2DD8                      ; $2DCA
        BCS L_2DD3                      ; $2DCC
        LDA $40                         ; $2DCE
        BNE L_2DD8                      ; $2DD0
        RTS                             ; $2DD2
L_2DD3:
        LDA #$03                        ; $2DD3
        STA timer,X                     ; $2DD5
        RTS                             ; $2DD7
L_2DD8:
        LDA #$01                        ; $2DD8
        STA timer,X                     ; $2DDA
        RTS                             ; $2DDC
L_2DDD:
        LDA ypos,X                      ; $2DDD
        SEC                             ; $2DDF
        SBC ypos                        ; $2DE0
        PHP                             ; $2DE2
        BCS L_2DE9                      ; $2DE3
        EOR #$FF                        ; $2DE5
        ADC #$01                        ; $2DE7
L_2DE9:
        CMP #$31                        ; $2DE9
        BCC L_2DEF                      ; $2DEB
        LDA #$30                        ; $2DED
L_2DEF:
        PLP                             ; $2DEF
        BCS L_2DF4                      ; $2DF0
        ORA #$80                        ; $2DF2
L_2DF4:
        STA $62                         ; $2DF4
        LDA xpos,X                      ; $2DF6
        SEC                             ; $2DF8
        SBC xpos                        ; $2DF9
        PHP                             ; $2DFB
        BCS L_2E02                      ; $2DFC
        EOR #$FF                        ; $2DFE
        ADC #$01                        ; $2E00
L_2E02:
        CMP #$31                        ; $2E02
        BCC L_2E08                      ; $2E04
        LDA #$30                        ; $2E06
L_2E08:
        PLP                             ; $2E08
        BCS L_2E0D                      ; $2E09
        ORA #$80                        ; $2E0B
L_2E0D:
        STA $63                         ; $2E0D
        RTS                             ; $2E0F
L_2E10:
        LDA #$00                        ; $2E10
        STA state,X                     ; $2E12
        LDA #$00                        ; $2E14
        STA timer,X                     ; $2E16
        STA $0106,X                     ; $2E18
        RTS                             ; $2E1B
L_2E1C:
        LDA $63                         ; $2E1C
        BPL L_2E24                      ; $2E1E
        LDA #$01                        ; $2E20
        BNE L_2E26                      ; $2E22
L_2E24:
        LDA #$00                        ; $2E24
L_2E26:
        STA facing,X                    ; $2E26
        RTS                             ; $2E28
L_2E29:
        PHA                             ; $2E29
        TXA                             ; $2E2A
        PHA                             ; $2E2B
        TYA                             ; $2E2C
        PHA                             ; $2E2D
        LDY #$86                        ; $2E2E
        JSR play_sound                  ; $2E30
        PLA                             ; $2E33
        TAY                             ; $2E34
        PLA                             ; $2E35
        TAX                             ; $2E36
        PLA                             ; $2E37
        RTS                             ; $2E38
; For each active actor except state $19, look at his tile row and the row
; below. cell_of_actor has his column in $E6,X and his tile row in $E9,X.
; Each of those rows checks that column and the two to the right. Hit bit 1
; is kept in $53 and gates the prize tiles, the lamps among them.
scan_specials:
        LDA #$00                        ; $2E39
        STA $0100                       ; $2E3B
        LDX #$02                        ; $2E3E
L_2E40:
        STX $51                         ; $2E40
        LDA active,X                    ; $2E42
        AND #$01                        ; $2E44
        BEQ L_2E68                      ; $2E46
        LDA state,X                     ; $2E48
        CMP #$19                        ; $2E4A
        BEQ L_2E68                      ; $2E4C
        CMP #$17                        ; $2E4E
        BNE L_2E55                      ; $2E50
        JSR L_2E10                      ; $2E52
L_2E55:
        LDA hit,X                       ; $2E55
        AND #$02                        ; $2E57
        STA $53                         ; $2E59
        JSR cell_of_actor                      ; $2E5B
        JSR scan_special_row                      ; $2E5E
        LDX $51                         ; $2E61
        INC $E9,X                       ; $2E63
        JSR scan_special_row                      ; $2E65
L_2E68:
        LDX $51                         ; $2E68
        DEX                             ; $2E6A
        BPL L_2E40                      ; $2E6B
        RTS                             ; $2E6D
; $4C counts 2, 1, 0, so three columns from $E6,X. Bit 7 of the tile is
; ignored. A shadow row therefore matches the same number as the row above.
scan_special_row:
        LDA $E9,X                       ; $2E6E
        TAX                             ; $2E70
        JSR point_at_row                      ; $2E71
        LDX $51                         ; $2E74
        LDY $E6,X                       ; $2E76
        LDA #$02                        ; $2E78
        STA $4C                         ; $2E7A
L_2E7C:
        LDA ($04),Y                     ; $2E7C
        AND #$7F                        ; $2E7E
        JSR match_special                      ; $2E80
        INY                             ; $2E83
        DEC $4C                         ; $2E84
        BPL L_2E7C                      ; $2E86
        RTS                             ; $2E88
; Tiles 2, 3, 4, and 5 are the four exit slots. The destination room, xpos,
; and ypos are the parallel tables at $4B5E/$4BFE/$4C12, then $4B72/$4C26/
; $4C3A, $4B86/$4C4E/$4C62, and $4B9A/$4C76/$4C8A. Tiles 2, 3, and 4 fall
; through to the next slot when that slot's xpos byte is 0. Tile 5 does not
; test xpos. The whole table is in rooms.asm. A slot whose xpos is 0 is the
; unused marker, including the tile 4 and tile 5 records in room 18, which
; are drawn as part of the boss and are not doors.
match_special:
        CMP #$05                        ; $2E89
        BNE L_2E90                      ; $2E8B
        JMP L_2F7B                      ; $2E8D
L_2E90:
        CMP #$02                        ; $2E90
        BNE L_2E97                      ; $2E92
        JMP L_2F3C                      ; $2E94
L_2E97:
        CMP #$03                        ; $2E97
        BNE L_2E9E                      ; $2E99
        JMP L_2F51                      ; $2E9B
L_2E9E:
        CMP #$04                        ; $2E9E
        BNE L_2EA5                      ; $2EA0
        JMP L_2F66                      ; $2EA2
L_2EA5:
        CMP #$62                        ; $2EA5
        BEQ L_2EF4                      ; $2EA7
L_2EA9:
        CMP #$2D                        ; $2EA9
        BEQ L_2EEA                      ; $2EAB
        CMP #$2E                        ; $2EAD
        BEQ L_2EEA                      ; $2EAF
; $53 held hit bit 1. The shift leaves zero when that bit was clear, and
; every prize tile below is then skipped. The tiles compared above, the
; doors among them, are tested either way.
        ASL $53                         ; $2EB1
        BEQ L_2EE9                      ; $2EB3
        CMP #$13                        ; $2EB5
        BEQ take_lamp                      ; $2EB7
        CMP #$14                        ; $2EB9
        BEQ take_lamp                      ; $2EBB
        CMP #$31                        ; $2EBD
        BEQ take_lamp                      ; $2EBF
        CMP #$64                        ; $2EC1
        BEQ L_2F01                      ; $2EC3
        CMP #$66                        ; $2EC5
        BEQ L_2EFA                      ; $2EC7
        CMP #$0F                        ; $2EC9
        BEQ L_2F01                      ; $2ECB
        CMP #$40                        ; $2ECD
        BEQ L_2F01                      ; $2ECF
        CMP #$57                        ; $2ED1
        BEQ L_2EDD                      ; $2ED3
        CMP #$54                        ; $2ED5
        BCC L_2EE9                      ; $2ED7
        CMP #$5B                        ; $2ED9
        BCC L_2F01                      ; $2EDB
L_2EDD:
        CMP #$71                        ; $2EDD
        BCC L_2EE5                      ; $2EDF
        CMP #$7D                        ; $2EE1
        BCC L_2F01                      ; $2EE3
L_2EE5:
        CMP #$60                        ; $2EE5
        BEQ L_2EF7                      ; $2EE7
L_2EE9:
        RTS                             ; $2EE9
L_2EEA:
        LDA $29                         ; $2EEA
        CMP #$10                        ; $2EEC
        BEQ L_2EF1                      ; $2EEE
        RTS                             ; $2EF0
L_2EF1:
        JMP L_2FD6                      ; $2EF1
L_2EF4:
        JMP L_310C                      ; $2EF4
L_2EF7:
        JMP L_306B                      ; $2EF7
L_2EFA:
        LDA $29                         ; $2EFA
        CMP #$0A                        ; $2EFC
        BCS L_2F01                      ; $2EFE
        RTS                             ; $2F00
L_2F01:
        JMP L_304B                      ; $2F01
; Tiles $13, $14, and $31. Bruce only: any other actor returns. claim_lamp
; returns the bare mount, or sets carry when this cell is not in the room's
; list. The mount is written on this row. The same byte with bit 7 set is
; written $28 bytes later, the shadow row. $7D is added to the low score
; byte, which the BCD digits show as 125. Sound $8E plays.
take_lamp:
        TXA                             ; $2F04
        BNE L_2EE9                      ; $2F05
        JSR stash_axy                      ; $2F07
        JSR claim_lamp                      ; $2F0A
        BCS L_2F39                      ; $2F0D
        PHA                             ; $2F0F
        JSR restore_axy                      ; $2F10
        PLA                             ; $2F13
        STA ($04),Y                     ; $2F14
        ORA #$80                        ; $2F16
        PHA                             ; $2F18
        TYA                             ; $2F19
        CLC                             ; $2F1A
        ADC #$28                        ; $2F1B
        TAY                             ; $2F1D
        PLA                             ; $2F1E
        STA ($04),Y                     ; $2F1F
        AND #$7F                        ; $2F21
        PHA                             ; $2F23
        TYA                             ; $2F24
        SEC                             ; $2F25
        SBC #$28                        ; $2F26
        TAY                             ; $2F28
        PLA                             ; $2F29
        JSR stash_axy                      ; $2F2A
        LDA #$7D                        ; $2F2D
        LDX #$02                        ; $2F2F
        JSR add_score                      ; $2F31
        LDY #$8E                        ; $2F34
        JSR play_sound                  ; $2F36
L_2F39:
        JMP restore_axy                      ; $2F39
L_2F3C:
        JSR L_30D4                      ; $2F3C
L_2F3F:
        BCS L_2EE9                      ; $2F3F
        LDX $29                         ; $2F41
        LDA $4B5E,X                     ; $2F43
        STA $4E                         ; $2F46
        LDA $4C12,X                     ; $2F48
        TAY                             ; $2F4B
        LDA $4BFE,X                     ; $2F4C
        BNE L_2F8E                      ; $2F4F
L_2F51:
        JSR L_30D4                      ; $2F51
        BCS L_2F3F                      ; $2F54
        LDX $29                         ; $2F56
        LDA $4B72,X                     ; $2F58
        STA $4E                         ; $2F5B
        LDA $4C3A,X                     ; $2F5D
        TAY                             ; $2F60
        LDA $4C26,X                     ; $2F61
        BNE L_2F8E                      ; $2F64
L_2F66:
        JSR L_30D4                      ; $2F66
L_2F69:
        BCS L_2F3F                      ; $2F69
        LDX $29                         ; $2F6B
        LDA $4B86,X                     ; $2F6D
        STA $4E                         ; $2F70
        LDA $4C62,X                     ; $2F72
        TAY                             ; $2F75
        LDA $4C4E,X                     ; $2F76
        BNE L_2F8E                      ; $2F79
L_2F7B:
        JSR L_30D4                      ; $2F7B
        BCS L_2F69                      ; $2F7E
        LDX $29                         ; $2F80
        LDA $4B9A,X                     ; $2F82
        STA $4E                         ; $2F85
        LDA $4C8A,X                     ; $2F87
        TAY                             ; $2F8A
        LDA $4C76,X                     ; $2F8B
L_2F8E:
        LDX $51                         ; $2F8E
        BEQ L_2F9A                      ; $2F90
        JSR L_3001                      ; $2F92
        PLA                             ; $2F95
        PLA                             ; $2F96
        JMP L_2E68                      ; $2F97
L_2F9A:
        STA xpos,X                      ; $2F9A
        TYA                             ; $2F9C
        STA ypos,X                      ; $2F9D
        LDA $4E                         ; $2F9F
        STA $29                         ; $2FA1
        LDX #$02                        ; $2FA3
L_2FA5:
        LDA active,X                    ; $2FA5
        AND #$01                        ; $2FA7
        BEQ L_2FB9                      ; $2FA9
        LDA $D1,X                       ; $2FAB
        STA $8E,X                       ; $2FAD
        LDA xpos                        ; $2FAF
        STA $8A,X                       ; $2FB1
        LDA ypos                        ; $2FB3
        STA $8C,X                       ; $2FB5
        BNE L_2FBF                      ; $2FB7
L_2FB9:
        LDA #$00                        ; $2FB9
        STA $8A,X                       ; $2FBB
        STA $8C,X                       ; $2FBD
L_2FBF:
        JSR L_3001                      ; $2FBF
        DEX                             ; $2FC2
        BNE L_2FA5                      ; $2FC3
        SEI                             ; $2FC5
        JSR L_0D42                      ; $2FC6
        LDX #$FF                        ; $2FC9
        TXS                             ; $2FCB
        JSR enter_room                      ; $2FCC
        JSR blit_sprites                      ; $2FCF
        CLI                             ; $2FD2
        JMP frame                      ; $2FD3
L_2FD6:
        TXA                             ; $2FD6
        BEQ L_2FDA                      ; $2FD7
L_2FD9:
        RTS                             ; $2FD9
L_2FDA:
        LDA ypos                        ; $2FDA
        CMP #$A0                        ; $2FDC
        BCC L_2FD9                      ; $2FDE
        LDX $03                         ; $2FE0
        DEC $4E95,X                     ; $2FE2
        INC $28                         ; $2FE5
        JSR lives_to_bcd                      ; $2FE7
        LDY #$8E                        ; $2FEA
        JSR play_sound                  ; $2FEC
        LDA #$56                        ; $2FEF
        LDX #$0A                        ; $2FF1
        LDY #$24                        ; $2FF3
        JSR print_string                      ; $2FF5
        LDA #$56                        ; $2FF8
        LDX #$0B                        ; $2FFA
        LDY #$24                        ; $2FFC
        JMP print_string                      ; $2FFE
L_3001:
        LDA #$01                        ; $3001
        EOR #$FF                        ; $3003
        AND active,X                    ; $3005
        STA active,X                    ; $3007
        LDY $276D,X                     ; $3009
        LDA $D015                       ; $300C
        AND $2778,Y                     ; $300F
        STA $D015                       ; $3012
        LDY $2770,X                     ; $3015
        BMI L_3023                      ; $3018
        LDA $D015                       ; $301A
        AND $2778,Y                     ; $301D
        STA $D015                       ; $3020
L_3023:
        LDA $EC,X                       ; $3023
        BEQ L_302F                      ; $3025
        JSR L_0E21                      ; $3027
        LDA #$FF                        ; $302A
        STA $4F40,Y                     ; $302C
L_302F:
        LDA $EF,X                       ; $302F
        BEQ L_303B                      ; $3031
        JSR L_0E21                      ; $3033
        LDA #$FF                        ; $3036
        STA $4F40,Y                     ; $3038
L_303B:
        CPX #$01                        ; $303B
        BNE L_3045                      ; $303D
        JSR clear_sprite_ram                      ; $303F
        JMP L_3048                      ; $3042
L_3045:
        JSR clear_upper_sprites                      ; $3045
L_3048:
        JMP L_0D42                      ; $3048
L_304B:
        LDA $29                         ; $304B
        CMP #$12                        ; $304D
        BNE L_3052                      ; $304F
        RTS                             ; $3051
L_3052:
        PHA                             ; $3052
        TXA                             ; $3053
        PHA                             ; $3054
        TYA                             ; $3055
        PHA                             ; $3056
        LDY #$8C                        ; $3057
        JSR play_sound                  ; $3059
        PLA                             ; $305C
        TAY                             ; $305D
        PLA                             ; $305E
        TAX                             ; $305F
        PLA                             ; $3060
        LDX $51                         ; $3061
        BEQ L_3068                      ; $3063
        JMP L_3001                      ; $3065
L_3068:
        JMP L_407D                      ; $3068
L_306B:
        LDA $05                         ; $306B
        CLC                             ; $306D
        ADC #$4C                        ; $306E
        STA $05                         ; $3070
        LDA ($04),Y                     ; $3072
        AND #$07                        ; $3074
        CMP #$05                        ; $3076
        BEQ L_30CC                      ; $3078
        LDA $4FDE                       ; $307A
        BNE L_3095                      ; $307D
        LDA #$01                        ; $307F
        STA $4FDE                       ; $3081
        LDA $E9,X                       ; $3084
        STA $4FE4                       ; $3086
        STY $4FE1                       ; $3089
        DEC $4FE1                       ; $308C
        DEC $4FE4                       ; $308F
        JMP L_30C8                      ; $3092
L_3095:
        LDA $4FDF                       ; $3095
        BNE L_30B0                      ; $3098
        LDA #$01                        ; $309A
        STA $4FDF                       ; $309C
        LDA $E9,X                       ; $309F
        STA $4FE5                       ; $30A1
        STY $4FE2                       ; $30A4
        DEC $4FE2                       ; $30A7
        DEC $4FE5                       ; $30AA
        JMP L_30C8                      ; $30AD
L_30B0:
        LDA $4FE0                       ; $30B0
        BNE L_30CC                      ; $30B3
        LDA #$01                        ; $30B5
        STA $4FE0                       ; $30B7
        LDA $E9,X                       ; $30BA
        STA $4FE6                       ; $30BC
        STY $4FE3                       ; $30BF
        DEC $4FE3                       ; $30C2
        DEC $4FE6                       ; $30C5
L_30C8:
        LDA #$FD                        ; $30C8
        STA ($04),Y                     ; $30CA
L_30CC:
        LDA $05                         ; $30CC
        SEC                             ; $30CE
        SBC #$4C                        ; $30CF
        STA $05                         ; $30D1
        RTS                             ; $30D3
; Carry clear means the exit is allowed. Room $0B always is. Otherwise the
; actor has to be leaving through the edge he is standing on: xpos below $10
; with $4F5D set, xpos above $A1 with $4F60 set, ypos above $C5 while the
; state is $0E, or ypos below $31 with $4F57 set. $4F5A, the down counter,
; is not read here.
L_30D4:
        LDA $29                         ; $30D4
        CMP #$0B                        ; $30D6
        BEQ L_310A                      ; $30D8
        SEC                             ; $30DA
        LDA xpos,X                      ; $30DB
        CMP #$10                        ; $30DD
        BCS L_30E6                      ; $30DF
        LDA $4F5D,X                     ; $30E1
        BNE L_310A                      ; $30E4
L_30E6:
        LDA #$A1                       ; $30E6
        CMP xpos,X                      ; $30E8
        BCS L_30F1                      ; $30EA
        LDA $4F60,X                     ; $30EC
        BNE L_310A                      ; $30EF
L_30F1:
        LDA #$C5                        ; $30F1
        CMP ypos,X                      ; $30F3
        BCS L_30FD                      ; $30F5
        LDA state,X                     ; $30F7
        CMP #$0E                        ; $30F9
        BEQ L_310A                      ; $30FB
L_30FD:
        LDA ypos,X                      ; $30FD
        CMP #$31                        ; $30FF
        BCS L_3108                      ; $3101
        LDA $4F57,X                     ; $3103
        BNE L_310A                      ; $3106
L_3108:
        SEC                             ; $3108
        RTS                             ; $3109
L_310A:
        CLC                             ; $310A
        RTS                             ; $310B
L_310C:
        LDA state,X                     ; $310C
        CMP #$19                        ; $310E
        BEQ L_3127                      ; $3110
        LDA hit,X                       ; $3112
        AND #$04                        ; $3114
        BEQ L_3127                      ; $3116
        LDA #$17                        ; $3118
        STA state,X                     ; $311A
        LDA #$10                        ; $311C
        STA timer,X                     ; $311E
        LDA #$00                        ; $3120
        STA $0106,X                     ; $3122
        STA $AA,X                       ; $3125
L_3127:
        LDA ($04),Y                     ; $3127
        JMP L_2EA9                      ; $3129
; Add the three bytes at $25 into the score at $41. $41 is the high byte and
; $43 is the low byte. Each score byte is then compared with the byte at
; $4392. When none is below its pair, $28, the lives, is incremented and the
; bytes at $438E are added into $4392. The score digits are redrawn either way.
apply_score:
        LDX #$02                        ; $312C
        CLC                             ; $312E
L_312F:
        LDA $41,X                       ; $312F
        ADC $25,X                       ; $3131
        STA $41,X                       ; $3133
        DEX                             ; $3135
        BPL L_312F                      ; $3136
        LDX #$00                        ; $3138
L_313A:
        LDA $41,X                       ; $313A
        CMP $4392,X                     ; $313C
        BCC L_315A                      ; $313F
        INX                             ; $3141
        CPX #$03                        ; $3142
        BCC L_313A                      ; $3144
        LDX #$02                        ; $3146
        CLC                             ; $3148
L_3149:
        LDA $4392,X                     ; $3149
        ADC $438E,X                     ; $314C
        STA $4392,X                     ; $314F
        DEX                             ; $3152
        BPL L_3149                      ; $3153
        INC $28                         ; $3155
        JSR lives_to_bcd                      ; $3157
L_315A:
        JSR stash_scores                      ; $315A
        LDY #$30                        ; $315D
        LDA $439C                       ; $315F
        JSR draw_split_digits                      ; $3162
        LDY #$32                        ; $3165
        LDA $439D                       ; $3167
        JSR draw_split_digits                      ; $316A
        LDY #$34                        ; $316D
        LDA $439E                       ; $316F
        JMP draw_split_digits                      ; $3172
; Add A to score byte X. X = 2 reaches $43, the low byte. A lamp passes
; $7D with X = 2.
add_score:
        PHA                             ; $3175
        LDA #$00                        ; $3176
        STA $25                         ; $3178
        STA $26                         ; $317A
        STA $27                         ; $317C
        PLA                             ; $317E
        STA $25,X                       ; $317F
        JMP apply_score                      ; $3181
; Every frame. With $0104 clear, redraw the score. In play, copy $41 over
; the top score at $4388 when the top is smaller, compared from the high
; byte, and redraw both.
refresh_top:
        LDA $0104                       ; $3184
        BEQ L_315A                      ; $3187
        LDA $CF7B                       ; $3189
        EOR $CF9B                       ; $318C
        BNE L_31A4                      ; $318F
        LDA $CF7E                       ; $3191
        EOR $CF9E                       ; $3194
        BNE L_31A4                      ; $3197
        LDA $CF81                       ; $3199
        EOR $CFA1                       ; $319C
        BNE L_31A4                      ; $319F
        NOP                             ; $31A1
        NOP                             ; $31A2
        NOP                             ; $31A3
L_31A4:
        LDX #$00                        ; $31A4
L_31A6:
        LDA $4388,X                     ; $31A6
        CMP $41,X                       ; $31A9
        BCC L_31B8                      ; $31AB
        BEQ L_31B1                      ; $31AD
        BCS L_315A                      ; $31AF
L_31B1:
        INX                             ; $31B1
        CPX #$03                        ; $31B2
        BCC L_31A6                      ; $31B4
        BCS L_315A                      ; $31B6
L_31B8:
        LDX #$03                        ; $31B8
L_31BA:
        LDA $41,X                       ; $31BA
        STA $4388,X                     ; $31BC
        DEX                             ; $31BF
        BPL L_31BA                      ; $31C0
L_31C2:
        LDX #$03                        ; $31C2
L_31C4:
        LDA $4388,X                     ; $31C4
        STA $4398,X                     ; $31C7
        LDA #$00                        ; $31CA
        STA $439B,X                     ; $31CC
        DEX                             ; $31CF
        BPL L_31C4                      ; $31D0
        STA $439E                       ; $31D2
        PHP                             ; $31D5
        SED                             ; $31D6
        LDX #$17                        ; $31D7
L_31D9:
        ASL $439A                       ; $31D9
        ROL $4399                       ; $31DC
        ROL $4398                       ; $31DF
        LDY #$03                        ; $31E2
L_31E4:
        LDA $439B,Y                     ; $31E4
        ADC $439B,Y                     ; $31E7
        STA $439B,Y                     ; $31EA
        DEY                             ; $31ED
        BPL L_31E4                      ; $31EE
        DEX                             ; $31F0
        BPL L_31D9                      ; $31F1
        PLP                             ; $31F3
        LDY #$3C                        ; $31F4
        LDA $439C                       ; $31F6
        JSR draw_split_digits                      ; $31F9
        LDY #$3E                        ; $31FC
        LDA $439D                       ; $31FE
        JSR draw_split_digits                      ; $3201
        LDY #$40                        ; $3204
        LDA $439E                       ; $3206
        JMP draw_split_digits                      ; $3209
        .byte $B0, $04

; A is shifted down by four. Used before a digit is drawn.
high_nibble:
        LSR A                           ; $320E
        LSR A                           ; $320F
        LSR A                           ; $3210
        LSR A                           ; $3211
        AND #$0F                        ; $3212
        RTS                             ; $3214
        .byte $A6, $29

; X is the room number. The room's entry bytes are read from $4BAE and $4BC2.
enter_room_at:
        LDA #$00                        ; $3217
        STA $51                         ; $3219
        TXA                             ; $321B
        STA $4E                         ; $321C
        LDA $4BC2,X                     ; $321E
        TAY                             ; $3221
        LDA $4BAE,X                     ; $3222
        JMP L_2F8E                      ; $3225

; $3228  180 bytes of data between routines

; Copy the three score bytes at $41 into the display copy at $4398.
stash_scores:
        LDX #$02                        ; $32DC
L_32DE:
        LDA $41,X                       ; $32DE
        STA $4398,X                     ; $32E0
        LDA #$00                        ; $32E3
        STA $439B,X                     ; $32E5
        DEX                             ; $32E8
        BPL L_32DE                      ; $32E9
        STA $439E                       ; $32EB
        PHP                             ; $32EE
        SED                             ; $32EF
        LDX #$17                        ; $32F0
L_32F2:
        ASL $439A                       ; $32F2
        ROL $4399                       ; $32F5
        ROL $4398                       ; $32F8
        LDY #$03                        ; $32FB
L_32FD:
        LDA $439B,Y                     ; $32FD
        ADC $439B,Y                     ; $3300
        STA $439B,Y                     ; $3303
        DEY                             ; $3306
        BPL L_32FD                      ; $3307
        DEX                             ; $3309
        BPL L_32F2                      ; $330A
        PLP                             ; $330C
        RTS                             ; $330D
; Keep A, X, and Y in $57-$59. take_lamp brackets claim_lamp with these.
stash_axy:
        STA $57                         ; $330E
        STX $58                         ; $3310
        STY $59                         ; $3312
        RTS                             ; $3314
restore_axy:
        LDA $57                         ; $3315
        LDX $58                         ; $3317
        LDY $59                         ; $3319
        RTS                             ; $331B
; The room in $29 selects a list through $4CE2. A record is four bytes: a
; flag (0 means taken, and the high bit means the end), the replacement tile
; with its high bit set, the column, and the tile row. $5A is the column
; under the scan and $5B is the tile row. A match stores 0 in the flag,
; decrements the room's remaining count at $2A,X, and returns the replacement
; with the high bit clear. Carry set means there was no match. The remaining
; counts copied to $2A are the twenty bytes at $4CCE. Rooms 13 and 19 have a
; dummy record whose column and row are 0. Room 0's list is at $4D0C: columns
; 31 and 38 of tile row 7, and columns 2, 25, 29, and 38 of tile row 10. The
; mounts are $1F and $12. The other lists are in rooms.asm.
claim_lamp:
        LDX $51                         ; $331C
        LDA $E9,X                       ; $331E
        STA $5B                         ; $3320
        STY $5A                         ; $3322
        LDA $29                         ; $3324
        ASL A                           ; $3326
        TAX                             ; $3327
        LDA $4CE2,X                     ; $3328
        STA $14                         ; $332B
        LDA $4CE3,X                     ; $332D
        STA $15                         ; $3330
        LDY #$00                        ; $3332
L_3334:
        LDA #$01                        ; $3334
        STA $5C                         ; $3336
        LDA ($14),Y                     ; $3338
        BMI L_335A                      ; $333A
        BNE L_3340                      ; $333C
        LSR $5C                         ; $333E
L_3340:
        INY                             ; $3340
        INY                             ; $3341
        LDA ($14),Y                     ; $3342
        CMP $5A                         ; $3344
        BEQ L_334A                      ; $3346
        LSR $5C                         ; $3348
L_334A:
        INY                             ; $334A
        LDA ($14),Y                     ; $334B
        CMP $5B                         ; $334D
        BEQ L_3353                      ; $334F
        LSR $5C                         ; $3351
L_3353:
        LDA $5C                         ; $3353
        BNE L_335C                      ; $3355
        INY                             ; $3357
        BNE L_3334                      ; $3358
L_335A:
        SEC                             ; $335A
        RTS                             ; $335B
L_335C:
        DEY                             ; $335C
        DEY                             ; $335D
        DEY                             ; $335E
        LDA #$00                        ; $335F
        STA ($14),Y                     ; $3361
        LDX $29                         ; $3363
        DEC $2A,X                       ; $3365
        INY                             ; $3367
        LDA ($14),Y                     ; $3368
        AND #$7F                        ; $336A
        CLC                             ; $336C
        RTS                             ; $336D
L_336E:
        LDA $29                         ; $336E
        ASL A                           ; $3370
        TAX                             ; $3371
        LDA $4CE2,X                     ; $3372
        STA $14                         ; $3375
        LDA $4CE3,X                     ; $3377
        STA $15                         ; $337A
        LDY #$00                        ; $337C
L_337E:
        LDA ($14),Y                     ; $337E
        BMI L_33B3                      ; $3380
        BNE L_33AC                      ; $3382
        INY                             ; $3384
        LDA ($14),Y                     ; $3385
        PHA                             ; $3387
        INY                             ; $3388
        LDA ($14),Y                     ; $3389
        PHA                             ; $338B
        INY                             ; $338C
        STY $52                         ; $338D
        LDA ($14),Y                     ; $338F
        TAX                             ; $3391
        JSR point_at_row                      ; $3392
        PLA                             ; $3395
        TAY                             ; $3396
        PLA                             ; $3397
        AND #$7F                        ; $3398
        STA ($04),Y                     ; $339A
        ORA #$80                        ; $339C
        PHA                             ; $339E
        TYA                             ; $339F
        CLC                             ; $33A0
        ADC #$28                        ; $33A1
        TAY                             ; $33A3
        PLA                             ; $33A4
        STA ($04),Y                     ; $33A5
        LDY $52                         ; $33A7
        JMP L_33AF                      ; $33A9
L_33AC:
        INY                             ; $33AC
        INY                             ; $33AD
        INY                             ; $33AE
L_33AF:
        INY                             ; $33AF
        JMP L_337E                      ; $33B0
L_33B3:
        LDX #$04                        ; $33B3
        LDA #$00                        ; $33B5
L_33B7:
        STA $4E97,X                     ; $33B7
        DEX                             ; $33BA
        BPL L_33B7                      ; $33BB
        LDA $29                         ; $33BD
        ASL A                           ; $33BF
        TAX                             ; $33C0
        LDA $4E9D,X                     ; $33C1
        PHA                             ; $33C4
        LDA $4E9C,X                     ; $33C5
        PHA                             ; $33C8
        RTS                             ; $33C9
L_33CA:
        LDA #$13                        ; $33CA
        STA $4E                         ; $33CC
L_33CE:
        LDA $4E                         ; $33CE
        ASL A                           ; $33D0
        TAX                             ; $33D1
        JSR L_33DA                      ; $33D2
        DEC $4E                         ; $33D5
        BPL L_33CE                      ; $33D7
        RTS                             ; $33D9
L_33DA:
        LDA $4CE2,X                     ; $33DA
        STA $14                         ; $33DD
        LDA $4CE3,X                     ; $33DF
        STA $15                         ; $33E2
        LDY #$00                        ; $33E4
L_33E6:
        LDA ($14),Y                     ; $33E6
        BMI L_33F4                      ; $33E8
        LDA #$01                        ; $33EA
        STA ($14),Y                     ; $33EC
        JSR L_3A63                      ; $33EE
        JMP L_33E6                      ; $33F1
L_33F4:
        RTS                             ; $33F4

; $33F5  1646 bytes of data between routines

L_3A63:
        LDA $14                         ; $3A63
        CLC                             ; $3A65
        ADC #$04                        ; $3A66
        STA $14                         ; $3A68
        BCC L_3A6E                      ; $3A6A
        INC $15                         ; $3A6C
L_3A6E:
        RTS                             ; $3A6E
; Once per logic frame, from frame, and once more at the end of enter_room.
; The word at $4EC4, room*2 is the hook address minus one, because RTS adds
; one. Room 1's hook opens the trap door. The pictures and the lamp test are
; in rooms.asm.
L_3A6F:
        LDA $29                         ; $3A6F
        ASL A                           ; $3A71
        TAX                             ; $3A72
        LDA $4EC5,X                     ; $3A73
        PHA                             ; $3A76
        LDA $4EC4,X                     ; $3A77
        PHA                             ; $3A7A
        RTS                             ; $3A7B
L_3A7C:
        LDA $0A                         ; $3A7C
        CLC                             ; $3A7E
        ADC #$04                        ; $3A7F
        STA $0A                         ; $3A81
        LDA $0B                         ; $3A83
        ADC #$00                        ; $3A85
        STA $0B                         ; $3A87
        RTS                             ; $3A89

; $3A8A  1523 bytes of data between routines

L_407D:
        LDA #$19                        ; $407D
        STA state,X                     ; $407F
        LDA #$10                        ; $4081
        STA timer,X                     ; $4083
        RTS                             ; $4085

; $4086  54 bytes of data between routines

; Once per logic frame, from frame, before the overlap. $0101, $0102, and
; $0103 each reload to a count from 1 to 15. The new row comes from
; noise_byte. The display charset is at $A000.
; $0101 writes glyph $93, bytes 2 and 3 ($A49A). That is the shadow of tile
; $13. Noise masked with $30 is combined with $CC, so one dot cycles through
; the four multicolor pairs. The same byte masked with $B8 goes to glyph $86.
; $0102 does the $30/$CC write to glyph $B1 ($A58A), the shadow of tile $31.
; $0103 masks with $0C, combines with $33, and stores glyph $94 ($A4A2), the
; shadow of tile $14. Masked with $2E, that byte also goes to glyph $E7.
; The $A800 copies ($AC9A, $AD8A, $ACA2) receive the mask by itself, and
; glyphs $86 and $E7 are not written there. Charset $B000, selected for room
; $12, is not written, so those lamps stay still. This snapshot's room shows
; charset $A000, and its lamps are tiles $14 and $31, so those two shapes
; flicker. Lamps of one shape share the glyph and change together.
flicker_lamps:
        DEC $0101                       ; $40BC
        BNE L_40E6                      ; $40BF
L_40C1:
        JSR noise_byte                      ; $40C1
        AND #$0F                        ; $40C4
        BEQ L_40C1                      ; $40C6
        STA $0101                       ; $40C8
        JSR noise_byte                      ; $40CB
        AND #$30                        ; $40CE
        STA $AC9A                       ; $40D0
        STA $AC9B                       ; $40D3
        ORA #$CC                        ; $40D6
        STA $A49A                       ; $40D8
        STA $A49B                       ; $40DB
        AND #$B8                        ; $40DE
        STA $A432                       ; $40E0
        STA $A433                       ; $40E3
L_40E6:
        DEC $0102                       ; $40E6
        BNE L_4108                      ; $40E9
L_40EB:
        JSR noise_byte                      ; $40EB
        AND #$0F                        ; $40EE
        BEQ L_40EB                      ; $40F0
        STA $0102                       ; $40F2
        JSR noise_byte                      ; $40F5
        AND #$30                        ; $40F8
        STA $AD8A                       ; $40FA
        STA $AD8B                       ; $40FD
        ORA #$CC                        ; $4100
        STA $A58A                       ; $4102
        STA $A58B                       ; $4105
L_4108:
        DEC $0103                       ; $4108
        BNE L_4132                      ; $410B
L_410D:
        JSR noise_byte                      ; $410D
        AND #$0F                        ; $4110
        BEQ L_410D                      ; $4112
        STA $0103                       ; $4114
        JSR noise_byte                      ; $4117
        AND #$0C                        ; $411A
        STA $ACA2                       ; $411C
        STA $ACA3                       ; $411F
        ORA #$33                        ; $4122
        STA $A4A2                       ; $4124
        STA $A4A3                       ; $4127
        AND #$2E                        ; $412A
        STA $A73A                       ; $412C
        STA $A73B                       ; $412F
L_4132:
        RTS                             ; $4132

; $4133  268 bytes of data between routines

L_423F:
        LDX #$09                        ; $423F
        LDA #$00                        ; $4241
L_4243:
        STA $8340,X                     ; $4243
        STA $8380,X                     ; $4246
        DEX                             ; $4249
        DEX                             ; $424A
        DEX                             ; $424B
        BPL L_4243                      ; $424C
        RTS                             ; $424E

; $424F  1335 bytes of data between routines

L_4786:
        INC $04                         ; $4786
        BNE L_478C                      ; $4788
        INC $05                         ; $478A
L_478C:
        RTS                             ; $478C
L_478D:
        INC $06                         ; $478D
        BNE L_4793                      ; $478F
        INC $07                         ; $4791
L_4793:
        RTS                             ; $4793
L_4794:
        INC $08                         ; $4794
        BNE L_479A                      ; $4796
        INC $09                         ; $4798
L_479A:
        RTS                             ; $479A

; $479B  192 bytes of data between routines

L_485B:
        PHA                             ; $485B
        DEC $F3                         ; $485C
        BPL L_4890                      ; $485E
        LDA #$27                        ; $4860
        STA $F3                         ; $4862
        LDA $04                         ; $4864
        CLC                             ; $4866
        ADC #$28                        ; $4867
        STA $04                         ; $4869
        BCC L_486F                      ; $486B
        INC $05                         ; $486D
L_486F:
        LDA $08                         ; $486F
        CLC                             ; $4871
        ADC #$28                        ; $4872
        STA $08                         ; $4874
        BCC L_487A                      ; $4876
        INC $09                         ; $4878
L_487A:
        LDA $10                         ; $487A
        CLC                             ; $487C
        ADC #$28                        ; $487D
        STA $10                         ; $487F
        BCC L_4885                      ; $4881
        INC $11                         ; $4883
L_4885:
        LDA $12                         ; $4885
        CLC                             ; $4887
        ADC #$28                        ; $4888
        STA $12                         ; $488A
        BCC L_4890                      ; $488C
        INC $13                         ; $488E
L_4890:
        PLA                             ; $4890
        RTS                             ; $4891

; Double the packed glyphs. Four source bytes are written twice through ($06),
; and the next four twice through ($08). The caller sets $F3 to the number of
; groups minus one. Startup at $047A calls this four times. The setups and
; the fact that $9000 is later overwritten are in rooms.asm. A picture row
; occupies two charset bytes because of this, which is why flicker_lamps
; stores its new value into both bytes of the pair.
expand_charset:
        LDY #$00                        ; $4892
        LDX #$03                        ; $4894
L_4896:
        LDA ($04),Y                     ; $4896
        STA ($06),Y                     ; $4898
        JSR L_478D                      ; $489A
        STA ($06),Y                     ; $489D
        JSR L_478D                      ; $489F
        JSR L_4786                      ; $48A2
        DEX                             ; $48A5
        BPL L_4896                      ; $48A6
        LDX #$03                        ; $48A8
L_48AA:
        LDA ($04),Y                     ; $48AA
        STA ($08),Y                     ; $48AC
        JSR L_4794                      ; $48AE
        STA ($08),Y                     ; $48B1
        JSR L_4794                      ; $48B3
        JSR L_4786                      ; $48B6
        DEX                             ; $48B9
        BPL L_48AA                      ; $48BA
        DEC $F3                         ; $48BC
        BPL expand_charset              ; $48BE
        RTS                             ; $48C0

; In-game raster interrupt. arm_raster points here and compares against raster
; $FB. This pass selects charset $8800 ($D018 = $32), which is the score font,
; and arms raster $40. The room index in $29 picks two tables. $480B, room*2
; is twelve color bytes, copied to $0153: four bands of $D021, $D022, $D023.
; $4833, room*2 is the split list, and only its first four bytes are copied to
; $47F5. Room 0's splits are $78, $97, and $FD. $FD ends the chain. Several
; rooms share one list; the pointers are in rooms.asm. Each color byte is
; stored with a high nibble of $F, and the VIC uses the low nibble.
; irq_service
; runs before the RTI, and that is the only pass that counts $48 down.
raster_irq:
        LDA #$32                        ; $48C1
        STA $D018                       ; $48C3
        LDA #$FC                        ; $48C6
        STA $D021                       ; $48C8
; The next interrupt is raster_open, at raster $40.
        LDA #$1E                        ; $48CB
        STA $0408                       ; $48CD
        LDA #$49                        ; $48D0
        STA $0409                       ; $48D2
        LDA #$01                        ; $48D5
        STA $D01A                       ; $48D7
        STA $D019                       ; $48DA
        LDA $D011                       ; $48DD
        AND #$7F                        ; $48E0
        STA $D011                       ; $48E2
        LDA #$40                        ; $48E5
        STA $D012                       ; $48E7
        LDA $29                         ; $48EA
        ASL A                           ; $48EC
        TAX                             ; $48ED
        LDA $480B,X                     ; $48EE
        STA $0A                         ; $48F1
        LDA $480C,X                     ; $48F3
        STA $0B                         ; $48F6
        LDA $4833,X                     ; $48F8
        STA $0C                         ; $48FB
        LDA $4834,X                     ; $48FD
        STA $0D                         ; $4900
        LDY #$0B                        ; $4902
L_4904:
        LDA ($0A),Y                     ; $4904
        STA $0153,Y                     ; $4906
        CPY #$04                        ; $4909
        BCS L_4912                      ; $490B
        LDA ($0C),Y                     ; $490D
        STA $47F5,Y                     ; $490F
L_4912:
        DEY                             ; $4912
        BPL L_4904                      ; $4913
        JSR $15A8                       ; $4915
        PLA                             ; $4918
        TAY                             ; $4919
        PLA                             ; $491A
        TAX                             ; $491B
        PLA                             ; $491C
        RTI                             ; $491D

; Raster $40, just above the picture. $F2 is restored to $D018. In this room
; that is $38, charset $A000. The first band is then taken from $0153.
raster_open:
        LDA #$41                        ; $491E
L_4920:
        CMP $D012                       ; $4920
        BCS L_4920                      ; $4923
        LDX #$0A                        ; $4925
L_4927:
        DEX                             ; $4927
        BNE L_4927                      ; $4928
        LDA $F2                         ; $492A
        STA $D018                       ; $492C
        LDA $0153                       ; $492F
        STA $D021                       ; $4932
        LDA $0154                       ; $4935
        STA $D022                       ; $4938
        LDA $0155                       ; $493B
        STA $D023                       ; $493E
        LDA #$00                        ; $4941
        STA $44                         ; $4943
        LDA #$03                        ; $4945
        STA $F4                         ; $4947
; The splits after this one are raster_split.
        LDA #$82                        ; $4949
        STA $0408                       ; $494B
        LDA #$49                        ; $494E
        STA $0409                       ; $4950

; Arm the next split from $47F5. A value of $FD points the vector back at
; raster_irq instead of raster_split.
raster_arm:
        LDA #$01                        ; $4953
        STA $D01A                       ; $4955
        STA $D019                       ; $4958
        LDA $D011                       ; $495B
        AND #$7F                        ; $495E
        STA $D011                       ; $4960
        LDX $44                         ; $4963
        LDA $47F5,X                     ; $4965
        STA $D012                       ; $4968
        CMP #$FD                        ; $496B
        BEQ L_4975                      ; $496D
L_496F:
        PLA                             ; $496F
        TAY                             ; $4970
        PLA                             ; $4971
        TAX                             ; $4972
        PLA                             ; $4973
        RTI                             ; $4974
L_4975:
; The split was $FD. Point the vector back at raster_irq.
        LDA #$C1                        ; $4975
        STA $0408                       ; $4977
        LDA #$48                        ; $497A
        STA $0409                       ; $497C
        JMP L_496F                      ; $497F

; A color split. Wait until the raster in the list, then store the next three
; bytes as $D021, $D022, and $D023. $F4 starts at 3 and advances by 3, so the
; bytes are band 2, then band 3, then band 4.
raster_split:
        LDX $F4                         ; $4982
        LDY $0153,X                     ; $4984
        LDX $44                         ; $4987
        LDA $47F5,X                     ; $4989
        TAX                             ; $498C
        INX                             ; $498D
        INX                             ; $498E
        TXA                             ; $498F
        INX                             ; $4990
L_4991:
        CPX $D012                       ; $4991
        BEQ L_49A0                      ; $4994
        CMP $D012                       ; $4996
        BNE L_4991                      ; $4999
        LDX #$0A                        ; $499B
L_499D:
        DEX                             ; $499D
        BNE L_499D                      ; $499E
L_49A0:
        STY $D021                       ; $49A0
        LDX $F4                         ; $49A3
        LDA $0154,X                     ; $49A5
        STA $D022                       ; $49A8
        LDA $0155,X                     ; $49AB
        STA $D023                       ; $49AE
        LDA $F4                         ; $49B1
        CLC                             ; $49B3
        ADC #$03                        ; $49B4
        STA $F4                         ; $49B6
        INC $44                         ; $49B8
        JMP raster_arm                  ; $49BA

; $49BD  1822 bytes of data between routines

; Bit 4 of $D011 turns the screen on. Bit 7 stays clear, so the raster compare uses $D012
; alone.
show_screen:
        LDA $D011                       ; $50DB
        AND #$7F                        ; $50DE
        ORA #$10                        ; $50E0
        STA $D011                       ; $50E2
        RTS                             ; $50E5

; $50E6  300 bytes of data between routines


; For each active actor, place his box over the room and scan it. xpos and ypos are
; converted to a character-cell cursor. scan_row is called once per row of the box.
overlap_actors:
        LDX #$02                        ; $5212
L_5214:
        STX $50                         ; $5214
        LDA active,X                    ; $5216
        AND #$01                        ; $5218
        BNE L_521F                      ; $521A
        JMP L_52A4                      ; $521C
L_521F:
        LDA xpos,X                      ; $521F
        SEC                             ; $5221
        SBC #$0C                        ; $5222
        STA $E6,X                       ; $5224
        AND #$03                        ; $5226
        STA $1B                         ; $5228
        LDA ypos,X                      ; $522A
        SEC                             ; $522C
        SBC #$1D                        ; $522D
        STA $E9,X                       ; $522F
        AND #$07                        ; $5231
        STA $1C                         ; $5233
        LDA $4F4E,X                     ; $5235
        TAX                             ; $5238
        LDA $1167,X                     ; $5239
        STA $52CC                       ; $523C
        LDA $1239,X                     ; $523F
        STA $52CD                       ; $5242
        JSR L_52AD                      ; $5245
        LDA $1C                         ; $5248
        CLC                             ; $524A
        ADC #$15                        ; $524B
        STA $4F39                       ; $524D
        SEC                             ; $5250
        SBC #$06                        ; $5251
        STA $4F38                       ; $5253
        JSR L_55AB                      ; $5256
        LDX $50                         ; $5259
        LDA $E6,X                       ; $525B
        LSR A                           ; $525D
        LSR A                           ; $525E
        STA $E6,X                       ; $525F
        LDA $E9,X                       ; $5261
        LSR A                           ; $5263
        LSR A                           ; $5264
        LSR A                           ; $5265
        STA $E9,X                       ; $5266
        LDY #$00                        ; $5268
        STY $52                         ; $526A
        LDA #$08                        ; $526C
        STA $4F3A                       ; $526E
        JSR L_530A                      ; $5271
        JSR scan_row                    ; $5274
        LDX $50                         ; $5277
        INC $E9,X                       ; $5279
        LDA #$10                        ; $527B
        STA $4F3A                       ; $527D
        JSR L_530A                      ; $5280
        JSR scan_row                    ; $5283
        LDX $50                         ; $5286
        INC $E9,X                       ; $5288
        LDA #$18                        ; $528A
        STA $4F3A                       ; $528C
        JSR L_530A                      ; $528F
        JSR scan_row                    ; $5292
        LDX $50                         ; $5295
        INC $E9,X                       ; $5297
        LDA #$20                        ; $5299
        STA $4F3A                       ; $529B
        JSR L_530A                      ; $529E
        JSR scan_row                    ; $52A1
L_52A4:
        LDX $50                         ; $52A4
        DEX                             ; $52A6
        BMI L_52AC                      ; $52A7
        JMP L_5214                      ; $52A9
L_52AC:
        RTS                             ; $52AC
L_52AD:
        LDA #$00                        ; $52AD
        TAY                             ; $52AF
L_52B0:
        CPY $1C                         ; $52B0
        BEQ L_52BA                      ; $52B2
        STA $CAE0,Y                     ; $52B4
        INY                             ; $52B7
        BNE L_52B0                      ; $52B8
L_52BA:
        LDX $1B                         ; $52BA
        CPX #$03                        ; $52BC
        BNE L_52C9                      ; $52BE
        LDX #$1F                        ; $52C0
L_52C2:
        STA $CAE0,Y                     ; $52C2
        INY                             ; $52C5
        DEX                             ; $52C6
        BPL L_52C2                      ; $52C7
L_52C9:
        LDX #$00                        ; $52C9
L_52CB:
        LDA $9060,X                     ; $52CB
        STA $CAE0,Y                     ; $52CE
        INY                             ; $52D1
        INX                             ; $52D2
        CPX #$60                        ; $52D3
        BCC L_52CB                      ; $52D5
        LDA $1B                         ; $52D7
        BEQ L_52F2                      ; $52D9
        CMP #$03                        ; $52DB
        BEQ L_52F3                      ; $52DD
        ASL A                           ; $52DF
        TAY                             ; $52E0
L_52E1:
        LDX #$1F                        ; $52E1
L_52E3:
        LSR $CAE0,X                     ; $52E3
        ROR $CB00,X                     ; $52E6
        ROR $CB20,X                     ; $52E9
        DEX                             ; $52EC
        BPL L_52E3                      ; $52ED
        DEY                             ; $52EF
        BNE L_52E1                      ; $52F0
L_52F2:
        RTS                             ; $52F2
L_52F3:
        LDY #$02                        ; $52F3
L_52F5:
        LDX #$1F                        ; $52F5
L_52F7:
        ASL $CB40,X                     ; $52F7
        ROL $CB20,X                     ; $52FA
        ROL $CB00,X                     ; $52FD
        ROL $CAE0,X                     ; $5300
        DEX                             ; $5303
        BPL L_52F7                      ; $5304
        DEY                             ; $5306
        BNE L_52F5                      ; $5307
        RTS                             ; $5309
L_530A:
        LDX $50                         ; $530A
        LDA $E9,X                       ; $530C
        TAX                             ; $530E
        JSR L_55C5                      ; $530F
        LDX $50                         ; $5312
        LDY $E6,X                       ; $5314
        JSR L_55D7                      ; $5316
        LDA ($04),Y                     ; $5319
        STA $43BC                       ; $531B
        JSR scale_by_eight                      ; $531E
        LDA $43BC                       ; $5321
        SEC                             ; $5324
        SBC $52                         ; $5325
        STA $5381                       ; $5327
        LDA $43BB                       ; $532A
        SBC #$00                        ; $532D
        CLC                             ; $532F
        ADC $7C                         ; $5330
        STA $5382                       ; $5332
        INY                             ; $5335
        JSR L_5649                      ; $5336
        LDA ($04),Y                     ; $5339
        STA $43BC                       ; $533B
        JSR scale_by_eight                      ; $533E
        LDA $43BC                       ; $5341
        SEC                             ; $5344
        SBC $52                         ; $5345
        STA $539B                       ; $5347
        LDA $43BB                       ; $534A
        SBC #$00                        ; $534D
        CLC                             ; $534F
        ADC $7C                         ; $5350
        STA $539C                       ; $5352
        INY                             ; $5355
        CPY #$28                        ; $5356
        BCC L_535E                      ; $5358
        LDA #$00                        ; $535A
        BEQ L_5363                      ; $535C
L_535E:
        JSR L_56BB                      ; $535E
        LDA ($04),Y                     ; $5361
L_5363:
        STA $43BC                       ; $5363
        JSR scale_by_eight                      ; $5366
        LDA $43BC                       ; $5369
        SEC                             ; $536C
        SBC $52                         ; $536D
        STA $53B5                       ; $536F
        LDA $43BB                       ; $5372
        SBC #$00                        ; $5375
        CLC                             ; $5377
        ADC $7C                         ; $5378
        STA $53B6                       ; $537A
        RTS                             ; $537D

; One row of the overlap test. The sprite row is ANDed with the room row under it. Each
; half of that byte is a 4-bit pattern, and the slot for that pattern is marked.
scan_row:
        LDY $52                         ; $537E
L_5380:
        LDA $A470,Y                     ; $5380
        AND $CAE0,Y                     ; $5383
        BEQ L_539A                      ; $5386
        STA $4E                         ; $5388
        LSR A                           ; $538A
        LSR A                           ; $538B
        LSR A                           ; $538C
        LSR A                           ; $538D
        TAX                             ; $538E
        STA $4EED,X                     ; $538F
        LDA $4E                         ; $5392
        AND #$0F                        ; $5394
        TAX                             ; $5396
        STA $4EED,X                     ; $5397
L_539A:
        LDA $A470,Y                     ; $539A
        AND $CB00,Y                     ; $539D
        BEQ L_53B4                      ; $53A0
        STA $4E                         ; $53A2
        LSR A                           ; $53A4
        LSR A                           ; $53A5
        LSR A                           ; $53A6
        LSR A                           ; $53A7
        TAX                             ; $53A8
        STA $4F04,X                     ; $53A9
        LDA $4E                         ; $53AC
        AND #$0F                        ; $53AE
        TAX                             ; $53B0
        STA $4F04,X                     ; $53B1
L_53B4:
        LDA $A470,Y                     ; $53B4
        AND $CB20,Y                     ; $53B7
        BEQ L_53CE                      ; $53BA
        STA $4E                         ; $53BC
        LSR A                           ; $53BE
        LSR A                           ; $53BF
        LSR A                           ; $53C0
        LSR A                           ; $53C1
        TAX                             ; $53C2
        STA $4F1B,X                     ; $53C3
        LDA $4E                         ; $53C6
        AND #$0F                        ; $53C8
        TAX                             ; $53CA
        STA $4F1B,X                     ; $53CB
L_53CE:
        INY                             ; $53CE
        CPY $4F38                       ; $53CF
        BEQ L_53E4                      ; $53D2
L_53D4:
        CPY $4F39                       ; $53D4
        BEQ L_5429                      ; $53D7
        CPY $4F3A                       ; $53D9
        BEQ L_53E1                      ; $53DC
        JMP L_5380                      ; $53DE
L_53E1:
        STY $52                         ; $53E1
        RTS                             ; $53E3
L_53E4:
        LDX #$02                        ; $53E4
L_53E6:
        LDA $4F32,X                     ; $53E6
        STA $4F35,X                     ; $53E9
        DEX                             ; $53EC
        BPL L_53E6                      ; $53ED
        JSR flags_from_nibbles          ; $53EF
        LDA hit,X                       ; $53F2
        STA $A7,X                       ; $53F4
        LDA $4F32                       ; $53F6
        CMP $4F35                       ; $53F9
        BEQ L_5406                      ; $53FC
        EOR #$01                        ; $53FE
        STA $4F32                       ; $5400
        JSR swap_column0                      ; $5403
L_5406:
        LDA $4F33                       ; $5406
        CMP $4F36                       ; $5409
        BEQ L_5416                      ; $540C
        EOR #$01                        ; $540E
        STA $4F33                       ; $5410
        JSR swap_column1                      ; $5413
L_5416:
        LDA $4F34                       ; $5416
        CMP $4F37                       ; $5419
        BEQ L_53D4                      ; $541C
        EOR #$01                        ; $541E
        STA $4F34                       ; $5420
        JSR swap_column2                      ; $5423
        JMP L_53D4                      ; $5426
L_5429:
        JSR flags_from_nibbles          ; $5429
        PLA                             ; $542C
        PLA                             ; $542D
        JMP L_52A4                      ; $542E

; Turn the marked patterns into flags. A set slot means that nibble occurred.
; $4F3C rolls in as bit 0, ground: nibbles 1, 4, 5, 6, 7, 9, and 13.
; $4F3D rolls in as bit 1, prize ink: nibbles 2, 6, 8, 9, and 10.
; $4F3E rolls in as bit 2 and $4F3F as bit 3. Bit 3 is the wall, nibbles
; 3, 7, and 11-15. swap_column0, swap_column1, and swap_column2 run when
; that column's property is not 0, and they exchange the wall slots with
; the ladder slots, so those nibbles become bit 2.
flags_from_nibbles:
        LDA #$00                        ; $5431
        LDX #$04                        ; $5433
L_5435:
        STA $4F3B,X                     ; $5435
        DEX                             ; $5438
        BPL L_5435                      ; $5439
        LDA $4F32                       ; $543B
        BEQ L_5446                      ; $543E
        DEC $4F32                       ; $5440
        JSR swap_column0                      ; $5443
L_5446:
        LDA $4F33                       ; $5446
        BEQ L_5451                      ; $5449
        DEC $4F33                       ; $544B
        JSR swap_column1                      ; $544E
L_5451:
        LDA $4F34                       ; $5451
        BEQ L_545C                      ; $5454
        DEC $4F34                       ; $5456
        JSR swap_column2                      ; $5459
L_545C:
        LDA $4EEE                       ; $545C
        ORA $4EF1                       ; $545F
        ORA $4EF2                       ; $5462
        ORA $4EF3                       ; $5465
        ORA $4EF4                       ; $5468
        ORA $4EFE                       ; $546B
        ORA $4EF6                       ; $546E
        ORA $4EFA                       ; $5471
        ORA $4F01                       ; $5474
        ORA $4F05                       ; $5477
        ORA $4F08                       ; $547A
        ORA $4F09                       ; $547D
        ORA $4F0A                       ; $5480
        ORA $4F0B                       ; $5483
        ORA $4EFE                       ; $5486
        ORA $4F0D                       ; $5489
        ORA $4F11                       ; $548C
        ORA $4F18                       ; $548F
        ORA $4F1C                       ; $5492
        ORA $4F1F                       ; $5495
        ORA $4F20                       ; $5498
        ORA $4F21                       ; $549B
        ORA $4F22                       ; $549E
        ORA $4F2C                       ; $54A1
        ORA $4F24                       ; $54A4
        ORA $4F28                       ; $54A7
        ORA $4F2F                       ; $54AA
        BEQ L_54B2                      ; $54AD
        INC $4F3C                       ; $54AF
L_54B2:
        LDA $4EEF                       ; $54B2
        ORA $4EF3                       ; $54B5
        ORA $4EF5                       ; $54B8
        ORA $4EF6                       ; $54BB
        ORA $4EF7                       ; $54BE
        ORA $4EF8                       ; $54C1
        ORA $4EFB                       ; $54C4
        ORA $4EFF                       ; $54C7
        ORA $4F02                       ; $54CA
        ORA $4F06                       ; $54CD
        ORA $4F0A                       ; $54D0
        ORA $4F0C                       ; $54D3
        ORA $4F0D                       ; $54D6
        ORA $4F0E                       ; $54D9
        ORA $4F0F                       ; $54DC
        ORA $4F12                       ; $54DF
        ORA $4F16                       ; $54E2
        ORA $4F19                       ; $54E5
        ORA $4F1D                       ; $54E8
        ORA $4F21                       ; $54EB
        ORA $4F23                       ; $54EE
        ORA $4F24                       ; $54F1
        ORA $4F25                       ; $54F4
        ORA $4F26                       ; $54F7
        ORA $4F29                       ; $54FA
        ORA $4F2D                       ; $54FD
        ORA $4F30                       ; $5500
        BEQ L_5508                      ; $5503
        INC $4F3D                       ; $5505
L_5508:
        LDA $4EFD                       ; $5508
        ORA $4EFE                       ; $550B
        ORA $4EFF                       ; $550E
        ORA $4F00                       ; $5511
        ORA $4F01                       ; $5514
        ORA $4F02                       ; $5517
        ORA $4F03                       ; $551A
        ORA $4F14                       ; $551D
        ORA $4F15                       ; $5520
        ORA $4F16                       ; $5523
        ORA $4F17                       ; $5526
        ORA $4F18                       ; $5529
        ORA $4F19                       ; $552C
        ORA $4F1A                       ; $552F
        ORA $4F2B                       ; $5532
        ORA $4F2C                       ; $5535
        ORA $4F2D                       ; $5538
        ORA $4F2E                       ; $553B
        ORA $4F2F                       ; $553E
        ORA $4F30                       ; $5541
        ORA $4F31                       ; $5544
        BEQ L_554C                      ; $5547
        INC $4F3E                       ; $5549
L_554C:
        LDA $4EF0                       ; $554C
        ORA $4EF4                       ; $554F
        ORA $4EF8                       ; $5552
        ORA $4EF9                       ; $5555
        ORA $4EFA                       ; $5558
        ORA $4EFB                       ; $555B
        ORA $4EFC                       ; $555E
        ORA $4F07                       ; $5561
        ORA $4F0B                       ; $5564
        ORA $4F0F                       ; $5567
        ORA $4F10                       ; $556A
        ORA $4F11                       ; $556D
        ORA $4F12                       ; $5570
        ORA $4F13                       ; $5573
        ORA $4F1E                       ; $5576
        ORA $4F22                       ; $5579
        ORA $4F26                       ; $557C
        ORA $4F27                       ; $557F
        ORA $4F28                       ; $5582
        ORA $4F29                       ; $5585
        ORA $4F2A                       ; $5588
        BEQ L_5590                      ; $558B
        INC $4F3F                       ; $558D
L_5590:
        LDX $50                         ; $5590
        LDA #$00                        ; $5592
        STA hit,X                       ; $5594
        LSR $4F3F                       ; $5596
        ROL hit,X                       ; $5599
        LSR $4F3E                       ; $559B
        ROL hit,X                       ; $559E
        LSR $4F3D                       ; $55A0
        ROL hit,X                       ; $55A3
        LSR $4F3C                       ; $55A5
        ROL hit,X                       ; $55A8
        RTS                             ; $55AA
L_55AB:
        LDA #$00                        ; $55AB
        LDX #$16                        ; $55AD
L_55AF:
        STA $4EED,X                     ; $55AF
        STA $4F04,X                     ; $55B2
        STA $4F1B,X                     ; $55B5
        DEX                             ; $55B8
        BPL L_55AF                      ; $55B9
        STA $4F32                       ; $55BB
        STA $4F33                       ; $55BE
        STA $4F34                       ; $55C1
        RTS                             ; $55C4
L_55C5:
        LDA $50E7,X                     ; $55C5
        STA $04                         ; $55C8
        STA $12                         ; $55CA
        LDA $50FF,X                     ; $55CC
        STA $05                         ; $55CF
        LDA $5117,X                     ; $55D1
        STA $13                         ; $55D4
        RTS                             ; $55D6
L_55D7:
        LDA ($12),Y                     ; $55D7
        AND #$07                        ; $55D9
        BEQ L_563D                      ; $55DB
        LDA $4F32                       ; $55DD
        BEQ L_55E3                      ; $55E0
        RTS                             ; $55E2
L_55E3:
        INC $4F32                       ; $55E3
; This column's property is not 0. Trade its wall nibble slots for its
; ladder slots, so bit 3 becomes bit 2. The other two columns are
; swap_column1 and swap_column2.
swap_column0:
        LDA $4EF0                       ; $55E6
        LDX $4EFD                       ; $55E9
        STX $4EF0                       ; $55EC
        STA $4EFD                       ; $55EF
        LDA $4EF4                       ; $55F2
        LDX $4EFE                       ; $55F5
        STX $4EF4                       ; $55F8
        STA $4EFE                       ; $55FB
        LDA $4EF8                       ; $55FE
        LDX $4EFF                       ; $5601
        STX $4EF8                       ; $5604
        STA $4EFF                       ; $5607
        LDA $4EF9                       ; $560A
        LDX $4F00                       ; $560D
        STX $4EF9                       ; $5610
        STA $4F00                       ; $5613
        LDA $4EFA                       ; $5616
        LDX $4F01                       ; $5619
        STX $4EFA                       ; $561C
        STA $4F01                       ; $561F
        LDA $4EFB                       ; $5622
        LDX $4F02                       ; $5625
        STX $4EFB                       ; $5628
        STA $4F02                       ; $562B
        LDA $4EFC                       ; $562E
        LDX $4F03                       ; $5631
        STX $4EFC                       ; $5634
        STA $4F03                       ; $5637
        LDX $50                         ; $563A
        RTS                             ; $563C
L_563D:
        LDA $4F32                       ; $563D
        BNE L_5643                      ; $5640
        RTS                             ; $5642
L_5643:
        DEC $4F32                       ; $5643
        JMP swap_column0                      ; $5646
L_5649:
        LDA ($12),Y                     ; $5649
        AND #$07                        ; $564B
        BEQ L_56AF                      ; $564D
        LDA $4F33                       ; $564F
        BEQ L_5655                      ; $5652
        RTS                             ; $5654
L_5655:
        INC $4F33                       ; $5655
swap_column1:
        LDA $4F07                       ; $5658
        LDX $4F14                       ; $565B
        STX $4F07                       ; $565E
        STA $4F14                       ; $5661
        LDA $4F0B                       ; $5664
        LDX $4F15                       ; $5667
        STX $4F0B                       ; $566A
        STA $4F15                       ; $566D
        LDA $4F0F                       ; $5670
        LDX $4F16                       ; $5673
        STX $4F0F                       ; $5676
        STA $4F16                       ; $5679
        LDA $4F10                       ; $567C
        LDX $4F17                       ; $567F
        STX $4F10                       ; $5682
        STA $4F17                       ; $5685
        LDA $4F11                       ; $5688
        LDX $4F18                       ; $568B
        STX $4F11                       ; $568E
        STA $4F18                       ; $5691
        LDA $4F12                       ; $5694
        LDX $4F19                       ; $5697
        STX $4F12                       ; $569A
        STA $4F19                       ; $569D
        LDA $4F13                       ; $56A0
        LDX $4F1A                       ; $56A3
        STX $4F13                       ; $56A6
        STA $4F1A                       ; $56A9
        LDX $50                         ; $56AC
        RTS                             ; $56AE
L_56AF:
        LDA $4F33                       ; $56AF
        BNE L_56B5                      ; $56B2
        RTS                             ; $56B4
L_56B5:
        DEC $4F33                       ; $56B5
        JMP swap_column1                      ; $56B8
L_56BB:
        LDA ($12),Y                     ; $56BB
        AND #$07                        ; $56BD
        BEQ L_5721                      ; $56BF
        LDA $4F34                       ; $56C1
        BEQ L_56C7                      ; $56C4
        RTS                             ; $56C6
L_56C7:
        INC $4F34                       ; $56C7
swap_column2:
        LDA $4F1E                       ; $56CA
        LDX $4F2B                       ; $56CD
        STX $4F1E                       ; $56D0
        STA $4F2B                       ; $56D3
        LDA $4F22                       ; $56D6
        LDX $4F2C                       ; $56D9
        STX $4F22                       ; $56DC
        STA $4F2C                       ; $56DF
        LDA $4F26                       ; $56E2
        LDX $4F2D                       ; $56E5
        STX $4F26                       ; $56E8
        STA $4F2D                       ; $56EB
        LDA $4F27                       ; $56EE
        LDX $4F2E                       ; $56F1
        STX $4F27                       ; $56F4
        STA $4F2E                       ; $56F7
        LDA $4F28                       ; $56FA
        LDX $4F2F                       ; $56FD
        STX $4F28                       ; $5700
        STA $4F2F                       ; $5703
        LDA $4F29                       ; $5706
        LDX $4F30                       ; $5709
        STX $4F29                       ; $570C
        STA $4F30                       ; $570F
        LDA $4F2A                       ; $5712
        LDX $4F31                       ; $5715
        STX $4F2A                       ; $5718
        STA $4F31                       ; $571B
        LDX $50                         ; $571E
        RTS                             ; $5720
L_5721:
        LDA $4F34                       ; $5721
        BNE L_5727                      ; $5724
        RTS                             ; $5726
L_5727:
        DEC $4F34                       ; $5727
        JMP swap_column2                      ; $572A

; $572D  35027 bytes of data between routines

; Startup entry for the player that lives under the KERNAL, at $E000.
reset_music:
        JMP L_E300                      ; $E000
; The other entry, used when the title option in $1385 is on.
start_tune:
        JMP L_E306                      ; $E003

; $E006  762 bytes of data between routines

L_E300:
        JMP L_E434                      ; $E300
L_E303:
        JMP L_E352                      ; $E303
L_E306:
        JMP L_E479                      ; $E306

; $E309  73 bytes of data between routines

L_E352:
        LDA #$00                        ; $E352
        STA $E30C                       ; $E354
        JSR L_E38C                      ; $E357
        LDA #$01                        ; $E35A
        STA $E30C                       ; $E35C
        RTS                             ; $E35F
L_E360:
        LDA $E309                       ; $E360
        CMP $E86B                       ; $E363
        BCC L_E36B                      ; $E366
        LDA $E86B                       ; $E368
L_E36B:
        ASL A                           ; $E36B
        ASL A                           ; $E36C
        ASL A                           ; $E36D
        TAY                             ; $E36E
        LDX #$FE                        ; $E36F
L_E371:
        INX                             ; $E371
        INX                             ; $E372
        LDA $E8C0,Y                     ; $E373
        STA $E3DF,X                     ; $E376
        INY                             ; $E379
        INX                             ; $E37A
        LDA $E8C0,Y                     ; $E37B
        STA $E3DF,X                     ; $E37E
        INY                             ; $E381
        INX                             ; $E382
        CPX #$0A                        ; $E383
        BCC L_E371                      ; $E385
        RTS                             ; $E387
        .byte $B9, $C0, $E8, $60

L_E38C:
        JSR L_E360                      ; $E38C
        JSR L_E3A8                      ; $E38F
        LDX #$00                        ; $E392
L_E394:
        STX $E12E                       ; $E394
        LDA #$FF                        ; $E397
        STA $E130                       ; $E399
        JSR L_E5BB                      ; $E39C
        LDX $E12E                       ; $E39F
        INX                             ; $E3A2
        CPX #$03                        ; $E3A3
        BNE L_E394                      ; $E3A5
        RTS                             ; $E3A7
L_E3A8:
        LDA #$00                        ; $E3A8
        STA $E30F                       ; $E3AA
        STA $E2E8                       ; $E3AD
        LDX #$2E                        ; $E3B0
L_E3B2:
        LDA $E2A7,X                     ; $E3B2
        STA $E0FF,X                     ; $E3B5
        DEX                             ; $E3B8
        BNE L_E3B2                      ; $E3B9
        LDA $E30D                       ; $E3BB
        STA $E12F                       ; $E3BE
        TXA                             ; $E3C1
        STA $E30B                       ; $E3C2
L_E3C5:
        STA $E30A                       ; $E3C5
        STA $E30E                       ; $E3C8
        STA $E30C                       ; $E3CB
        STA $E100                       ; $E3CE
        LDX #$17                        ; $E3D1
L_E3D3:
        STA $D400,X                     ; $E3D3
        DEX                             ; $E3D6
        BPL L_E3D3                      ; $E3D7
        RTS                             ; $E3D9
L_E3DA:
        LDA #$00                        ; $E3DA
        BEQ L_E3C5                      ; $E3DC
L_E3DE:
        LDA $E8F6                       ; $E3DE
        RTS                             ; $E3E1
L_E3E2:
        LDA $E98A                       ; $E3E2
        RTS                             ; $E3E5
L_E3E6:
        LDA $EA44                       ; $E3E6
        RTS                             ; $E3E9
L_E3EA:
        LDA $E12E                       ; $E3EA
        BEQ L_E408                      ; $E3ED
        LSR A                           ; $E3EF
        BCS L_E41E                      ; $E3F0
        JSR L_E3E6                      ; $E3F2
        PHP                             ; $E3F5
        INC $E3E7                       ; $E3F6
        BNE L_E3FE                      ; $E3F9
        INC $E3E8                       ; $E3FB
L_E3FE:
        PHA                             ; $E3FE
        JSR L_E3E6                      ; $E3FF
        STA $E130                       ; $E402
        PLA                             ; $E405
        PLP                             ; $E406
        RTS                             ; $E407
L_E408:
        JSR L_E3DE                      ; $E408
        PHP                             ; $E40B
        INC $E3DF                       ; $E40C
        BNE L_E414                      ; $E40F
        INC $E3E0                       ; $E411
L_E414:
        PHA                             ; $E414
        JSR L_E3DE                      ; $E415
        STA $E130                       ; $E418
        PLA                             ; $E41B
        PLP                             ; $E41C
        RTS                             ; $E41D
L_E41E:
        JSR L_E3E2                      ; $E41E
        PHP                             ; $E421
        INC $E3E3                       ; $E422
        BNE L_E42A                      ; $E425
        INC $E3E4                       ; $E427
L_E42A:
        PHA                             ; $E42A
        JSR L_E3E2                      ; $E42B
        STA $E130                       ; $E42E
        PLA                             ; $E431
        PLP                             ; $E432
        RTS                             ; $E433
L_E434:
        JSR L_E3DA                      ; $E434
        RTS                             ; $E437

; $E438  29 bytes of data between routines

L_E455:
        STX $E309                       ; $E455
        JMP L_E303                      ; $E458
        .byte $AE, $6C, $E8

L_E45E:
        LDA $E870,X                     ; $E45E
        BMI L_E47B                      ; $E461
        ASL A                           ; $E463
        BPL L_E470                      ; $E464
        ASL A                           ; $E466
        LSR A                           ; $E467
        LSR A                           ; $E468
        STA $E100                       ; $E469
        INX                             ; $E46C
        JMP L_E45E                      ; $E46D
L_E470:
        LSR A                           ; $E470
        INX                             ; $E471
        STX $E86C                       ; $E472
        TAX                             ; $E475
        JMP L_E455                      ; $E476
L_E479:
        LDY #$01                        ; $E479
L_E47B:
        LDX $E86D                       ; $E47B
        STX $E86C                       ; $E47E
        TYA                             ; $E481
        AND #$01                        ; $E482
        BNE L_E45E                      ; $E484
        JMP L_E3DA                      ; $E486

; $E489  306 bytes of data between routines

L_E5BB:
        LDA $E130                       ; $E5BB
        BMI L_E5C3                      ; $E5BE
        JMP L_E6B5                      ; $E5C0
L_E5C3:
        JSR L_E3EA                      ; $E5C3
        CMP #$FF                        ; $E5C6
        BNE L_E5D7                      ; $E5C8
        LDA #$01                        ; $E5CA
        STA $E30B                       ; $E5CC
        LDA #$FF                        ; $E5CF
        STA $E30E                       ; $E5D1
        JMP L_E6B5                      ; $E5D4
L_E5D7:
        TAY                             ; $E5D7
        LSR A                           ; $E5D8
        LSR A                           ; $E5D9
        LSR A                           ; $E5DA
        LSR A                           ; $E5DB
        AND #$07                        ; $E5DC
        TAX                             ; $E5DE
        LDA $E2D6,X                     ; $E5DF
        STA $E5E7                       ; $E5E2
        CLC                             ; $E5E5
        BCC L_E5E8                      ; $E5E6
L_E5E8:
        JMP L_E5FD                      ; $E5E8

; $E5EB  18 bytes of data between routines

L_E5FD:
        TYA                             ; $E5FD
        AND #$0F                        ; $E5FE
        LDX $E12E                       ; $E600
        LDY $E119,X                     ; $E603
        JSR L_E6B6                      ; $E606
        TXA                             ; $E609
        LDX $E12E                       ; $E60A
        STA $E10A,X                     ; $E60D
        TYA                             ; $E610
        STA $E10D,X                     ; $E611
        JMP L_E5BB                      ; $E614

; $E617  158 bytes of data between routines

L_E6B5:
        RTS                             ; $E6B5
L_E6B6:
        LDX $E2E7                       ; $E6B6
        BEQ L_E6BE                      ; $E6B9
        CLC                             ; $E6BB
        ADC #$10                        ; $E6BC
L_E6BE:
        TAX                             ; $E6BE
        DEY                             ; $E6BF
        BEQ L_E6E9                      ; $E6C0
        BMI L_E6CC                      ; $E6C2
        LDA $E6F9,X                     ; $E6C4
        TAX                             ; $E6C7
        LDY #$FF                        ; $E6C8
        DEX                             ; $E6CA
        RTS                             ; $E6CB
L_E6CC:
        LDA $E6F9,X                     ; $E6CC
        LSR A                           ; $E6CF
        LSR A                           ; $E6D0
        LSR A                           ; $E6D1
        BNE L_E6D6                      ; $E6D2
        LDA #$01                        ; $E6D4
L_E6D6:
        STA $E312                       ; $E6D6
        LDA $E6F9,X                     ; $E6D9
        SEC                             ; $E6DC
        SBC $E312                       ; $E6DD
        TAX                             ; $E6E0
        LDA #$00                        ; $E6E1
        SEC                             ; $E6E3
        SBC $E312                       ; $E6E4
        TAY                             ; $E6E7
        RTS                             ; $E6E8
L_E6E9:
        LDA $E6F9,X                     ; $E6E9
        LSR A                           ; $E6EC
        STA $E312                       ; $E6ED
        TAX                             ; $E6F0
        LDA #$00                        ; $E6F1
        SEC                             ; $E6F3
        SBC $E312                       ; $E6F4
        TAY                             ; $E6F7
        RTS                             ; $E6F8
