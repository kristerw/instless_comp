# instless_comp
###Assembler syntax
This is an implementation of a one instruction set computer, but the assembler actually has two instructions: `movdbz` and `exit`. The `exit` instruction is actually a `movdbz` in disguise – the difference from the “real” `movdbz` is that `exit` switches back to the original X86 task instead of a task implementing a `movdbz` instruction.

The format of the assembler is
```
<label>: movdbz <dest_reg|discard> <src_reg|constant> <label zero> <label nonzero>
```
or
```
<label>: exit
```

Comments can be added by `;` which discards the rest of the line.

The registers are of the form `r0`, `r1`, …. The destination register may be `discard`, which is a special builtin write-only register, and is used when only the side effect of comparing the source with 0 is needed. The source register may be a constant 0-1024 (note that the value is decremented before being written to the destination – that is why 1024 is allowed even though the max register value is 1023). 

The labels can be any name `[a-zA-Z][a-zA-Z0-9_]*` that is not `movdbz`, `exit`, or `discard`.
