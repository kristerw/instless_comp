# instless_comp – instruction-less computation
###Assembler syntax
This is an implementation of a "one instruction set computer", so the assembler has only one instruction: `movdbz`.

The format of the assembler is
```
<label>: movdbz <dest_reg|discard> <src_reg|constant> <label zero> <label nonzero>
```

Comments can be added by `;` which discards the rest of the line.

The registers are of the form `r0`, `r1`, …. The destination register may be `discard`, which is a special built-in write-only register, and is used when only the side effect of comparing the source with 0 is needed. The source register may be a constant 0-1024 written as decimal or hexadecimal (note that the value is decremented before being written to the destination – that is why 1024 is allowed even though the max register value is 1023). 

The labels can be any name `[a-zA-Z][a-zA-Z0-9_]*` that is not `movdbz` or `discard`. There is a built-in label `exit` that switches back to the original X86 task instead of to a task implementing a `movdbz` instruction (i.e. it exits the program).

The program starts executing from the first instruction in the file.
