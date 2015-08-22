#ifndef SETUP_H
#define SETUP_H

void run_movdbz_program(void);

unsigned int read_reg(unsigned int reg_nr);
void write_reg(unsigned int reg_nr, unsigned int val);

void start_movdbz_program(void);

#endif
