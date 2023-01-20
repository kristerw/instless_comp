# Instruction-less computation
This is an implementation of the ideas in the paper “[The Page-Fault Weird Machine: Lessons in Instruction-less Computation](https://www.usenix.org/conference/woot13/workshop-program/presentation/bangert)” by J. Bangert et al., where it is shown that the IA-32 fault handling mechanism is Turing complete, so you can run programs without executing any CPU instructions!

I did this implementation to get an understanding of how it works in detail – the paper does not describe all the boring parts needed to get this working, and the [trapcc](https://github.com/jbangert/trapcc) implementation from the paper is IMHO hard to understand (it generates all the page tables etc. by ruby scrips, and it need to do global analysis with graph coloring etc. to generate working programs). My implementation tries to be easier to understand by choosing a less efficient instruction encoding that eliminates the need of global analysis, so each instruction can be generated individually.

I have made a python assembler that generates some code, but that is only syntactic sugar that is not strictly needed. For example, instructions 

```
L5:     movdbz r3, 1024, L7, L7
L7:     movdbz r2, r2, L8, exit
L8:     movdbz r3, r3, L7, L7
```

are generated as the C code

```
gen_movdbz(5, 3, 4, 6, 6);    // L5: movdbz r3, 1024, L7, L7
gen_movdbz(6, 2, 2, 7, -1);   // L7: movdbz r2, r2, L8, exit
gen_movdbz(7, 3, 3, 6, 6);    // L8: movdbz r3, r3, L7, L7
```

and it should be easy to follow how each instruction is generated as page tables etc. The construction is described in two blog posts ([overview](http://kristerw.blogspot.se/2015/08/instruction-less-computation.html) and [technical details](http://kristerw.blogspot.se/2015/08/instruction-less-computation-technical.html)).

### Building and running the software
Building the software is as easy as to run
```
make
```
which builds a kernel that you need to run on the bochs emulator (or real hardware). It may be possible to use other emulators, but the paper claims that none implements the correct fault behaviour (but that may have been fixed since the paper was written...)

I tested to build on a clean Ubuntu 14.04.3 LTS 64-bit desktop, and you only need to install `bochs-sdl` to be ready to go.

There is a bochs configuration file in the repository, so running bochs in the source code directory should use it autimatically.

### Changing the movdbz program
The example movdbz program in this repository can be found in [movdbz-addition.s](movdbz-addition.s), and it implements addition as described in the [blog post](http://kristerw.blogspot.se/2015/08/instruction-less-computation.html). You can change this to your own file by updating the `MOVDBZ_PROG` variable in the [Makefile](Makefile), and updating [run_movdbz_program.c](run_movdbz_program.c) to set up the input/output for your program.

### Assembler syntax
The constructed machine is a "one instruction set computer", so the assembler has only one instruction: `movdbz`.

The format of the assembler is
```
<label>: movdbz <dest_reg|discard> <src_reg|constant> <label zero> <label nonzero>
```

Comments can be added by `;` which discards the rest of the line.

The registers are of the form `r0`, `r1`, …. The destination register may be `discard`, which is a special built-in write-only register, and is used when only the side effect of comparing the source with 0 is needed. The source register may be a constant 0-1024 written as decimal or hexadecimal (note that the value is decremented before being written to the destination – that is why 1024 is allowed even though the max register value is 1023). 

The labels can be any name `[a-zA-Z][a-zA-Z0-9_]*` that is not `movdbz` or `discard`. There is a built-in label `exit` that switches back to the original X86 task instead of to a task implementing a `movdbz` instruction (i.e. it exits the program).

The program starts executing from the first instruction in the file.
