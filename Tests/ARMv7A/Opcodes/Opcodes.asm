.armv7a
.create "output.bin",0x1000

.macro EmitItBody,source
	moveq r4,source
	addeq r4,r4,#1
.endmacro

.arm
ArmStart:
	movw r0,#0x1234
	movt r0,#0x5678
	rbit r1,r0
	ubfx r2,r1,#4,#8
	b ArmForward
	nop
ArmForward:

.thumb
ThumbStart:
	movw r3,#0xABCD
	tbb [r3,r2]
	itt eq
	EmitItBody r3
	b.w ThumbForward
	nop
ThumbForward:
	bx lr

.close
