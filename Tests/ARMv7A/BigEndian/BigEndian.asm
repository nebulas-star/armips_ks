.armv7a.big
.create "output.bin",0

.arm
	movw r0,#0x1234
	rbit r1,r0

.thumb
	movw r2,#0x5678
	nop

.close
