.armv7a
.create "output.bin",0

.thumb
MOV     R2, 10
mov r3, 0x10
mov r2, r3
mov r8, r2
mov r2, r8
mov.w r4, 10
movs r5, 10

.arm
mov r0, 10
mov r1, 0x10

.close
