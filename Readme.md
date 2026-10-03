# armips keystone assembler
* fork from https://github.com/Kingcom/armips
* add backend https://github.com/keystone-engine/keystone/

# License
The part without `keystone` using MIT License. Copyright (c) 2009-2020 Kingcom, 2026 Nebulas.
`keystone` submodule using GNU GPLv2 License.

## Change

### 1. optional command line parameters
```
    -infolog <filename>
```
Writes messages from `.info` directives to the specified UTF-8 file. The file is created only after the assembly has been validated successfully.

#### `.info` Message
```
    .info    "Message"[,...]
```
Prints the message and sets warning/error flags. Useful with conditionals.

`.info` concatenates its comma-separated string, integer, or floating-point expressions and writes one line to the file specified by `-infolog`. It has no effect when `-infolog` is not specified. For example:

```
.info "address:", org(), ", Type 2."
```

### 2. ARMv7-A / Thumb-2 backend

When armips is built with `ARMIPS_ARMV7A=ON`, the `.armv7a` directive selects
the Keystone-based little-endian ARMv7-A backend. `.armv7a.big` selects its
big-endian variant. Use `.arm` and `.thumb` to switch between A32 and Thumb-2
instruction state. The existing `.gba`, `.nds`, `.3ds`, `.arm.little`, and
`.arm.big` modes continue to use the original armips encoder.

The initial backend supports global labels and forward references. It does not
support `@StaticLabel`, `@@LocalLabel`, `.pool`, or the `ldr ..., =value`
literal-pool pseudo instruction. Keystone is included as the `ext/keystone`
submodule and the backend is enabled automatically when that submodule is
initialized. An alternate Keystone source tree can be selected with
`ARMIPS_KEYSTONE_SOURCE_DIR`, or the backend can be disabled with
`ARMIPS_ARMV7A=OFF`. Keystone is distributed under GPLv2 with its FOSS License
Exception; review those terms when distributing a build that includes this
backend.

### 3. Write text with UTF-8 encoding

```
    .utf8  "String"[,...]
    .utf8n "String"[,...]
```

Inserts the given strings using UTF-8 encoding. Integer parameters are inserted as bytes. `.utf8` appends a null byte after all parameters, while `.utf8n` omits it.