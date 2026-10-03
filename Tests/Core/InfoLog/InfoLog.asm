.gba
.create "assembled.bin", 0x100

.info "address:", org(), ", Type ", 1 + 1, "."
.org 0x120
.info "address:", org(), ", Type 2."
.info "UTF-8: 中文"

.close
