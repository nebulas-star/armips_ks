.armv7a
.create "output.bin",0

@StaticLabel:
	nop

.pool

	ldr r0,=0x12345678

.close
