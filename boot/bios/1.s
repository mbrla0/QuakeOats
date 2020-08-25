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
;
[BITS 16]
[ORG  0h]

start:
	; Firstly, we need a stack, which we can thankfully find starting at
	; absolute address 0x500, with a length of 30463 bytes. Knowing the layout
	; of low memory has been the same since the IBM PC days, we can safely
	; assume it will be here, no matter in which cursed hardware people will
	; undoubtedly try running this in.
	mov ss, 7c0h
	mov sp, 0

	; Now, we need to find out our absolute address. For that, we can do a
	; relative near jump with an offset of zero, which will just continue to the
	; next instruction, but with its address at the top of the stack. Which we 
	; can then use to figure out our absolute address in IP.
	call near 0

a:	mov ax, word [ss:sp]
	sub ax, a

	mov ss, 7c0h
	mov sp, 0

	; Before we start loading things from disk, we need to make sure that the
	; BIOS running in this computer supports the logical block addressing 
	; extension to the 0x13 interrupt. This is a pretty safe requirement, unless
	; you plan to run this code in a 90's PC. In which case, you're more than
	; welcome to turn all of this into a CHS-addressed mess yourself.
	;
	; After this interrupt returns, the carry flag will be clear if the LBA
	; extensions are supported.
	push ax
	push dx

	mov ax, 4100h
	mov bx, 55AAh
	mov dl, 80h
	int 13h
	
	jnc near lba_ok

	mov ax, err_lba
	jmp near die
lba_ok:
	; Now, the fun begins.
	; We have to load the superblock from the ext2/3/4 file system in this 
	; partition. The superblock begins at the 1024th byte of the drive, and
	; has a length of exactly 1024 bytes, so, we load that in at the start
	; of Work RAM, which is located at address 0x7e00.
	
die:

die_end:
	cli
	jmp near die_end

strcmp:
stage2: db "boot2.bin", 0

err_blk: db "Invalid ext2 superblock at disk offset 0x1024", 0
err_lba: db "BIOS does not support the required LBA extension for INT 13h", 0
err_nsf: db "Could not find the third stage bootloader at /boot2.bin", 0

; Pad with zeros to 510 bytes and then add the magic MBR value.
times 510 - ($-$$) db 0
db 0x55, 0xaa

