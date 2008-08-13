/*
 * Copyright 2001 by Arto Salmi and Joze Fabcic
 * Copyright 2006-2008 by Brian Dominy <brian@oddchange.com>
 *
 * This file is part of GCC6809.
 *
 * GCC6809 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GCC6809 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GCC6809; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "6809.h"
#include "monitor.h"
#include <ctype.h>
#include <signal.h>


/* A container for all symbol table information */
struct symbol_table symtab;

/* The function call stack */
struct function_call fctab[MAX_FUNCTION_CALLS];

/* The top of the function call stack */
struct function_call *current_function_call;

/* Automatically break after executing this many instructions */
int auto_break_insn_count = 0;

int monitor_on = 0;



enum addr_mode
{
  _illegal, _implied, _imm_byte, _imm_word, _direct, _extended,
  _indexed, _rel_byte, _rel_word, _reg_post, _sys_post, _usr_post
};

enum opcode
{
  _undoc, _abx, _adca, _adcb, _adda, _addb, _addd, _anda, _andb,
  _andcc, _asla, _aslb, _asl, _asra, _asrb, _asr, _bcc, _lbcc,
  _bcs, _lbcs, _beq, _lbeq, _bge, _lbge, _bgt, _lbgt, _bhi,
  _lbhi, _bita, _bitb, _ble, _lble, _bls, _lbls, _blt, _lblt,
  _bmi, _lbmi, _bne, _lbne, _bpl, _lbpl, _bra, _lbra, _brn,
  _lbrn, _bsr, _lbsr, _bvc, _lbvc, _bvs, _lbvs, _clra, _clrb,
  _clr, _cmpa, _cmpb, _cmpd, _cmps, _cmpu, _cmpx, _cmpy, _coma,
  _comb, _com, _cwai, _daa, _deca, _decb, _dec, _eora, _eorb,
  _exg, _inca, _incb, _inc, _jmp, _jsr, _lda, _ldb, _ldd,
  _lds, _ldu, _ldx, _ldy, _leas, _leau, _leax, _leay, _lsra,
  _lsrb, _lsr, _mul, _nega, _negb, _neg, _nop, _ora, _orb,
  _orcc, _pshs, _pshu, _puls, _pulu, _rola, _rolb, _rol, _rora,
  _rorb, _ror, _rti, _rts, _sbca, _sbcb, _sex, _sta, _stb,
  _std, _sts, _stu, _stx, _sty, _suba, _subb, _subd, _swi,
  _swi2, _swi3, _sync, _tfr, _tsta, _tstb, _tst, _reset,
#ifdef H6309
  _negd, _comd, _lsrd, _rord, _asrd, _rold, _decd, _incd, _tstd,
  _clrd
#endif
};

char *mne[] = {
  "???", "ABX", "ADCA", "ADCB", "ADDA", "ADDB", "ADDD", "ANDA", "ANDB",
  "ANDCC", "ASLA", "ASLB", "ASL", "ASRA", "ASRB", "ASR", "BCC", "LBCC",
  "BCS", "LBCS", "BEQ", "LBEQ", "BGE", "LBGE", "BGT", "LBGT", "BHI",
  "LBHI", "BITA", "BITB", "BLE", "LBLE", "BLS", "LBLS", "BLT", "LBLT",
  "BMI", "LBMI", "BNE", "LBNE", "BPL", "LBPL", "BRA", "LBRA", "BRN",
  "LBRN", "BSR", "LBSR", "BVC", "LBVC", "BVS", "LBVS", "CLRA", "CLRB",
  "CLR", "CMPA", "CMPB", "CMPD", "CMPS", "CMPU", "CMPX", "CMPY", "COMA",
  "COMB", "COM", "CWAI", "DAA", "DECA", "DECB", "DEC", "EORA", "EORB",
  "EXG", "INCA", "INCB", "INC", "JMP", "JSR", "LDA", "LDB", "LDD",
  "LDS", "LDU", "LDX", "LDY", "LEAS", "LEAU", "LEAX", "LEAY", "LSRA",
  "LSRB", "LSR", "MUL", "NEGA", "NEGB", "NEG", "NOP", "ORA", "ORB",
  "ORCC", "PSHS", "PSHU", "PULS", "PULU", "ROLA", "ROLB", "ROL", "RORA",
  "RORB", "ROR", "RTI", "RTS", "SBCA", "SBCB", "SEX", "STA", "STB",
  "STD", "STS", "STU", "STX", "STY", "SUBA", "SUBB", "SUBD", "SWI",
  "SWI2", "SWI3", "SYNC", "TFR", "TSTA", "TSTB", "TST", "RESET",
#ifdef H6309
  "NEGD", "COMD", "LSRD", "RORD", "ASRD", "ROLD", "DECD",
  "INCD", "TSTD", "CLRD",
#endif
};

typedef struct
{
  UINT8 code;
  UINT8 mode;
} opcode_t;

opcode_t codes[256] = {
  {_neg, _direct},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_com, _direct},
  {_lsr, _direct},
  {_undoc, _illegal},
  {_ror, _direct},
  {_asr, _direct},
  {_asl, _direct},
  {_rol, _direct},
  {_dec, _direct},
  {_undoc, _illegal},
  {_inc, _direct},
  {_tst, _direct},
  {_jmp, _direct},
  {_clr, _direct},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_nop, _implied},
  {_sync, _implied},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_lbra, _rel_word},
  {_lbsr, _rel_word},
  {_undoc, _illegal},
  {_daa, _implied},
  {_orcc, _imm_byte},
  {_undoc, _illegal},
  {_andcc, _imm_byte},
  {_sex, _implied},
  {_exg, _reg_post},
  {_tfr, _reg_post},
  {_bra, _rel_byte},
  {_brn, _rel_byte},
  {_bhi, _rel_byte},
  {_bls, _rel_byte},
  {_bcc, _rel_byte},
  {_bcs, _rel_byte},
  {_bne, _rel_byte},
  {_beq, _rel_byte},
  {_bvc, _rel_byte},
  {_bvs, _rel_byte},
  {_bpl, _rel_byte},
  {_bmi, _rel_byte},
  {_bge, _rel_byte},
  {_blt, _rel_byte},
  {_bgt, _rel_byte},
  {_ble, _rel_byte},
  {_leax, _indexed},
  {_leay, _indexed},
  {_leas, _indexed},
  {_leau, _indexed},
  {_pshs, _sys_post},
  {_puls, _sys_post},
  {_pshu, _usr_post},
  {_pulu, _usr_post},
  {_undoc, _illegal},
  {_rts, _implied},
  {_abx, _implied},
  {_rti, _implied},
  {_cwai, _imm_byte},
  {_mul, _implied},
  {_reset, _implied},
  {_swi, _implied},
  {_nega, _implied},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_coma, _implied},
  {_lsra, _implied},
  {_undoc, _illegal},
  {_rora, _implied},
  {_asra, _implied},
  {_asla, _implied},
  {_rola, _implied},
  {_deca, _implied},
  {_undoc, _illegal},
  {_inca, _implied},
  {_tsta, _implied},
  {_undoc, _illegal},
  {_clra, _implied},
  {_negb, _implied},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_comb, _implied},
  {_lsrb, _implied},
  {_undoc, _illegal},
  {_rorb, _implied},
  {_asrb, _implied},
  {_aslb, _implied},
  {_rolb, _implied},
  {_decb, _implied},
  {_undoc, _illegal},
  {_incb, _implied},
  {_tstb, _implied},
  {_undoc, _illegal},
  {_clrb, _implied},
  {_neg, _indexed},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_com, _indexed},
  {_lsr, _indexed},
  {_undoc, _illegal},
  {_ror, _indexed},
  {_asr, _indexed},
  {_asl, _indexed},
  {_rol, _indexed},
  {_dec, _indexed},
  {_undoc, _illegal},
  {_inc, _indexed},
  {_tst, _indexed},
  {_jmp, _indexed},
  {_clr, _indexed},
  {_neg, _extended},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_com, _extended},
  {_lsr, _extended},
  {_undoc, _illegal},
  {_ror, _extended},
  {_asr, _extended},
  {_asl, _extended},
  {_rol, _extended},
  {_dec, _extended},
  {_undoc, _illegal},
  {_inc, _extended},
  {_tst, _extended},
  {_jmp, _extended},
  {_clr, _extended},
  {_suba, _imm_byte},
  {_cmpa, _imm_byte},
  {_sbca, _imm_byte},
  {_subd, _imm_word},
  {_anda, _imm_byte},
  {_bita, _imm_byte},
  {_lda, _imm_byte},
  {_undoc, _illegal},
  {_eora, _imm_byte},
  {_adca, _imm_byte},
  {_ora, _imm_byte},
  {_adda, _imm_byte},
  {_cmpx, _imm_word},
  {_bsr, _rel_byte},
  {_ldx, _imm_word},
  {_undoc, _illegal},
  {_suba, _direct},
  {_cmpa, _direct},
  {_sbca, _direct},
  {_subd, _direct},
  {_anda, _direct},
  {_bita, _direct},
  {_lda, _direct},
  {_sta, _direct},
  {_eora, _direct},
  {_adca, _direct},
  {_ora, _direct},
  {_adda, _direct},
  {_cmpx, _direct},
  {_jsr, _direct},
  {_ldx, _direct},
  {_stx, _direct},
  {_suba, _indexed},
  {_cmpa, _indexed},
  {_sbca, _indexed},
  {_subd, _indexed},
  {_anda, _indexed},
  {_bita, _indexed},
  {_lda, _indexed},
  {_sta, _indexed},
  {_eora, _indexed},
  {_adca, _indexed},
  {_ora, _indexed},
  {_adda, _indexed},
  {_cmpx, _indexed},
  {_jsr, _indexed},
  {_ldx, _indexed},
  {_stx, _indexed},
  {_suba, _extended},
  {_cmpa, _extended},
  {_sbca, _extended},
  {_subd, _extended},
  {_anda, _extended},
  {_bita, _extended},
  {_lda, _extended},
  {_sta, _extended},
  {_eora, _extended},
  {_adca, _extended},
  {_ora, _extended},
  {_adda, _extended},
  {_cmpx, _extended},
  {_jsr, _extended},
  {_ldx, _extended},
  {_stx, _extended},
  {_subb, _imm_byte},
  {_cmpb, _imm_byte},
  {_sbcb, _imm_byte},
  {_addd, _imm_word},
  {_andb, _imm_byte},
  {_bitb, _imm_byte},
  {_ldb, _imm_byte},
  {_undoc, _illegal},
  {_eorb, _imm_byte},
  {_adcb, _imm_byte},
  {_orb, _imm_byte},
  {_addb, _imm_byte},
  {_ldd, _imm_word},
  {_undoc, _illegal},
  {_ldu, _imm_word},
  {_undoc, _illegal},
  {_subb, _direct},
  {_cmpb, _direct},
  {_sbcb, _direct},
  {_addd, _direct},
  {_andb, _direct},
  {_bitb, _direct},
  {_ldb, _direct},
  {_stb, _direct},
  {_eorb, _direct},
  {_adcb, _direct},
  {_orb, _direct},
  {_addb, _direct},
  {_ldd, _direct},
  {_std, _direct},
  {_ldu, _direct},
  {_stu, _direct},
  {_subb, _indexed},
  {_cmpb, _indexed},
  {_sbcb, _indexed},
  {_addd, _indexed},
  {_andb, _indexed},
  {_bitb, _indexed},
  {_ldb, _indexed},
  {_stb, _indexed},
  {_eorb, _indexed},
  {_adcb, _indexed},
  {_orb, _indexed},
  {_addb, _indexed},
  {_ldd, _indexed},
  {_std, _indexed},
  {_ldu, _indexed},
  {_stu, _indexed},
  {_subb, _extended},
  {_cmpb, _extended},
  {_sbcb, _extended},
  {_addd, _extended},
  {_andb, _extended},
  {_bitb, _extended},
  {_ldb, _extended},
  {_stb, _extended},
  {_eorb, _extended},
  {_adcb, _extended},
  {_orb, _extended},
  {_addb, _extended},
  {_ldd, _extended},
  {_std, _extended},
  {_ldu, _extended},
  {_stu, _extended}
};

opcode_t codes10[256] = {
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_lbrn, _rel_word},
  {_lbhi, _rel_word},
  {_lbls, _rel_word},
  {_lbcc, _rel_word},
  {_lbcs, _rel_word},
  {_lbne, _rel_word},
  {_lbeq, _rel_word},
  {_lbvc, _rel_word},
  {_lbvs, _rel_word},
  {_lbpl, _rel_word},
  {_lbmi, _rel_word},
  {_lbge, _rel_word},
  {_lblt, _rel_word},
  {_lbgt, _rel_word},
  {_lble, _rel_word},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_swi2, _implied},
  {_undoc, _illegal}, /* 10 40 */
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpd, _imm_word},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpy, _imm_word},
  {_undoc, _illegal},
  {_ldy, _imm_word},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpd, _direct},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpy, _direct},
  {_undoc, _illegal},
  {_ldy, _direct},
  {_sty, _direct},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpd, _indexed},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpy, _indexed},
  {_undoc, _illegal},
  {_ldy, _indexed},
  {_sty, _indexed},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpd, _extended},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpy, _extended},
  {_undoc, _illegal},
  {_ldy, _extended},
  {_sty, _extended},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_lds, _imm_word},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_lds, _direct},
  {_sts, _direct},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_lds, _indexed},
  {_sts, _indexed},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_lds, _extended},
  {_sts, _extended}
};

opcode_t codes11[256] = {
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_swi3, _implied},
  {_undoc, _illegal}, /* 11 40 */
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpu, _imm_word},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmps, _imm_word},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpu, _direct},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmps, _direct},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpu, _indexed},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmps, _indexed},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmpu, _extended},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_cmps, _extended},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal},
  {_undoc, _illegal}
};

char *reg[] = {
  "D", "X", "Y", "U", "S", "PC", "??", "??",
  "A", "B", "CC", "DP", "??", "??", "??", "??"
};

char index_reg[] = { 'X', 'Y', 'U', 'S' };

char *off4[] = {
  "0", "1", "2", "3", "4", "5", "6", "7",
  "8", "9", "10", "11", "12", "13", "14", "15",
  "-16", "-15", "-14", "-13", "-12", "-11", "-10", "-9",
  "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1"
};


void
add_named_symbol (const char *id, target_addr_t value, const char *filename)
{
	struct x_symbol *sym;
	static char *last_filename = "";
	
	symtab.addr_to_symbol[value] = sym = malloc (sizeof (struct x_symbol));
	sym->flags = S_NAMED;
	sym->u.named.id = symtab.name_area_next;
	sym->u.named.addr = value;

	strcpy (symtab.name_area_next, id);
	symtab.name_area_next += strlen (symtab.name_area_next) + 1;

	if (!filename)
		filename = "";

	if (strcmp (filename, last_filename))
	{
		last_filename = symtab.name_area_next;
		strcpy (last_filename, filename);
		symtab.name_area_next += strlen (symtab.name_area_next) + 1;
	}
	sym->u.named.file = last_filename;
}


struct x_symbol *
find_symbol (target_addr_t value)
{
	struct x_symbol *sym = NULL;

#if 1
	return NULL;
#endif

	while (value > 0)
	{
		sym = symtab.addr_to_symbol[value];
		if (sym)
			break;
		else
			value--;
	}
	return sym;
}


struct x_symbol *
find_symbol_by_name (const char *id)
{
	unsigned int addr;
	struct x_symbol *sym;

	for (addr = 0; addr < 0x10000; addr++)
	{
		sym = symtab.addr_to_symbol[addr];
		if (sym && !strcmp (sym->u.named.id, id))
			return sym;
	}
	return NULL;
}


/* Disassemble the current instruction.  Returns the number of bytes that 
compose it. */
int
dasm (char *buf, int opc)
{				
  UINT8 op, am;
  char *op_str;
  int pc = opc & 0xffff;
  char R;
  int fetch1;			/* the first (MSB) fetched byte, used in macro RDWORD */

  op = fetch8 ();

  if (op == 0x10)
    {
      op = fetch8 ();
      am = codes10[op].mode;
      op = codes10[op].code;
    }
  else if (op == 0x11)
    {
      op = fetch8 ();
      am = codes11[op].mode;
      op = codes11[op].code;
    }
  else
    {
      am = codes[op].mode;
      op = codes[op].code;
    }

  op_str = (char *) mne[op];

  switch (am)
    {
    case _illegal:
      sprintf (buf, "???");
      break;
    case _implied:
      sprintf (buf, "%s ", op_str);
      break;
    case _imm_byte:
      sprintf (buf, "%s #$%02X", op_str, fetch8 ());
      break;
    case _imm_word:
      sprintf (buf, "%s #$%04X", op_str, fetch16 ());
      break;
    case _direct:
      sprintf (buf, "%s <%s", op_str, monitor_addr_name (fetch8 ()));
      break;
    case _extended:
      sprintf (buf, "%s %s", op_str, monitor_addr_name (fetch16 ()));
      break;

    case _indexed:
      op = fetch8 ();
      R = index_reg[(op >> 5) & 0x3];

      if ((op & 0x80) == 0)
	{
	  sprintf (buf, "%s %s,%c", op_str, off4[op & 0x1f], R);
	  break;
	}

      switch (op & 0x1f)
	{
	case 0x00:
	  sprintf (buf, "%s ,%c+", op_str, R);
	  break;
	case 0x01:
	  sprintf (buf, "%s ,%c++", op_str, R);
	  break;
	case 0x02:
	  sprintf (buf, "%s ,-%c", op_str, R);
	  break;
	case 0x03:
	  sprintf (buf, "%s ,--%c", op_str, R);
	  break;
	case 0x04:
	  sprintf (buf, "%s ,%c", op_str, R);
	  break;
	case 0x05:
	  sprintf (buf, "%s B,%c", op_str, R);
	  break;
	case 0x06:
	  sprintf (buf, "%s A,%c", op_str, R);
	  break;
	case 0x08:
	  sprintf (buf, "%s $%02X,%c", op_str, fetch8 (), R);
	  break;
	case 0x09:
	  sprintf (buf, "%s $%04X,%c", op_str, fetch16 (), R);
	  break;
	case 0x0B:
	  sprintf (buf, "%s D,%c", op_str, R);
	  break;
	case 0x0C:
	  sprintf (buf, "%s $%02X,PC", op_str, fetch8 ());
	  break;
	case 0x0D:
	  sprintf (buf, "%s $%04X,PC", op_str, fetch16 ());
	  break;
	case 0x11:
	  sprintf (buf, "%s [,%c++]", op_str, R);
	  break;
	case 0x13:
	  sprintf (buf, "%s [,--%c]", op_str, R);
	  break;
	case 0x14:
	  sprintf (buf, "%s [,%c]", op_str, R);
	  break;
	case 0x15:
	  sprintf (buf, "%s [B,%c]", op_str, R);
	  break;
	case 0x16:
	  sprintf (buf, "%s [A,%c]", op_str, R);
	  break;
	case 0x18:
	  sprintf (buf, "%s [$%02X,%c]", op_str, fetch8 (), R);
	  break;
	case 0x19:
	  sprintf (buf, "%s [$%04X,%c]", op_str, fetch16 (), R);
	  break;
	case 0x1B:
	  sprintf (buf, "%s [D,%c]", op_str, R);
	  break;
	case 0x1C:
	  sprintf (buf, "%s [$%02X,PC]", op_str, fetch8 ());
	  break;
	case 0x1D:
	  sprintf (buf, "%s [$%04X,PC]", op_str, fetch16 ());
	  break;
	case 0x1F:
	  sprintf (buf, "%s [%s]", op_str, monitor_addr_name (fetch16 ()));
	  break;
	default:
	  sprintf (buf, "%s ??", op_str);
	  break;
	}
      break;

    case _rel_byte:
      fetch1 = ((INT8) fetch8 ());
      sprintf (buf, "%s $%04X", op_str, (fetch1 + pc) & 0xffff);
      break;

    case _rel_word:
      sprintf (buf, "%s $%04X", op_str, (fetch16 () + pc) & 0xffff);
      break;

    case _reg_post:
      op = fetch8 ();
      sprintf (buf, "%s %s,%s", op_str, reg[op >> 4], reg[op & 15]);
      break;

    case _usr_post:
    case _sys_post:
      op = fetch8 ();
      sprintf (buf, "%s ", op_str);

      if (op & 0x80)
	strcat (buf, "PC,");
      if (op & 0x40)
	strcat (buf, am == _usr_post ? "S," : "U,");
      if (op & 0x20)
	strcat (buf, "Y,");
      if (op & 0x10)
	strcat (buf, "X,");
      if (op & 0x08)
	strcat (buf, "DP,");
      if ((op & 0x06) == 0x06)
	strcat (buf, "D,");
      else
	{
	  if (op & 0x04)
	    strcat (buf, "B,");
	  if (op & 0x02)
	    strcat (buf, "A,");
	}
      if (op & 0x01)
	strcat (buf, "CC,");
      buf[strlen (buf) - 1] = '\0';
      break;

    }
  return pc - opc;
}

int
sizeof_file (FILE * file)
{
  int size;

  fseek (file, 0, SEEK_END);
  size = ftell (file);
  rewind (file);

  return size;
}


int
load_map_file (const char *name)
{
	FILE *fp;
	char map_filename[256];
	char buf[256];
	char *value_ptr, *id_ptr;
	target_addr_t value;
	char *file_ptr;

	sprintf (map_filename, "%s.map", name);

	fp = fopen (map_filename, "r");
	if (!fp)
		return -1;

	for (;;)
	{
		fgets (buf, sizeof(buf)-1, fp);
		if (feof (fp))
			break;

		value_ptr = buf;
		if (strncmp (value_ptr, "      ", 6))
			continue;

		while (*value_ptr == ' ')
			value_ptr++;

		value = strtoul (value_ptr, &id_ptr, 16);
		if (id_ptr == value_ptr)
			continue;

		while (*id_ptr == ' ')
			id_ptr++;

		id_ptr = strtok (id_ptr, " \t\n");
		if (((*id_ptr == 'l') || (*id_ptr == 's')) && (id_ptr[1] == '_'))
			continue;
		++id_ptr;

		file_ptr = strtok (NULL, " \t\n");

		add_named_symbol (id_ptr, value, file_ptr);
	}

	fclose (fp);
	return 0;
}


int
load_hex (char *name)
{
  FILE *fp;
  int count, addr, type, data, checksum;
  int done = 1;
  int line = 0;

  fp = fopen (name, "r");

  if (fp == NULL)
    {
      printf ("failed to open hex record file %s.\n", name);
      return 1;
    }

  while (done != 0)
    {
      line++;

      if (fscanf (fp, ":%2x%4x%2x", &count, &addr, &type) != 3)
	{
	  printf ("line %d: invalid hex record information.\n", line);
	  break;
	}

      checksum = count + (addr >> 8) + (addr & 0xff) + type;

      switch (type)
	{
	case 0:
	  for (; count != 0; count--, addr++, checksum += data)
	    {
	      fscanf (fp, "%2x", &data);
	      write8 (addr, (UINT8) data);
	    }

	  checksum = (-checksum) & 0xff;
	  fscanf (fp, "%2x", &data);
	  if (data != checksum)
	    {
	      printf ("line %d: invalid hex record checksum.\n", line);
	      done = 0;
	      break;
	    }
	  (void) fgetc (fp);	/* skip CR/LF/NULL */
	  break;

	case 1:
	  checksum = (-checksum) & 0xff;
	  fscanf (fp, "%2x", &data);
	  if (data != checksum)
	    printf ("line %d: invalid hex record checksum \n", line);
	  done = 0;
	  break;

	case 2:
	default:
	  printf ("line %d: not supported hex type %d.\n", line, type);
	  done = 0;
	  break;
	}
    }

  fclose (fp);
  return 0;
}


int
load_s19 (char *name)
{
  FILE *fp;
  int count, addr, type, data, checksum;
  int done = 1;
  int line = 0;

  fp = fopen (name, "r");

  if (fp == NULL)
    {
      printf ("failed to open S-record file %s.\n", name);
      return 1;
    }

  while (done != 0)
    {
      line++;

      if (fscanf (fp, "S%1x%2x%4x", &type, &count, &addr) != 3)
	{
	  printf ("line %d: invalid S record information.\n", line);
	  break;
	}

      checksum = count + (addr >> 8) + (addr & 0xff);

      switch (type)
	{
	case 1:
	  for (count -= 3; count != 0; count--, addr++, checksum += data)
	    {
	      fscanf (fp, "%2x", &data);
	      write8 (addr, (UINT8) data);
	    }

	  checksum = (~checksum) & 0xff;
	  fscanf (fp, "%2x", &data);
	  if (data != checksum)
	    {
	      printf ("line %d: invalid S record checksum.\n", line);
	      done = 0;
	      break;
	    }
	  (void) fgetc (fp);	/* skip CR/LF/NULL */
	  break;

	case 9:
	  checksum = (~checksum) & 0xff;
	  fscanf (fp, "%2x", &data);
	  if (data != checksum)
	    printf ("line %d: invalid S record checksum.\n", line);
	  done = 0;
	  break;

	default:
	  printf ("line %d: S%d not supported.\n", line, type);
	  done = 0;
	  break;
	}
    }

  fclose (fp);
  return 0;
}



void
monitor_call (unsigned int flags)
{
#ifdef CALL_STACK
	if (current_function_call <= &fctab[MAX_FUNCTION_CALLS-1])
	{
		current_function_call++;
		current_function_call->entry_point = get_pc ();
		current_function_call->flags = flags;
	}
#endif
}


void
monitor_return (void)
{
#ifdef CALL_STACK
	if (current_function_call > &fctab[MAX_FUNCTION_CALLS-1])
	{
		current_function_call--;
		return;
	}

	while ((current_function_call->flags & FC_TAIL_CALL) && 
		(current_function_call > fctab))
	{
		current_function_call--;
	}

	if (current_function_call > fctab)
		current_function_call--;
#endif
}


const char *
monitor_addr_name (target_addr_t addr)
{
	static char addr_name[256];
	struct x_symbol *sym;

	sym = find_symbol (addr);
	if (sym)
	{
		if (sym->u.named.addr == addr)
			return sym->u.named.id;
		else
			sprintf (addr_name, "%s+0x%X", 
				sym->u.named.id, addr - sym->u.named.addr);
	}
	else
		sprintf (addr_name, "$%04X", addr);
	return addr_name;
}


void
str_toupper (char *str)
{
  if (*str == '\0' || str == NULL)
    return;
  do
    {
      *str = toupper (*str);
      str++;
    }
  while (*str != '\0');
}

int
str_getnumber (char *str)
{
	struct x_symbol *sym = find_symbol_by_name (str);
	if (sym)
		return sym->u.named.addr;

  return strtoul (str, NULL, 0);
}

int
str_scan (char *str, char *table[], int maxi)
{
  int i = 0;

  for (;;)
    {
      while (isgraph (*str) == 0 && *str != '\0')
	str++;
      if (*str == '\0')
	return i;
      table[i] = str;
      if (maxi-- == 0)
	return i;

      while (isgraph (*str) != 0)
	str++;
      if (*str == '\0')
	return i;
      *str++ = '\0';
      i++;
    }
}

typedef struct
{
  int cmd_nro;
  char *cmd_string;
} command_t;

enum cmd_numbers
{
  CMD_HELP,
  CMD_DUMP,
  CMD_DASM,
  CMD_RUN,
  CMD_STEP,
  CMD_CONTINUE,
  CMD_SET,
  CMD_CLR,
  CMD_SHOW,
  CMD_SETBRK,
  CMD_CLRBRK,
  CMD_BRKSHOW,
  CMD_JUMP,
  CMD_QUIT,
  CMD_BACKTRACE,
  CMD_NEXT,
  CMD_RESET,

  REG_PC,
  REG_X,
  REG_Y,
  REG_S,
  REG_U,
  REG_D,
  REG_CC,
  REG_DP,
  REG_A,
  REG_B,
  CCR_E,
  CCR_F,
  CCR_H,
  CCR_I,
  CCR_N,
  CCR_Z,
  CCR_V,
  CCR_C
};

command_t cmd_table[] = {
  {CMD_HELP, "h"},
  {CMD_DUMP, "dump"},
  {CMD_DASM, "dasm"},
  {CMD_RUN, "r"},
  {CMD_JUMP, "jump"},
  {CMD_SET, "set"},
  {CMD_CLR, "clear"},
  {CMD_SHOW, "show"},
  {CMD_SETBRK, "b"},
  {CMD_CLRBRK, "bc"},
  {CMD_BRKSHOW, "bl"},
  {CMD_QUIT, "q"},
  {CMD_CONTINUE, "c"},
  {CMD_STEP, "s"},
  {CMD_NEXT, "n"},
  {CMD_BACKTRACE, "bt" },
  {CMD_RESET, "reset" },
  {-1, NULL}
};

command_t arg_table[] = {
  {REG_PC, "PC"},
  {REG_X, "X"},
  {REG_Y, "Y"},
  {REG_S, "S"},
  {REG_U, "U"},
  {REG_D, "D"},
  {REG_CC, "CC"},
  {REG_DP, "DP"},
  {REG_A, "A"},
  {REG_B, "B"},
  {CCR_E, "E"},
  {CCR_F, "F"},
  {CCR_H, "H"},
  {CCR_I, "I"},
  {CCR_N, "N"},
  {CCR_Z, "Z"},
  {CCR_V, "V"},
  {CCR_C, "C"},

  {-1, NULL}
};



static void
monitor_signal (int sigtype)
{
  (void) sigtype;
  putchar ('\n');
  monitor_on = 1;
}


void
monitor_init (void)
{
	int tmp;
	extern int debug_enabled;
	target_addr_t a;

	symtab.name_area = symtab.name_area_next = 
		malloc (symtab.name_area_free = 0x100000);
	a = 0; 
	do {
		symtab.addr_to_symbol[a] = NULL;
	} while (++a != 0);

	fctab[0].entry_point = read16 (0xfffe);
	memset (&fctab[0].entry_regs, 0, sizeof (struct cpu_regs));
	current_function_call = &fctab[0];

  auto_break_insn_count = 0;
  monitor_on = debug_enabled;
  signal (SIGINT, monitor_signal);
}


int
check_break (unsigned break_pc)
{
	if (auto_break_insn_count > 0)
		if (--auto_break_insn_count == 0)
			return 1;
	return 0;
}



void
cmd_show (void)
{
  int cc = get_cc ();
  int pc = get_pc ();
  char inst[50];
  int offset, moffset;

  moffset = dasm (inst, pc);

  printf ("S  $%04X U  $%04X X  $%04X Y  $%04X   EFHINZVC\n", get_s (),
	  get_u (), get_x (), get_y ());
  printf ("A  $%02X   B  $%02X   DP $%02X   CC $%02X     ", get_a (),
	  get_b (), get_dp (), cc);
  printf ("%c%c%c%c", (cc & E_FLAG ? '1' : '.'), (cc & F_FLAG ? '1' : '.'),
	  (cc & H_FLAG ? '1' : '.'), (cc & I_FLAG ? '1' : '.'));
  printf ("%c%c%c%c\n", (cc & N_FLAG ? '1' : '.'), (cc & Z_FLAG ? '1' : '.'),
	  (cc & V_FLAG ? '1' : '.'), (cc & C_FLAG ? '1' : '.'));
  printf ("PC: %s  ", monitor_addr_name (pc));
  printf ("Cycle  %lX   ", total);
  for (offset = 0; offset < moffset; offset++)
    printf ("%02X", read8 (offset+pc));
  printf ("  NextInst: %s\n", inst);
}


void
monitor_prompt (void)
{
	char inst[50];
	target_addr_t pc = get_pc ();
	dasm (inst, pc);

	printf ("S:%04X U:%04X X:%04X Y:%04X D:%04X\n", 
		get_s (), get_u (), get_x (), get_y (), get_d ());

	printf ("%30.30s   %s\n", monitor_addr_name (pc), inst);
}


void
monitor_backtrace (void)
{
	struct function_call *fc = current_function_call;
	while (fc >= &fctab[0]) {
		printf ("%s\n", monitor_addr_name (fc->entry_point));
		fc--;
	}
}

int
monitor6809 (void)
{
	int rc;

	signal (SIGINT, monitor_signal);
	rc = command_loop ();
	monitor_on = 0;
	return rc;

#if 0
	case CMD_BACKTRACE:
		monitor_backtrace ();
		continue;

	case CMD_JUMP:
	  if (arg_count != 2)
	    break;
	  set_pc (str_getnumber (arg[1]) & 0xffff);
	  return 0;

	case CMD_SET:
	  if (arg_count == 3)
	    {
	      int temp = str_getnumber (arg[2]);

	      switch (get_command (arg[1], arg_table))
		{
		case REG_PC:
		  set_pc (temp);
		  continue;
		case REG_X:
		  set_x (temp);
		  continue;
		case REG_Y:
		  set_y (temp);
		  continue;
		case REG_S:
		  set_s (temp);
		  continue;
		case REG_U:
		  set_u (temp);
		  continue;
		case REG_D:
		  set_d (temp);
		  continue;
		case REG_CC:
		  set_cc (temp);
		  continue;
		case REG_DP:
		  set_dp (temp);
		  continue;
		case REG_A:
		  set_a (temp);
		  continue;
		case REG_B:
		  set_b (temp);
		  continue;
		}
	      break;		/* invalid argument */
	    }
	  else if (arg_count == 2)
	    {
	      switch (get_command (arg[1], arg_table))
		{
		case CCR_E:
		  set_cc (get_cc () | E_FLAG);
		  continue;
		case CCR_F:
		  set_cc (get_cc () | F_FLAG);
		  continue;
		case CCR_H:
		  set_cc (get_cc () | H_FLAG);
		  continue;
		case CCR_I:
		  set_cc (get_cc () | I_FLAG);
		  continue;
		case CCR_N:
		  set_cc (get_cc () | N_FLAG);
		  continue;
		case CCR_Z:
		  set_cc (get_cc () | Z_FLAG);
		  continue;
		case CCR_V:
		  set_cc (get_cc () | V_FLAG);
		  continue;
		case CCR_C:
		  set_cc (get_cc () | C_FLAG);
		  continue;
		}
	      break;		/* invalid argument */
	    }
	  break;		/* invalid number of arguments */

	case CMD_CLR:
	  if (arg_count != 2)
	    break;

	  switch (get_command (arg[1], arg_table))
	    {
	    case REG_PC:
	      set_pc (0);
	      continue;
	    case REG_X:
	      set_x (0);
	      continue;
	    case REG_Y:
	      set_y (0);
	      continue;
	    case REG_S:
	      set_s (0);
	      continue;
	    case REG_U:
	      set_u (0);
	      continue;
	    case REG_D:
	      set_d (0);
	      continue;
	    case REG_DP:
	      set_dp (0);
	      continue;
	    case REG_CC:
	      set_cc (0);
	      continue;
	    case REG_A:
	      set_a (0);
	      continue;
	    case REG_B:
	      set_b (0);
	      continue;

	    case CCR_E:
	      set_cc (get_cc () & ~E_FLAG);
	      continue;
	    case CCR_F:
	      set_cc (get_cc () & ~F_FLAG);
	      continue;
	    case CCR_H:
	      set_cc (get_cc () & ~H_FLAG);
	      continue;
	    case CCR_I:
	      set_cc (get_cc () & ~I_FLAG);
	      continue;
	    case CCR_N:
	      set_cc (get_cc () & ~N_FLAG);
	      continue;
	    case CCR_Z:
	      set_cc (get_cc () & ~Z_FLAG);
	      continue;
	    case CCR_V:
	      set_cc (get_cc () & ~V_FLAG);
	      continue;
	    case CCR_C:
	      set_cc (get_cc () & ~C_FLAG);
	      continue;
	    }
	  break;		/* invalid argument */

	case CMD_SHOW:
	  if (arg_count != 2)
	    {
	      cmd_show ();
	      continue;
	    }

	  switch (get_command (arg[1], arg_table))
	    {
	    case REG_PC:
	      printf ("PC: %s\n", monitor_addr_name (get_pc ()));
	      continue;
	    case REG_X:
	      printf ("X:  $%04X\n", get_x ());
	      continue;
	    case REG_Y:
	      printf ("Y:  $%04X\n", get_y ());
	      continue;
	    case REG_S:
	      printf ("S:  $%04X\n", get_s ());
	      continue;
	    case REG_U:
	      printf ("U:  $%04X\n", get_u ());
	      continue;
	    case REG_D:
	      printf ("D:  $%04X\n", get_d ());
	      continue;
	    case REG_DP:
	      printf ("DP: $%02X\n", get_dp ());
	      continue;
	    case REG_CC:
	      printf ("CC: $%02X\n", get_cc ());
	      continue;
	    case REG_A:
	      printf ("A:  $%02X\n", get_a ());
	      continue;
	    case REG_B:
	      printf ("B:  $%02X\n", get_b ());
	      continue;

	    case CCR_E:
	      printf ("CC:E %c\n", get_cc () & E_FLAG ? '1' : '0');
	      continue;
	    case CCR_F:
	      printf ("CC:F %c\n", get_cc () & F_FLAG ? '1' : '0');
	      continue;
	    case CCR_H:
	      printf ("CC:H %c\n", get_cc () & H_FLAG ? '1' : '0');
	      continue;
	    case CCR_I:
	      printf ("CC:I %c\n", get_cc () & I_FLAG ? '1' : '0');
	      continue;
	    case CCR_N:
	      printf ("CC:N %c\n", get_cc () & N_FLAG ? '1' : '0');
	      continue;

	    case CCR_Z:
	      printf ("CC:Z %c\n", get_cc () & Z_FLAG ? '1' : '0');
	      continue;
	    case CCR_V:
	      printf ("CC:V %c\n", get_cc () & V_FLAG ? '1' : '0');
	      continue;
	    case CCR_C:
	      printf ("CC:C %c\n", get_cc () & C_FLAG ? '1' : '0');
	      continue;
	    }
	  break;		/* invalid argument */

	case CMD_RESET:
		cpu_reset ();
		continue;
#endif
}
