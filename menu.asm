	org $4000

    ; --- HEADER ORIGINALE COMPANION (31 byte) ---
    db 0x8b, 0xe6, 0x52, 0x1d, 0x40, 0x00, 0x00, 0x4e, 0x40, 0x26, 0x45, 0x00, 0x00, 0x1c, 0xe3, 0x00
    db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc9, 0x00, 0x00, 0xc9, 0x00, 0x00, 0x1f, 0x40

    ; --- PUNTO DI INGRESSO REALE A $401F ---
START:
    di                  
    ld sp, $e7ff        

    ; --- INIZIALIZZAZIONE HARDWARE Z80PIO (Porta Controllo $21) ---
    ld a, $4f           
    out ($21), a
    call VDP_WAIT

    ; --- INIZIALIZZAZIONE REGISTRI VDP ---
    ld a, $00 
    out ($81), a 
    ld a, $80 
    out ($81), a 

    ld a, $42 
    out ($81), a 
    ld a, $81 
    out ($81), a 

    ld a, $00 
    out ($81), a 
    ld a, $82 
    out ($81), a 

    ld a, $10 
    out ($81), a 
    ld a, $83 
    out ($81), a 

    ld a, $01 
    out ($81), a 
    ld a, $84 
    out ($81), a 

    ld a, $f4 
    out ($81), a 
    ld a, $87 
    out ($81), a 

    ; --- INIZIALIZZAZIONE COLOR TABLE (VRAM $0400) ---
    ; Tutto lo schermo Bianco (F) su Blu (4) per evitare glitch
    ld a, $00 
    out ($81), a 
    ld a, $44 
    out ($81), a 
    ld b, 32                 
FILL_COLOR:
    ld a, $f4 
    out ($80), a 
    call VDP_WAIT 
    djnz FILL_COLOR

    ; --- PASSO 1: PULIZIA SCHERMO INTEGRALE (NAME TABLE A $0000) ---
    ld a, $00 
    out ($81), a 
    call VDP_WAIT
    ld a, $40 
    out ($81), a 
    call VDP_WAIT
    ld bc, 768               
PULISCI_LOOP:
    ld a, 0 
    out ($80), a 
    call VDP_WAIT 
    dec bc 
    ld a, b 
    or c 
    jr nz, PULISCI_LOOP

    ; --- PASSO 2: CARICAMENTO DEI PATTERN DEL FONT ($0800) ---
    ld a, $00 
    out ($81), a 
    ld a, $48 
    out ($81), a 
    call VDP_WAIT
    ld hl, FONT_DATA
    ld bc, 38 * 8            
CARICA_FONT:
    ld a, (hl) 
    out ($80), a 
    call VDP_WAIT 
    inc hl 
    dec bc 
    ld a, b 
    or c 
    jr nz, CARICA_FONT
    ; --- PASSO 2B: STAMPA INTESTAZIONE PICOBBCBC (RIGA 0, COLONNA 0 -> VRAM $0000) ---
    ld a, $00 
    out ($81), a 
    call VDP_WAIT
    ld a, $40 
    out ($81), a 
    call VDP_WAIT
    ld hl, HEADER_STRING
    ld b, 32                 
STAMPA_HEADER_LOOP:
    ld a, (hl) 
    out ($80), a 
    call VDP_WAIT 
    inc hl 
    djnz STAMPA_HEADER_LOOP

    ; --- PASSO 3: STAMPA INIZIALE DEL MENU ---
    ld hl, MENU_STRINGS      
    ld de, $0044             
    ld a, 9
    ld ($e000), a            
STAMPA_MENU_LOOP:
    ld a, e 
    out ($81), a 
    call VDP_WAIT
    ld a, d 
    or $40 
    out ($81), a 
    call VDP_WAIT
    push hl 
    push de 
    ld b, 24                 
STAMPA_RIGA_FISSA:
    ld a, (hl) 
    out ($80), a 
    call VDP_WAIT 
    inc hl 
    djnz STAMPA_RIGA_FISSA
    pop de 
    pop hl 
    ld bc, 24 
    add hl, bc
    ex de, hl 
    ld bc, 32 
    add hl, bc 
    ex de, hl                
    ld a, ($e000) 
    dec a 
    ld ($e000), a 
    jr nz, STAMPA_MENU_LOOP  

    ; --- SETUP VARIABILI CURSORE IN RAM ---
    ld a, 0 
    ld ($e001), a 
    ld ($e002), a            

    ; =========================================================================
    ; MAIN LOOP DEFINITIVO (A=SU, Z=GIU, V=START)
    ; =========================================================================
GAME_LOOP:
    call DISEGNA_CURSORE     
    ld a, $ef 
    out ($20), a 
    call VDP_WAIT 
    in a, ($20) 
    ld c, a                  
    ld a, c 
    cp $00 
    jr z, FINE_INPUT 
    cp $ff 
    jr z, FINE_INPUT
    bit 0, c 
    jr z, VAI_SU
    bit 1, c 
    jr z, VAI_GIU       
    bit 2, c 
    jr z, PREMUTO_START
    xor a 
    ld ($e002), a 
    jr FINE_INPUT

VAI_GIU:
    ld a, ($e002) 
    and a 
    jr nz, FINE_INPUT        
    ld a, 1 
    ld ($e002), a 
    call CANCELLA_CURSORE    
    ld a, ($e001) 
    cp 8 
    jr z, RESET_SU 
    inc a 
    jr SALVA_POS
RESET_SU:
    xor a 
    jr SALVA_POS

VAI_SU:
    ld a, ($e002) 
    and a 
    jr nz, FINE_INPUT
    ld a, 1 
    ld ($e002), a 
    call CANCELLA_CURSORE
    ld a, ($e001) 
    and a 
    jr z, RESET_GIU 
    dec a 
    jr SALVA_POS
RESET_GIU:
    ld a, 8
SALVA_POS:
    ld ($e001), a            
FINE_INPUT:
    ld bc, 4000
DELAY_LOOP:
    dec bc 
    ld a, b 
    or c 
    jr nz, DELAY_LOOP
    jr GAME_LOOP             

    ; =========================================================================
    ; EVENTO PREMUTO START: BUS TRIGGER SULL'OFFSET $5000
    ; =========================================================================
PREMUTO_START:
    ld b, 4
LAMPEGGIO_LOOP:
    push bc 
    call CANCELLA_CURSORE 
    ld bc, 6000 
    call DELAY_ATTESA
    call DISEGNA_CURSORE 
    ld bc, 6000 
    call DELAY_ATTESA 
    pop bc 
    djnz LAMPEGGIO_LOOP
    ld a, ($e001) 
    ld l, a 
    ld h, 0                  
    ld de, $5000 
    add hl, de               
BUS_TRIGGER_LOOP:
    ld a, (hl) 
    nop 
    nop 
    jr BUS_TRIGGER_LOOP      

DELAY_ATTESA:
    dec bc 
    ld a, b 
    or c 
    jr nz, DELAY_ATTESA 
    ret

    ; =========================================================================
    ; SOTTOROUTINE GRAFICHE CURSORE DIRETTE SULL'INDICE 37
    ; =========================================================================
DISEGNA_CURSORE:
    call CALCOLA_POS_CURSORE 
    ld a, 37 
    out ($80), a 
    ret
CANCELLA_CURSORE:
    call CALCOLA_POS_CURSORE 
    ld a, 0 
    out ($80), a 
    ret
CALCOLA_POS_CURSORE:
    ld a, ($e001) 
    ld l, a 
    ld h, 0
    add hl, hl 
    add hl, hl 
    add hl, hl 
    add hl, hl 
    add hl, hl               
    ld de, $0042 
    add hl, de               
    ld a, l 
    out ($81), a 
    call VDP_WAIT
    ld a, h 
    or $40 
    out ($81), a 
    call VDP_WAIT 
    ret

VDP_WAIT:
    push af 
    pop af 
    ret                      
    ; =========================================================================
    ; DATA BASE: STRINGHE DEL MENU A LUNGHEZZA FISSA (24 BYTE CIASCUNA)
    ; =========================================================================
HEADER_STRING:
    ; P, I, C, O, B, B, C, P, C, [spazio] -> 10 caratteri
    db 26, 19, 13, 25, 12, 12, 13, 26, 13, 0
    ; B, Y, [spazio] -> 3 caratteri
    db 12, 35, 0
    ; A, ., O, T, T, A, V, I, A, N, I, [spazio] -> 12 caratteri
    db 11, 1, 25, 30, 30, 11, 32, 19, 11, 24, 19, 0
    ; C, [spazio], 2, 0, 2, 6, [spazio finale] -> 7 caratteri (Totale = 32)
    db 13, 0, 3, 25, 3, 7, 0

MENU_STRINGS:
    db 2, 1, 0, 11, 14, 32, 11, 24, 13, 15, 14, 0, 12, 19, 14, 14, 19, 24, 17, 0, 0, 0, 0, 0
    db 3, 1, 0, 11, 14, 32, 11, 24, 13, 15, 14, 0, 14, 15, 16, 15, 24, 13, 15, 0, 0, 0, 0, 0
    db 4, 1, 0, 12, 28, 19, 14, 17, 15, 0, 12, 31, 19, 22, 14, 15, 28, 0, 0, 0, 0, 0, 0, 0
    db 5, 1, 0, 13, 22, 31, 12, 0, 26, 22, 11, 35, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 6, 1, 0, 13, 22, 31, 12, 0, 26, 22, 11, 35, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 7, 1, 0, 13, 22, 31, 12, 0, 26, 22, 11, 35, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 8, 1, 0, 13, 25, 24, 32, 15, 24, 30, 19, 25, 24, 29, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0
    db 9, 1, 0, 14, 31, 26, 22, 19, 13, 11, 30, 15, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 10, 1, 0, 23, 11, 29, 30, 15, 28, 0, 26, 22, 11, 35, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0

    ; =========================================================================
    ; DATA BASE: PATTERN GRAFICI DEL FONT (MAPPATURA DIRETTA DA 0 A 37)
    ; =========================================================================
FONT_DATA:
    db $00,$00,$00,$00,$00,$00,$00,$00 ; Indice 0: Spazio vuoto
    db $00,$00,$00,$00,$00,$0c,$0c,$00 ; Indice 1: Il punto '.'
    db $08,$18,$08,$08,$08,$08,$1c,$00 ; '1' (Indice 2)
    db $3c,$42,$02,$3c,$40,$40,$7e,$00 ; '2' (Indice 3)
    db $3c,$02,$02,$3c,$02,$02,$3c,$00 ; '3' (Indice 4)
    db $08,$18,$28,$48,$7e,$08,$08,$00 ; '4' (Indice 5)
    db $7e,$40,$7c,$02,$02,$42,$3c,$00 ; '5' (Indice 6)
    db $3c,$40,$7c,$42,$42,$42,$3c,$00 ; '6' (Indice 7)
    db $7e,$02,$04,$08,$10,$10,$10,$00 ; '7' (Indice 8)
    db $3c,$42,$42,$3c,$42,$42,$3c,$00 ; '8' (Indice 9)
    db $3c,$42,$42,$3e,$02,$02,$3c,$00 ; '9' (Indice 10)
    db $18,$24,$42,$42,$7e,$42,$42,$00 ; 'A' (Indice 11)
    db $7c,$42,$42,$7c,$42,$42,$7c,$00 ; 'B' (Indice 12)
    db $3c,$42,$40,$40,$40,$42,$3c,$00 ; 'C' (Indice 13)
    db $78,$44,$42,$42,$42,$44,$78,$00 ; 'D' (Indice 14)
    db $7e,$40,$40,$7c,$40,$40,$7e,$00 ; 'E' (Indice 15)
    db $7e,$40,$40,$7c,$40,$40,$40,$00 ; 'F' (Indice 16)
    db $3c,$42,$40,$4e,$42,$42,$3c,$00 ; 'G' (Indice 17)
    db $42,$42,$42,$7e,$42,$42,$42,$00 ; 'H' (Indice 18)
    db $1c,$08,$08,$08,$08,$08,$1c,$00 ; 'I' (Indice 19)
    db $02,$02,$02,$02,$02,$22,$1c,$00 ; 'J' (Indice 20)
    db $42,$44,$48,$70,$48,$44,$42,$00 ; 'K' (Indice 21)
    db $40,$40,$40,$40,$40,$40,$7e,$00 ; 'L' (Indice 22)
    db $42,$66,$5a,$42,$42,$42,$42,$00 ; 'M' (Indice 23)
    db $42,$62,$52,$4a,$46,$42,$42,$00 ; 'N' (Indice 24)
    db $3c,$42,$42,$42,$42,$42,$3c,$00 ; 'O' (Indice 25)
    db $7c,$42,$42,$7c,$40,$40,$40,$00 ; 'P' (Indice 26)
    db $3c,$42,$42,$42,$4a,$44,$3a,$00 ; 'Q' (Indice 27)
    db $7c,$42,$42,$7c,$48,$44,$42,$00 ; 'R' (Indice 28)
    db $3c,$40,$3c,$02,$02,$42,$3c,$00 ; 'S' (Indice 29)
    db $3e,$08,$08,$08,$08,$08,$08,$00 ; 'T' (Indice 30)
    db $42,$42,$42,$42,$42,$42,$3c,$00 ; 'U' (Indice 31)
    db $42,$42,$42,$24,$24,$14,$08,$00 ; 'V' (Indice 32)
    db $42,$42,$42,$2a,$2a,$22,$22,$00 ; 'W' (Indice 33)
    db $42,$24,$14,$08,$14,$24,$42,$00 ; 'X' (Indice 34)
    db $42,$42,$24,$08,$08,$08,$08,$00 ; 'Y' (Indice 35)
    db $7e,$02,$04,$08,$10,$20,$7e,$00 ; 'Z' (Indice 36)
    db $40,$60,$70,$78,$70,$60,$40,$00 ; '>' (Indice 37: Freccia Cursore)

    ; --- FIX ANTICRASH BIOS DI LEADED SOLDER ---
    org $52e6
    db $00
