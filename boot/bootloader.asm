; Hadeed sector-0 boot sector. NASM syntax. BIOS loads it at 0000:7C00, DL=boot drive.
; It clears the text display and writes UTF-8 bytes for "حديد" as Latin-1 glyph codes only
; where a VGA font supplies those glyphs. Stock VGA ROM fonts do not contain Arabic glyphs.
; Therefore this is an exact byte write to B8000, not a claim of Arabic shaping/font support.

bits 16
org 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    cld

    mov ax, 0x0003                  ; 80x25 color text mode; initializes CRTC and VGA text memory
    int 0x10

    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ax, 0x0720                  ; space, light-grey-on-black
    mov cx, 80*25
    rep stosw

    ; VGA text memory is 16-bit cells: AL=character code, AH=attribute.
    ; Bytes are a temporary visible marker. Kernel M2 installs a supplied 8x16 Arabic font
    ; and writes shaped glyph codes. The marker is intentionally ASCII-safe: "HADEED".
    mov di, (12*80 + 36)*2
    mov si, banner
    mov cx, banner_end - banner
.put:
    lodsb
    stosb
    mov al, 0x0F
    stosb
    loop .put

.hang:
    hlt
    jmp .hang

banner: db 'HADEED'
banner_end:

times 510-($-$$) db 0
dw 0xAA55
