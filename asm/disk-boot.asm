; Bruce Lee.d64
;
; The disk has one program, "BRUCE LEE+2 /REM". It loads at $0801 like a
; BASIC file. The BASIC line is SYS 2059, which is $080B, the first
; instruction below. From there the loader copies a decruncher to $00F0
; and jumps into it. The decruncher is the Exomizer one: it reads the
; packed bytes downward from $A447 and writes the program downward from
; $CF78. When the packed stream says it is finished, the loader switches
; the ROMs back in and jumps to $2800.
;
; Addresses on the left are where the instruction runs, not where it sits
; in the disk file. The copy stub runs at $080B. The decruncher is stored
; at $A548 and runs at $00F0.

; ---- BASIC line, load address $0801 ----
;   next line $080B, line number 1994, SYS 2059, end of line.
;   2059 decimal is $080B.

        * = $080B

; Copy 256 bytes from $A548 to $00F0. $01 arrives as $37 from BASIC.
; INC $01 makes it $38, which is RAM at $A000, so the copy reads the
; packed file instead of the BASIC ROM.
copy_decruncher:
        LDX #$00
        SEI
        INC $01
copy_page:
        LDA $A548,X
        STA $00F0,X
        INX
        BNE copy_page
        JMP decrunch


        * = $00F0

; The byte at $00F0 is not executed. It is the bit bucket. Each ASL shifts
; one packed bit into the carry. When the bucket becomes 0, the next packed
; byte is loaded into it.
bit_bucket:
        .byte $CD

; Read the next packed byte. $FA and $FB are the address inside the LDA
; below, so decrementing them walks the source pointer backward. The first
; byte read is $A447. The 256 bytes at $A448 are the length tables, copied
; to $071C before the first read.
get_byte:
        LDA $FA
        BNE got_low
        DEC $FB
got_low:
        DEC $FA
        LDA $A448
        RTS

; Write A to the destination, then walk that pointer backward. $FE and $FF
; are the address inside the STA. The first byte lands at $CF78.
put_byte:
        STA $CF78
        LDA $FE
        BNE put_low
        DEC $FF
put_low:
        DEC $FE
        RTS

; Read X bits into $A3 (low) and $A4 (high). The entry points just below
; add 1, 2, or 3 to X first, which is how the caller asks for a few extra
; bits.
get_bits_plus3:
        INX
get_bits_plus2:
        INX
get_bits_plus1:
        INX
get_bits:
        LDA #$00
        STA $A3
        STA $A4
next_bit:
        ASL bit_bucket
        BNE have_bit
        JSR get_byte
        ROL A
        STA bit_bucket
have_bit:
        ROL $A3
        ROL $A4
        DEX
        BNE next_bit
        LDA $A3
        RTS

; A length. Y counts the leading 1-bits. Y reaching $30 means the packed
; stream is finished. Otherwise Y picks a bit count and a base from the
; tables at $071C and $072C, and the extra bits are added to that base.
; The border colour is flashed once per length so the unpack is visible.
read_length:
        LDY #$1F
length_bit:
        INY
        JSR get_bits_plus1
        BNE length_bit
        CPY #$30
        BEQ unpacked
        LDX $071C,Y
        BEQ length_base
        JSR get_bits
length_base:
        ADC $072C,Y
        DEC $01
        STY $D020
        INC $01
        TAY
        RTS

unpacked:
        LDA #$37
        STA $01
        CLI
        JMP $2800


        * = $014E

; Copy the 256-byte length tables from $A448 to $071C, then unpack.
decrunch:
        LDA $A448,X
        STA $071C,X
        INX
        BNE decrunch

; A run of literal bytes. read_length returns the count in Y.
literals:
        JSR read_length
copy_literal:
        JSR get_byte
        JSR put_byte
        DEY
        BNE copy_literal

; A run copied from earlier in the output. $AA is how many sequences.
; For each one, read_length (entered in the middle, so Y starts at 0)
; chooses the offset, and the bytes are read backward from that offset.
sequences:
        JSR read_length
        STA $AA
next_sequence:
        LDY #$00
        JSR length_bit + 1          ; $0129, Y is not reset to $1F
        PHA
        DEY
        BNE have_index
        JSR get_bits_plus3
        BCC store_offset
have_index:
        DEY
        BEQ short_index
        LDY #$04
        INX
short_index:
        JSR get_bits_plus2
        TYA
        ADC $A3
        TAY
pick_length:
        LDX $075C,Y
        JSR get_bits
        ADC $0769,Y
        TAX
        LDA $A4
        ADC $0776,Y
        CPY #$06
        BNE offset_ready
        LDY #$0C
        CPX #$3B
        BEQ pick_length
offset_ready:
        TAY
        TXA
store_offset:
        SEC
        ADC $FE
        STA $C3
        TYA
        ADC $FF
        STA $C4
        PLA
        TAX
        LDY #$00
        LDA ($C3),Y
        DEC $C4
copy_sequence:
        DEY
        JSR put_byte
        LDA ($C3),Y
        DEX
        BNE copy_sequence
        DEC $AA
        BNE next_sequence
        BEQ literals
