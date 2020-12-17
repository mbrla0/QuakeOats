; boot/bios/1.s: Second bootloader stage for the QuakeOats project.
; 
; At this point, we have been booted up from the first sector of the target hard
; drive partition. Since QuakeOats boots off of an ext4 partition, we have 1024
; bytes of room for this stage, which ext4 considers to be padding space. We can
; assume that DL holds the code for the drive from which this was loaded.
;
; Since 1024 bytes is by no means much, we can't really boot our ELF kernel, as
; the code size requirements for first finding its file data, then querying for 
; a memory region that can house the full kernel, then parsing the ELF and 
; setting the code up *easily* goes past the room we have.
;
; So, what this stage does is look for the third stage bootloader, which may be
; effectively as big as we want, given it does not break over the low memory
; barrier. The third stage bootloader is located at the root directory, under
; the name specified in this file.
;
; NOTE!
; One of the best things about this stage is that it has no fucking clue where
; it even is in memory, so it has to manage being addressed in a completely 
; relative way. We have to pray that none of the code here crosses a segment
; boundary :^)

;;;;;;;;;;;;;;;;
;; MEMORY MAP ;;
;;;;;;;;;;;;;;;;
;
; This program makes use the two free ranges in low memory, the ones right
; below and right above the region at 07c0h:0000h in the following way:
;     [0050h:0000h, 07c0h:0000h] Stack region
;     [07e0h:0000h, 07f0h:0000h] Program data {
;         [07e0h:0000h, 07e0h:0010h] LBA packet for interrupt 13h
;         [07e0h:0010h, 07e0h:0012h] Offset of program code in the CS
;         [07e0h:0012h, 07e0h:0014h] Initial contents of the DX register
;     }
;     [07f0h:0000h, 8000h:0000h] Temporary general purpose buffer
;
[BITS 16]
[ORG  0h]
start:
	; Firstly, we need a stack, which we can thankfully find starting at
	; absolute address 0x500, with a length of 30463 bytes. Knowing the layout
	; of low memory has been the same since the IBM PC days, we can safely
	; assume it will be here, no matter in which cursed hardware people will
	; undoubtedly try running this in.
	mov cx, 7c0h
	mov ss, cx
	mov sp, 0

	; Now, we need to find out our absolute address. For that, we can do a
	; relative near jump with an offset of zero, which will just continue to the
	; next instruction, but with its address at the top of the stack. Which we 
	; can then use to figure out our absolute address in IP.
	call near a

a:	mov ax, word [ss:bp]
	sub ax, a

	mov ss, cx
	mov sp, 0

	mov cx, 07e0h
	mov es, cx
	mov [es:10h], word ax

	; Before we start loading things from disk, we need to make sure that the
	; BIOS running in this computer supports the logical block addressing 
	; extension to the 0x13 interrupt. This is a pretty safe requirement, unless
	; you plan to run this code in a 90's PC. In which case, you're more than
	; welcome to turn all of this into a CHS-addressed mess yourself.
	;
	; After this interrupt returns, the carry flag will be clear if the LBA
	; extensions are supported.
	push word dx
	push word ax

	mov ax, 4100h
	mov bx, 55AAh
	mov dl, 80h
	int 13h
	
	jnc near lba_ok

	pop  word bx
	push word bx

	mov ax, err_lba
	add ax, bx
	mov cx, err_nsf - err_lba

	jmp near die
lba_ok:
	; Now, the fun begins.
	; We have to load the superblock from the ext2/3/4 file system in this 
	; partition. The superblock begins at the 1024th byte of the drive, and
	; has a length of exactly 1024 bytes, so, we load that in at the start
	; of Work RAM, which is located at address 0x7f00.
	;
	; In order to do this, all of our transfers will begin by setting up the
	; disk address packet at 07e0h:0000h, then transfering to 07f0h:0000h.
	;
	; You can find more info on this structure and procedure at:
	;     https://wiki.osdev.org/ATA_in_x86_RealMode_(BIOS) 
	; 
	mov cx, 07e0h
	mov es, cx

	mov [es:00h], byte 10h ; Packet is 16 bytes long.
	mov [es:01h], byte 00h ; The value of this is reserved and equal to zero.
	mov [es:02h], word 02h ; We want two sectors from the disk.

	mov [es:04h], word  00h ; Write the results to offset 0.
	mov [es:06h], word 7f0h ; Write the results to segment 07f0h.

	mov [es:08h], dword 02h ; We want to pull in two sectors.
	mov [es:0Ch], dword 00h ; (No need for the upper 32 bits)

	mov ds, cx
	mov si, 0000h

	pop  word ax
	pop  word dx
	push word ax
	push word dx
	
	mov ax, 4200h
	mov dh, 00h

	int 13h

	; Make sure the superblock is in an okay state.
	;
	; You can find more info on the ext2 layouts at:
	;     https://wiki.osdev.org/Ext2
	;
	mov cx, 07f0h
	mov es, cx

	cmp word [es:3Ah], 01h
	je e2fs_ok

	cmp word [es:3Ch], 03h
	jne e2fs_ok

	pop  word bx
	pop  word bx
	push word bx

	mov ax, err_2fs
	add ax, bx
	mov cx, err_end - err_2fs

	jmp near die
e2fs_ok:
	mov ax, ; 
	
read_inode:
	; Call this in order to load the contents of an inode from disk.
	; Expects:
	; 
read_disk:
	; Call this in order to load the contents of the disk into a 
	; given address range in memory.
die:
	; Call this in case of an error. Displays the error string and halts.
	; Expects:
	;     AX -> The pointer to the error string in the code segment.
	;     CX -> The length of the string to be written.
	mov bp, ax
	mov bx, cs
	mov es, bx

	mov ah, 13h
	mov al, 1
	mov bx, 0
	mov dx, 0

	int 13h
die_end:
	cli
	jmp near die_end

strcmp:
stage2:  db "boot2.bin", 0
err_blk: db "Invalid ext2 superblock at disk offset 0x1024", 0
err_lba: db "BIOS does not support the required LBA extension for INT 13h", 0
err_nsf: db "Could not find the third stage bootloader at /boot2.bin", 0
err_2fs: db "The ext2 superblock has instructed us to stop", 0
err_end:

; Pad with zeros to 510 bytes and then add the magic MBR value.
times 510 - ($-$$) db 0
db 0x55, 0xaa

