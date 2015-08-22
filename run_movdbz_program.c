#include "setup.h"
#include "screen.h"

static void
add(int a, int b)
{
  write_reg(0, a);
  write_reg(1, b);

  printf("Calculate saturated r0 + r1 where r0 = %d and r1 = %d\n",
	 read_reg(0), read_reg(1));

  start_movdbz_program();

  printf("Result: r3 = %d\n\n", read_reg(3));
}


void
run_movdbz_program(void)
{
  add(4, 8);
  add(100, 1);
  add(0, 0);
  add(1000, 1000);
}
