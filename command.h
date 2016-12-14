#ifndef COMMAND_H
#define COMMAND_H

void keybuffering (int flag);
void command_periodic (void);
void command_exit_irq_hook (unsigned long cycles);
void command_insn_hook (void);
void command_init (void);
void keybuffering_defaults (void);
void print_current_insn (void);
int command_loop (void);
void command_read_hook (absolute_address_t addr);
void command_write_hook (absolute_address_t addr, U8 val);
int kbhit(void);
int kbchar(void);

#endif
