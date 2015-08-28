#include "setup.h"
#include "screen.h"

/* The address space is split into sections that are spread out so
 * that each is allocated using separate page table (i.e. all start
 * at a multiple of 0x00400000).  */
#define STACK_ADDRESS       0x00000000
#define INST_ADDRESS        0x00400000
#define X86_BASE_ADDRESS    0x00c00000
#define GDT_ADDRESS         0x01800000
#define X86_PD_ADDRESS      0x07c00000
#define PROG_BASE_ADDR      0x08000000

/* The IDT is a part of the instruction, and is mapped as the first page
 * in the instruction range. */
#define IDT_ADDRESS         (INST_ADDRESS)


/* Page number for the first page in the movdbz program, and translation
 * to virtual addresses for the program's pages.
 *
 * This address range use direct mapping, so the translation is trivial. */
#define PROG_BASE_PAGE ((PROG_BASE_ADDR) >> 12)
#define PROGPAGE2VIRT(x) ((unsigned int *)((PROG_BASE_ADDR) + ((x) << 12)))


/* The movdbz binary starts with 12 pages containing
 * - Stack PT and a page for the actual stack.
 * - The GTD PT and four pages for the GDT.
 * - An initial partial instruction whose PD is used when calling the
 *   first instruction in the movdbz program.
 * - A register (initialized to 1) used as input to all internal NOP
 *   instructions.
 * - A register used as output to all internal NOPs (and for 'discard'
 *   register in the assembler.
 *
 * This is followed by the registers, constants (which are read-only
 * registers initialized with the constant's value + 1), and the
 * instructions.  */
#define STACK_PAGE           0
#define STACK_PT_PAGE        1
#define GTD_PT_PAGE          2
#define GDT_PAGE0            3
#define GDT_PAGE1            4
#define GDT_PAGE2            5
#define GDT_PAGE3            6
#define INIT_0               7  // PD
#define INIT_1               8  // PT for INST_ADDRESS
#define INIT_2               9  // INST
#define REG_CONST_ONE_PAGE  10  // Must be (REG_R0_PAGE - 2).
#define REG_DISCARD_PAGE    11  // Must be (REG_R0_PAGE - 1).
#define REG_R0_PAGE         12

/* Each instruction is encoded in four pages.  */
#define PAGES_PER_INST       4
#define PD_OFF               0
#define INST_PT_OFF          1
#define INST_OFF             2
#define IDT_OFF              3

/* The register numbers for the two special registers (i.e. the two registers
 * before r0).  */
#define REG_DISCARD         -1
#define REG_CONSTANT_ONE    -2


static void gen_reg(int, unsigned int);
static void gen_movdbz(int, int, int, int, int);

#include "generated-prog.c"


static unsigned int x86_tss[26];
extern void set_gdtr(unsigned int base_addr, unsigned short table_limit);


#define PG_P  0x001 // Present (i.e. all other ignored if this is not set)
#define PG_W  0x002 // Write enable
#define PG_PS 0x080 // 4MB pages if CR4.PSE=1, ignored if CR4.PSE=0


static void
write_cr0(unsigned int data)
{
  __asm __volatile("movl %0,%%cr0" : : "r" (data) : "memory");
}


static unsigned int
read_cr0(void)
{
  unsigned int data;
  __asm __volatile("movl %%cr0,%0" : "=r" (data));
  return data;
}


static void
write_cr3(unsigned int data)
{
  __asm __volatile("movl %0,%%cr3" : : "r" (data) : "memory");
}


static unsigned int
read_eflags(void)
{
  unsigned int data;
  __asm __volatile("pushf\npop %0" : "=r" (data));
  return data;
}


static void
write_cr4(unsigned int data)
{
  __asm __volatile("movl %0,%%cr4" : : "r" (data) : "memory");
}


static unsigned int
read_cr4(void)
{
  unsigned int data;
  __asm __volatile("movl %%cr4,%0" : "=r" (data));
  return data;
}


static void
set_idtr(unsigned int base_addr, unsigned short table_limit)
{
  struct __attribute__((__packed__))
  {
    unsigned short table_limit;
    unsigned int base_addr;
  } idtr;
  idtr.table_limit = table_limit;
  idtr.base_addr = base_addr;
  __asm __volatile("lidtl %0" : : "m" (idtr) : "memory");
}


static void
init_paging(void)
{
  unsigned int *pde = (unsigned int *)X86_PD_ADDRESS;
  for (int i = 0; i < 512; i++)
    pde[i] = PG_P | PG_PS | PG_W | (i << 22);  // Direct mapping.
  write_cr3(X86_PD_ADDRESS);
  write_cr4(read_cr4() | 0x10);
  write_cr0(read_cr0() | (1 << 31));
}


static void
encode_gdte(
    unsigned int *p,
    unsigned int type,
    unsigned int base,
    unsigned int limit)
{
  limit = limit >> 12;
  p[0] = ((base & 0xffff) << 16) | (limit & 0xffff);
  p[1] = ((base & 0xff000000)
	  | 0x00c00000 |  (limit & 0x000f0000)
	  | (type << 8)
	  | ((base & 0x00ff0000) >> 16));
}


static void
init_gdt(unsigned int *gdt)
{
  memset(gdt, 0, 4096 * 4);
  encode_gdte(&gdt[2], 0x9A, 0, 0xfffffff);  /* code 0x08 */
  encode_gdte(&gdt[4], 0x92, 0, 0xfffffff);  /* data 0x10 */
  encode_gdte(&gdt[6], 0x89, (unsigned int)x86_tss, 0xfffff);  /* TSS 0x18 */
  encode_gdte(&gdt[0x7fe], 0x89, 0x40ffd0, 0xfffff);
  encode_gdte(&gdt[0xbfe], 0x89, 0x41ffd0, 0xfffff);
  encode_gdte(&gdt[0xffe], 0x89, 0x42ffd0, 0xfffff);
}


static void
init_tss(void)
{
  x86_tss[7] = X86_PD_ADDRESS;
}


unsigned int
read_reg(unsigned int reg_nr)
{
  unsigned int val = *(PROGPAGE2VIRT(reg_nr + REG_R0_PAGE) + 2);
  return val >> 2;
}


void
write_reg(unsigned int reg_nr, unsigned int val)
{
  *(PROGPAGE2VIRT(reg_nr + REG_R0_PAGE) + 2) = val << 2;
}


/* Populate the register page (i.e. the last part of the TSS).  */
static void
gen_reg(int reg_nr, unsigned int value)
{
  unsigned int *p = PROGPAGE2VIRT(REG_R0_PAGE + reg_nr);
  p[ 2] = value << 2; // ESP
  p[ 6] = 0x10;  // ES
  p[ 7] = 0x08;  // CS
  p[ 8] = 0x10;  // SS
  p[ 9] = 0x10;  // DS
  p[10] = 0x10;  // FS
  p[11] = 0x10;  // GS
  p[12] = 0;  // LDT Segment Selector
}


static void
generate_pagetable(
    unsigned int pd_page)
{
  unsigned int *pde_ptr = PROGPAGE2VIRT(pd_page);

  /* Map stack at 0x00000000. */
  unsigned int *p0 = PROGPAGE2VIRT(STACK_PT_PAGE);
  p0[0] = PG_P | PG_W | ((PROG_BASE_PAGE + STACK_PAGE) << 12);
  pde_ptr[0] = PG_P | PG_W | ((PROG_BASE_PAGE + STACK_PT_PAGE) << 12);

  /* Map the current instruction (including IDT) at 0x00400000. */
  unsigned int *p1 = PROGPAGE2VIRT(pd_page + INST_PT_OFF);
  p1[0] = PG_P | PG_W | ((PROG_BASE_PAGE + pd_page + IDT_OFF) << 12);
  pde_ptr[1] = PG_P | PG_W | ((PROG_BASE_PAGE + pd_page + INST_PT_OFF) << 12);

  /* Map the memory used for the x86 at 0x00c00000. */
  pde_ptr[3] = PG_P | PG_PS | PG_W | (3 << 22);

  /* Map the GDT on four consecutive pages at 0x01800000. */
  unsigned int *p6 = PROGPAGE2VIRT(GTD_PT_PAGE);
  for (int i = 0; i < 4; i++)
    p6[i] = PG_P | PG_W | ((PROG_BASE_PAGE + GDT_PAGE0 + i) << 12);
  pde_ptr[6] = PG_P | PG_W | ((PROG_BASE_PAGE + GTD_PT_PAGE) << 12);
}


static unsigned int
inst_to_bank(
    int inst_nr)
{
  return (inst_nr % 3) + 1;
}


static unsigned int
inst_to_gdte(
    int inst_nr)
{
  if (inst_nr < 0)
    return 0x18;
  else
    return inst_to_bank(inst_nr) << 12 | 0x0ff8;
}


/* Populate the IDT with the destination instructions. */
static void
generate_idt_page(
    unsigned int pd_page,
    int dest1_inst_nr,
    int dest2_inst_nr)
{
  unsigned int *p = PROGPAGE2VIRT(pd_page + IDT_OFF);
  unsigned int tss_seg_selector1 = inst_to_gdte(dest1_inst_nr);
  unsigned int tss_seg_selector2 = inst_to_gdte(dest2_inst_nr);

  /* #DF - Double Fault */
  p[16] = tss_seg_selector2 << 16;
  p[17] = 0xe500;  /* Task gate present,
		      DPL (Descriptor Privilege Level) = 3
		      (i.e. application privilege level) */

  /* #PF - Page Fault */
  p[28] = tss_seg_selector1 << 16;
  p[29] = 0xe500;
}


/* Populate the instruction page (i.e. the first part of the "source TSS"). */
static void
generate_inst_page(
    unsigned int pd_page,
    int inst_nr)
{
  unsigned int *p = PROGPAGE2VIRT(pd_page + INST_OFF);
  unsigned int inst_bank = inst_to_bank(inst_nr);
  unsigned int base = 0x400000 + 0x010000 * inst_bank - 0x30;
  p[1019] = (PROG_BASE_PAGE + pd_page) << 12;  /* CR3 */
  p[1020] = 0xfffefff;  /* EIP */
  p[1021] = read_eflags();  /* EFLAGS */
  encode_gdte(&p[1022], 137, base, 0x0fffffff);  /* EAX and EDX */
}


/* Map the "destination TSS" (GDT page and destination register) into the
 * address space. */
static void
map_dest_tss(
    unsigned int pd_page,
    int inst_nr,
    int reg_nr)
{
  unsigned int *p1 = PROGPAGE2VIRT(pd_page + INST_PT_OFF);
  unsigned int inst_bank = inst_to_bank(inst_nr);

  p1[(inst_bank << 4) - 1]
    = PG_P | PG_W | ((PROG_BASE_PAGE + GDT_PAGE0 + inst_bank) << 12);
  p1[(inst_bank << 4)]
    = PG_P | PG_W | ((PROG_BASE_PAGE + REG_R0_PAGE + reg_nr) << 12);
}


/* Map the "source TSS" (instruction page and source register) into the
 * address space. */
static void
map_src_tss(
    unsigned int pd_page,
    int inst_nr,
    int reg_nr)
{
  unsigned int *p1 = PROGPAGE2VIRT(pd_page + INST_PT_OFF);
  unsigned int inst_bank = inst_to_bank(inst_nr);
  
  p1[(inst_bank << 4) - 1]
    = PG_P | PG_W | ((PROG_BASE_PAGE + FIRST_INST_PAGE + inst_nr * PAGES_PER_INST + INST_OFF) << 12);
  p1[(inst_bank << 4)]
    = PG_P | PG_W | ((PROG_BASE_PAGE + REG_R0_PAGE + reg_nr) << 12);
}


/* Generate one instruction.  */
static void
gen_inst(
    int inst_nr,
    int dest1_inst_nr,
    int dest2_inst_nr,
    int dest_reg,
    int dest1_input_reg,
    int dest2_input_reg)
{
  unsigned int pd_page = FIRST_INST_PAGE + inst_nr * PAGES_PER_INST + PD_OFF;
  generate_pagetable(pd_page);
  generate_idt_page(pd_page, dest1_inst_nr, dest2_inst_nr);
  generate_inst_page(pd_page, inst_nr);
  map_dest_tss(pd_page, inst_nr, dest_reg);
  if (dest1_inst_nr >= 0)
    map_src_tss(pd_page, dest1_inst_nr, dest1_input_reg);
  if (dest2_inst_nr >= 0)
    map_src_tss(pd_page, dest2_inst_nr, dest2_input_reg);
}


/* Generate one movdbz assembler instruction.  */
static void
gen_movdbz(
    int asminst_nr,
    int dest_reg,
    int source_reg,
    int asmdest1_nr,
    int asmdest2_nr)
{
  /* The movdbz assembler instruction is implemented as three real
   * instructions; two NOP instructions followed by an instruction doing
   * the work.  This simplifies the implementation by automatically handling
   * the restrictions of the X86 hardware:
   * - A task cannot switch to itself
   * - There are limited number of GDTs (because of the workaround for the
   *   busy bit), and the GDTs for the instructions and target instructions
   *   must be distinct.
   * - The input register of the instruction need to be encoded in the
   *   predecessor.
   * Adding the NOPs means that we can generate one assembler instruction
   * at a time, without needing to fix up the code for these issues.  */
  gen_inst(asminst_nr * 3, asminst_nr * 3 + 2, asminst_nr * 3 + 2,
	   REG_DISCARD, source_reg, source_reg);
  gen_inst(asminst_nr * 3 + 1, asminst_nr * 3 + 2, asminst_nr * 3 + 2,
	   REG_DISCARD, source_reg, source_reg);
  gen_inst(asminst_nr * 3 + 2, asmdest1_nr * 3, asmdest2_nr * 3 + 1,
	   dest_reg, REG_CONSTANT_ONE, REG_CONSTANT_ONE);
}


static void
setup_x86(void)
{
  init_paging();   
  init_tss();
  init_gdt((void *)GDT_ADDRESS);
  set_gdtr(GDT_ADDRESS, 0xffff);   // Sets TSS to 0x18
  set_idtr(IDT_ADDRESS, 0x7ff);
}


static void
setup_movdbz_program(void)
{
  memset(PROGPAGE2VIRT(0), 0, (LAST_INST_PAGE + 1) * 4096);

  gen_reg(REG_CONSTANT_ONE,  1);
  gen_reg(REG_DISCARD,  0);

  gen_program();

  /* Set up an initial page table and inst needed to call the first
   * instruction in the movdbz program.  */
  generate_pagetable(INIT_0);
  map_src_tss(INIT_0, 0, REG_CONSTANT_ONE);

  init_gdt(PROGPAGE2VIRT(GDT_PAGE0));
}


void
start_movdbz_program(void)
{
  write_cr3((PROG_BASE_PAGE + INIT_0) << 12);
  __asm __volatile ("ljmp  $0x1ff8, $0x0;add $4,%%esp" : : : "memory");
}


void
kmain(void)
{
  clear_screen();
  setup_x86();
  setup_movdbz_program();

  run_movdbz_program();
}
