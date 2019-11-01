#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>

typedef enum {false, true} bool;

#define ARR_SIZE(a) (sizeof (a) / sizeof *(a))

struct Str {
    const char *data;
    size_t len;
};
typedef struct Str Str;

static Str
str(const char *s)
{
    return (Str){s, strlen(s)};
}

static bool
str_eq(Str a, Str b)
{
    if (a.len != b.len) {
        return false;
    }
    return memcmp(a.data, b.data, a.len) == 0;
}

static int32_t
str_to_i32(Str s)
{
    int32_t base = 10;
    if (s.len >= 2 && s.data[0] == '0' && s.data[1] == 'x') {
        s.data += 2;
        s.len -= 2;
        base = 16;
    }
    int32_t n = 0;
    for (size_t i = 0; i < s.len; i++) {
        n *= base;
        char c = s.data[i];
        if (c >= '0' && c <= '9') {
            n += c - '0';
        } else if (c >= 'a' && c <= 'f') {
            n += c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            n += c - 'A' + 10;
        }
    }
    return n;
}

static void
print_error(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

struct UnknownValue {
    uint32_t *ptr;
    enum InstrType {
        INSTR_I, INSTR_J, INSTR_B,
    } type;
    Str label;
    uint64_t relative_to;
};
typedef struct UnknownValue UnknownValue;

struct LabelValue {
    Str label;
    uint64_t value;
};
typedef struct LabelValue LabelValue;

struct State {
    Str code;
    size_t i;

    uint64_t pc;

    LabelValue labels[20];
    size_t n_labels;

#define MAX_UNKNOWNS 20
    UnknownValue unknowns[20];
    size_t n_unknowns;
};
typedef struct State State;

static bool
is_letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool
is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool
is_labelchar(char c)
{
    return is_letter(c) || is_digit(c) || c == '_';
}

static bool
is_labelstart(char c)
{
    return is_letter(c) || c == '_';
}

static bool
is_whitespace(char c)
{
    return c == ' ' || c == '\t';
}

static Str
read_token(State *st)
{
    size_t i = st->i;
    while (i < st->code.len && is_whitespace(st->code.data[i])) {
        i++;
    }
    if(i < st->code.len && st->code.data[i] == ';') {
        while (i < st->code.len && st->code.data[i] != '\n') {
            i++;
        }
    }
    char first = st->code.data[i];
    size_t token_start = i;
    if (is_labelstart(first)) {
        while (i < st->code.len && is_labelchar(st->code.data[i])) {
            i++;
        }
    } else if (is_digit(first)) {
        while (i < st->code.len
                && (is_digit(st->code.data[i])
                    || is_letter(st->code.data[i])))
        {
            i++;
        }
    } else if (i < st->code.len) {
        i++;
    }
    st->i = i;
    return (Str){st->code.data + token_start, i - token_start};
}

enum Reg {
    REG_ZERO, REG_RA,   REG_SP,   REG_GP,
    REG_TP,   REG_T0,   REG_T1,   REG_T2,
    REG_S0,   REG_S1,   REG_A0,   REG_A1,
    REG_A2,   REG_A3,   REG_A4,   REG_A5,
    REG_A6,   REG_A7,   REG_S2,   REG_S3,
    REG_S4,   REG_S5,   REG_S6,   REG_S7,
    REG_S8,   REG_S9,   REG_S10,  REG_S11,
    REG_T3,   REG_T4,   REG_T5,   REG_T6,
};
typedef enum Reg Reg;

static Reg
read_reg(State *st)
{
    Str s = read_token(st);
    if      (str_eq(s, str("zero"))) { return REG_ZERO; }
    else if (str_eq(s, str("ra")))   { return REG_RA; }
    else if (str_eq(s, str("sp")))   { return REG_SP; }
    else if (str_eq(s, str("gp")))   { return REG_GP; }
    else if (str_eq(s, str("tp")))   { return REG_TP; }
    else if (str_eq(s, str("t0")))   { return REG_T0; }
    else if (str_eq(s, str("t1")))   { return REG_T1; }
    else if (str_eq(s, str("t2")))   { return REG_T2; }
    else if (str_eq(s, str("s0")))   { return REG_S0; }
    else if (str_eq(s, str("s1")))   { return REG_S1; }
    else if (str_eq(s, str("a0")))   { return REG_A0; }
    else if (str_eq(s, str("a1")))   { return REG_A1; }
    else if (str_eq(s, str("a2")))   { return REG_A2; }
    else if (str_eq(s, str("a3")))   { return REG_A3; }
    else if (str_eq(s, str("a4")))   { return REG_A4; }
    else if (str_eq(s, str("a5")))   { return REG_A5; }
    else if (str_eq(s, str("a6")))   { return REG_A6; }
    else if (str_eq(s, str("a7")))   { return REG_A7; }
    else if (str_eq(s, str("s2")))   { return REG_S2; }
    else if (str_eq(s, str("s3")))   { return REG_S3; }
    else if (str_eq(s, str("s4")))   { return REG_S4; }
    else if (str_eq(s, str("s5")))   { return REG_S5; }
    else if (str_eq(s, str("s6")))   { return REG_S6; }
    else if (str_eq(s, str("s7")))   { return REG_S7; }
    else if (str_eq(s, str("s8")))   { return REG_S8; }
    else if (str_eq(s, str("s9")))   { return REG_S9; }
    else if (str_eq(s, str("s10")))  { return REG_S10; }
    else if (str_eq(s, str("s11")))  { return REG_S11; }
    else if (str_eq(s, str("t3")))   { return REG_T3; }
    else if (str_eq(s, str("t4")))   { return REG_T4; }
    else if (str_eq(s, str("t5")))   { return REG_T5; }
    else if (str_eq(s, str("t6")))   { return REG_T6; }
    else {
        print_error("Unknown register: %.*s\n", (int)s.len, s.data);
    }
}

typedef uint32_t Csr;

static Csr
read_csr(State *st)
{
    Str s = read_token(st);
    if (str_eq(s, str("ustatus")))             { return 0x000; }
    else if (str_eq(s, str("uie")))            { return 0x004; }
    else if (str_eq(s, str("utvec")))          { return 0x005; }
    else if (str_eq(s, str("uscratch")))       { return 0x040; }
    else if (str_eq(s, str("uepc")))           { return 0x041; }
    else if (str_eq(s, str("ucause")))         { return 0x042; }
    else if (str_eq(s, str("utval")))          { return 0x043; }
    else if (str_eq(s, str("uip")))            { return 0x044; }
    else if (str_eq(s, str("fflags")))         { return 0x001; }
    else if (str_eq(s, str("frm")))            { return 0x002; }
    else if (str_eq(s, str("fcsr")))           { return 0x003; }
    else if (str_eq(s, str("cycle")))          { return 0xC00; }
    else if (str_eq(s, str("time")))           { return 0xC01; }
    else if (str_eq(s, str("instret")))        { return 0xC02; }
    else if (str_eq(s, str("hpmcounter3")))    { return 0xC03; }
    else if (str_eq(s, str("hpmcounter4")))    { return 0xC04; }
    else if (str_eq(s, str("hpmcounter31")))   { return 0xC1F; }
    else if (str_eq(s, str("cycleh")))         { return 0xC80; }
    else if (str_eq(s, str("timeh")))          { return 0xC81; }
    else if (str_eq(s, str("instreth")))       { return 0xC82; }
    else if (str_eq(s, str("hpmcounter3h")))   { return 0xC83; }
    else if (str_eq(s, str("hpmcounter4h")))   { return 0xC84; }
    else if (str_eq(s, str("hpmcounter31h")))  { return 0xC9F; }
    else if (str_eq(s, str("sstatus")))        { return 0x100; }
    else if (str_eq(s, str("sedeleg")))        { return 0x102; }
    else if (str_eq(s, str("sideleg")))        { return 0x103; }
    else if (str_eq(s, str("sie")))            { return 0x104; }
    else if (str_eq(s, str("stvec")))          { return 0x105; }
    else if (str_eq(s, str("scounteren")))     { return 0x106; }
    else if (str_eq(s, str("sscratch")))       { return 0x140; }
    else if (str_eq(s, str("sepc")))           { return 0x141; }
    else if (str_eq(s, str("scause")))         { return 0x142; }
    else if (str_eq(s, str("stval")))          { return 0x143; }
    else if (str_eq(s, str("sip")))            { return 0x144; }
    else if (str_eq(s, str("satp")))           { return 0x180; }
    else if (str_eq(s, str("hstatus")))        { return 0x600; }
    else if (str_eq(s, str("hedeleg")))        { return 0x602; }
    else if (str_eq(s, str("hideleg")))        { return 0x603; }
    else if (str_eq(s, str("hcounteren")))     { return 0x606; }
    else if (str_eq(s, str("hgatp")))          { return 0x680; }
    else if (str_eq(s, str("htimedelta")))     { return 0x605; }
    else if (str_eq(s, str("htimedeltah")))    { return 0x615; }
    else if (str_eq(s, str("vsstatus")))       { return 0x200; }
    else if (str_eq(s, str("vsie")))           { return 0x204; }
    else if (str_eq(s, str("vstvec")))         { return 0x205; }
    else if (str_eq(s, str("vsscratch")))      { return 0x240; }
    else if (str_eq(s, str("vsepc")))          { return 0x241; }
    else if (str_eq(s, str("vscause")))        { return 0x242; }
    else if (str_eq(s, str("vstval")))         { return 0x243; }
    else if (str_eq(s, str("vsip")))           { return 0x244; }
    else if (str_eq(s, str("vsatp")))          { return 0x280; }
    else if (str_eq(s, str("mvendorid")))      { return 0xF11; }
    else if (str_eq(s, str("marchid")))        { return 0xF12; }
    else if (str_eq(s, str("mimpid")))         { return 0xF13; }
    else if (str_eq(s, str("mhartid")))        { return 0xF14; }
    else if (str_eq(s, str("mstatus")))        { return 0x300; }
    else if (str_eq(s, str("misa")))           { return 0x301; }
    else if (str_eq(s, str("medeleg")))        { return 0x302; }
    else if (str_eq(s, str("mideleg")))        { return 0x303; }
    else if (str_eq(s, str("mie")))            { return 0x304; }
    else if (str_eq(s, str("mtvec")))          { return 0x305; }
    else if (str_eq(s, str("mcounteren")))     { return 0x306; }
    else if (str_eq(s, str("mstatush")))       { return 0x310; }
    else if (str_eq(s, str("mscratch")))       { return 0x340; }
    else if (str_eq(s, str("mepc")))           { return 0x341; }
    else if (str_eq(s, str("mcause")))         { return 0x342; }
    else if (str_eq(s, str("mtval")))          { return 0x343; }
    else if (str_eq(s, str("mip")))            { return 0x344; }
    else if (str_eq(s, str("pmpcfg0")))        { return 0x3A0; }
    else if (str_eq(s, str("pmpcfg1")))        { return 0x3A1; }
    else if (str_eq(s, str("pmpcfg2")))        { return 0x3A2; }
    else if (str_eq(s, str("pmpcfg3")))        { return 0x3A3; }
    else if (str_eq(s, str("pmpaddr0")))       { return 0x3B0; }
    else if (str_eq(s, str("pmpaddr1")))       { return 0x3B1; }
    else if (str_eq(s, str("pmpaddr15")))      { return 0x3BF; }
    else if (str_eq(s, str("mcycle")))         { return 0xB00; }
    else if (str_eq(s, str("minstret")))       { return 0xB02; }
    else if (str_eq(s, str("mhpmcounter3")))   { return 0xB03; }
    else if (str_eq(s, str("mhpmcounter4")))   { return 0xB04; }
    else if (str_eq(s, str("mhpmcounter31")))  { return 0xB1F; }
    else if (str_eq(s, str("mcycleh")))        { return 0xB80; }
    else if (str_eq(s, str("minstreth")))      { return 0xB82; }
    else if (str_eq(s, str("mhpmcounter3h")))  { return 0xB83; }
    else if (str_eq(s, str("mhpmcounter4h")))  { return 0xB84; }
    else if (str_eq(s, str("mhpmcounter31h"))) { return 0xB9F; }
    else if (str_eq(s, str("mcountinhibit")))  { return 0x320; }
    else if (str_eq(s, str("mhpmevent3")))     { return 0x323; }
    else if (str_eq(s, str("mhpmevent4")))     { return 0x324; }
    else if (str_eq(s, str("mhpmevent31")))    { return 0x33F; }
    else if (str_eq(s, str("tselect")))        { return 0x7A0; }
    else if (str_eq(s, str("tdata1")))         { return 0x7A1; }
    else if (str_eq(s, str("tdata2")))         { return 0x7A2; }
    else if (str_eq(s, str("tdata3")))         { return 0x7A3; }
    else if (str_eq(s, str("dcsr")))           { return 0x7B0; }
    else if (str_eq(s, str("dpc")))            { return 0x7B1; }
    else if (str_eq(s, str("dscratch0")))      { return 0x7B2; }
    else if (str_eq(s, str("dscratch1")))      { return 0x7B3; }
    else {
        print_error("Unknown csr: %.*s\n", (int)s.len, s.data);
    }
}

struct Expr {
    bool known;
    union {
        int32_t result;  // if known
        Str label;       // if not known
    };
};
typedef struct Expr Expr;

static Expr
read_expr(State *st)
{
    Str t1 = read_token(st);
    if (t1.len) {
        if (is_labelstart(t1.data[0])) {
            return (Expr) {
                .known = false,
                .label = t1,
            };
        } else {
            Str n = t1;
            int sign = 1;
            if (t1.data[0] == '-') {
                sign = -1;
                n = read_token(st);
            }
            if (n.len &&  is_digit(n.data[0])) {
                return (Expr) {
                    .known = true,
                    .result = str_to_i32(n) * sign,
                };
            }
        }
    }
}

#include "instructions.c"

#define MAX_OUTPUT_SIZE 1000
struct Output {
    uint32_t output_data[MAX_OUTPUT_SIZE];
    size_t output_len;
};
typedef struct Output Output;

static uint32_t *
output32(Output *out, uint32_t data)
{
    if (out->output_len >= MAX_OUTPUT_SIZE) {
        abort();
    }
    out->output_data[out->output_len] = data;
    out->output_len++;
    return &out->output_data[out->output_len - 1];
}

struct CompiledInstr {
    uint32_t instr;
    bool replace_imm;

    // Only used if replace_imm == true.
    UnknownValue unknown_value;
};
typedef struct CompiledInstr CompiledInstr;

static CompiledInstr
compile_instr_type_r(State *st, uint32_t (fn)(Reg, Reg, Reg))
{
    Reg rd = read_reg(st);
    Str comma = read_token(st);
    Reg rs1 = read_reg(st);
    comma = read_token(st);
    Reg rs2 = read_reg(st);
    return (CompiledInstr) {
        .instr = fn(rd, rs1, rs2),
        .replace_imm = false,
    };
}

static CompiledInstr
compile_instr_type_i(State *st, uint32_t (fn)(Reg, Reg, int32_t), enum InstrType type)
{
    Reg rd = read_reg(st);
    Str comma = read_token(st);
    Reg rs1 = read_reg(st);
    comma = read_token(st);
    Expr e = read_expr(st);
    return (CompiledInstr) {
        .instr = fn(rd, rs1, e.known ? e.result : 0),
        .replace_imm = !e.known,
        .unknown_value = {
            .type = type,
            .label = e.known ? (Str){} : e.label,
        },
    };
}

static CompiledInstr
compile_instr_type_u(State *st, uint32_t (fn)(Reg, int32_t))
{
    Reg rd = read_reg(st);
    Str comma = read_token(st);
    Expr e = read_expr(st);
    return (CompiledInstr) {
        .instr = fn(rd, e.known ? e.result << 12 : 0),
        .replace_imm = !e.known,
    };
}

static CompiledInstr
compile_instr_rm(State *st, uint32_t (fn)(Reg, Reg, int32_t))
{
    Reg r1 = read_reg(st);
    Str comma = read_token(st);
    Expr e = read_expr(st);
    Str par = read_token(st);
    Reg r2 = read_reg(st);
    par = read_token(st);
    return (CompiledInstr) {
        .instr = fn(r1, r2, e.known ? e.result : 0),
        .replace_imm = !e.known,
    };
}

static CompiledInstr
compile_instr_type_j(State *st, uint32_t (fn)(Reg, int32_t), enum InstrType type)
{
    Reg rd = read_reg(st);
    Str comma = read_token(st);
    Expr e = read_expr(st);
    return (CompiledInstr) {
        .instr = fn(rd, e.known ? e.result : 0),
        .replace_imm = !e.known,
        .unknown_value = {
            .type = type,
            .label = e.known ? (Str){} : e.label,
        },
    };
}

static CompiledInstr
compile_instr_type_csr(State *st, uint32_t (fn)(Reg, Csr, Reg))
{
    Reg rd = read_reg(st);
    Str comma = read_token(st);
    Csr csr = read_csr(st);
    comma = read_token(st);
    Reg rs1 = read_reg(st);
    return (CompiledInstr) {
        .instr = fn(rd, csr, rs1),
    };
}

static CompiledInstr
compile_instr_type_csri(State *st, uint32_t (fn)(Reg, Csr, int32_t))
{
    Reg rd = read_reg(st);
    Str comma = read_token(st);
    Csr csr = read_csr(st);
    comma = read_token(st);
    Expr e = read_expr(st);
    return (CompiledInstr) {
        .instr = fn(rd, csr, e.known ? e.result : 0),
        .replace_imm = !e.known,
    };
}

enum Target {
    TARGET_RV32, TARGET_RV64,
};
typedef enum Target Target;

static void
compile_inst(Output *out, State *st, Str first, Target target)
{
    CompiledInstr instr = {0};
    if (str_eq(first, str("add"))) {
        instr = compile_instr_type_r(st, instr_add);
    } else if (str_eq(first, str("addi"))) {
        instr = compile_instr_type_i(st, instr_addi, INSTR_I);
    } else if (str_eq(first, str("jal"))) {
        instr = compile_instr_type_j(st, instr_jal, INSTR_J);
        instr.unknown_value.relative_to = st->pc;
    } else if (str_eq(first, str("jalr"))) {
        instr = compile_instr_type_i(st, instr_jalr, INSTR_I);
    } else if (str_eq(first, str("sub"))) {
        instr = compile_instr_type_r(st, instr_sub);
    } else if (str_eq(first, str("auipc"))) {
        instr = compile_instr_type_u(st, instr_auipc);
    } else if (str_eq(first, str("lui"))) {
        instr = compile_instr_type_u(st, instr_lui);
    } else if (str_eq(first, str("and"))) {
        instr = compile_instr_type_r(st, instr_and);
    } else if (str_eq(first, str("andi"))) {
        instr = compile_instr_type_i(st, instr_andi, INSTR_I);
    } else if (str_eq(first, str("or"))) {
        instr = compile_instr_type_r(st, instr_or);
    } else if (str_eq(first, str("ori"))) {
        instr = compile_instr_type_i(st, instr_ori, INSTR_I);
    } else if (str_eq(first, str("beq"))) {
        instr = compile_instr_type_i(st, instr_beq, INSTR_B);
        instr.unknown_value.relative_to = st->pc;
    } else if (str_eq(first, str("bge"))) {
        instr = compile_instr_type_i(st, instr_bge, INSTR_B);
        instr.unknown_value.relative_to = st->pc;
    } else if (str_eq(first, str("bgeu"))) {
        instr = compile_instr_type_i(st, instr_bgeu, INSTR_B);
        instr.unknown_value.relative_to = st->pc;
    } else if (str_eq(first, str("blt"))) {
        instr = compile_instr_type_i(st, instr_blt, INSTR_B);
        instr.unknown_value.relative_to = st->pc;
    } else if (str_eq(first, str("bltu"))) {
        instr = compile_instr_type_i(st, instr_bltu, INSTR_B);
        instr.unknown_value.relative_to = st->pc;
    } else if (str_eq(first, str("bne"))) {
        instr = compile_instr_type_i(st, instr_bne, INSTR_B);
        instr.unknown_value.relative_to = st->pc;
    } else if (str_eq(first, str("lb"))) {
        instr = compile_instr_rm(st, instr_lb);
    } else if (str_eq(first, str("lbu"))) {
        instr = compile_instr_rm(st, instr_lbu);
    } else if (str_eq(first, str("sb"))) {
        instr = compile_instr_rm(st, instr_sb);
    } else if (str_eq(first, str("lw"))) {
        instr = compile_instr_rm(st, instr_lw);
    } else if (str_eq(first, str("lwu"))) {
        instr = compile_instr_rm(st, instr_lwu);
    } else if (str_eq(first, str("sw"))) {
        instr = compile_instr_rm(st, instr_sw);
    } else if (target == TARGET_RV64 && str_eq(first, str("ld"))) {
        instr = compile_instr_rm(st, instr_ld);
    } else if (target == TARGET_RV64 && str_eq(first, str("sd"))) {
        instr = compile_instr_rm(st, instr_sd);
    } else if (str_eq(first, str("lh"))) {
        instr = compile_instr_rm(st, instr_lh);
    } else if (str_eq(first, str("lhu"))) {
        instr = compile_instr_rm(st, instr_lhu);
    } else if (str_eq(first, str("sh"))) {
        instr = compile_instr_rm(st, instr_sh);
    } else if (target == TARGET_RV64 && str_eq(first, str("addw"))) {
        instr = compile_instr_type_r(st, instr64_addw);
    } else if (target == TARGET_RV64 && str_eq(first, str("subw"))) {
        instr = compile_instr_type_r(st, instr64_subw);
    } else if (target == TARGET_RV64 && str_eq(first, str("addiw"))) {
        instr = compile_instr_type_i(st, instr64_addiw, INSTR_I);
    } else if (target == TARGET_RV32 && str_eq(first, str("slli"))) {
        instr = compile_instr_type_i(st, instr32_slli, INSTR_I);
    } else if (target == TARGET_RV64 && str_eq(first, str("slli"))) {
        instr = compile_instr_type_i(st, instr64_slli, INSTR_I);
    } else if (target == TARGET_RV32 && str_eq(first, str("srli"))) {
        instr = compile_instr_type_i(st, instr32_srli, INSTR_I);
    } else if (target == TARGET_RV64 && str_eq(first, str("srli"))) {
        instr = compile_instr_type_i(st, instr64_srli, INSTR_I);
    } else if (str_eq(first, str("sll"))) {
        instr = compile_instr_type_r(st, instr_sll);
    } else if (str_eq(first, str("srl"))) {
        instr = compile_instr_type_r(st, instr_srl);
    } else if (target == TARGET_RV64 && str_eq(first, str("sllw"))) {
        instr = compile_instr_type_r(st, instr64_sllw);
    } else if (target == TARGET_RV64 && str_eq(first, str("slliw"))) {
        instr = compile_instr_type_i(st, instr64_slliw, INSTR_I);
    } else if (target == TARGET_RV64 && str_eq(first, str("srlw"))) {
        instr = compile_instr_type_r(st, instr64_srlw);
    } else if (target == TARGET_RV64 && str_eq(first, str("srliw"))) {
        instr = compile_instr_type_i(st, instr64_srliw, INSTR_I);
    } else if (str_eq(first, str("xor"))) {
        instr = compile_instr_type_r(st, instr_xor);
    } else if (str_eq(first, str("xori"))) {
        instr = compile_instr_type_i(st, instr_xori, INSTR_I);
    } else if (str_eq(first, str("slt"))) {
        instr = compile_instr_type_r(st, instr_slt);
    } else if (str_eq(first, str("sltu"))) {
        instr = compile_instr_type_r(st, instr_sltu);
    } else if (str_eq(first, str("slti"))) {
        instr = compile_instr_type_i(st, instr_slti, INSTR_I);
    } else if (str_eq(first, str("sltiu"))) {
        instr = compile_instr_type_i(st, instr_sltiu, INSTR_I);
    } else if (target == TARGET_RV32 && str_eq(first, str("srai"))) {
        instr = compile_instr_type_i(st, instr32_srai, INSTR_I);
    } else if (target == TARGET_RV64 && str_eq(first, str("srai"))) {
        instr = compile_instr_type_i(st, instr64_srai, INSTR_I);
    } else if (target == TARGET_RV64 && str_eq(first, str("sraiw"))) {
        instr = compile_instr_type_i(st, instr64_sraiw, INSTR_I);
    } else if (target == TARGET_RV64 && str_eq(first, str("sraw"))) {
        instr = compile_instr_type_r(st, instr64_sraw);
    } else if (str_eq(first, str("sra"))) {
        instr = compile_instr_type_r(st, instr_sra);
    } else if (str_eq(first, str("nop"))) {
        instr = (CompiledInstr){.instr = instr_addi(REG_ZERO, REG_ZERO, 0)};
    } else if (str_eq(first, str("ecall"))) {
        instr = (CompiledInstr){.instr = instr_ecall()};
    } else if (str_eq(first, str("ebreak"))) {
        instr = (CompiledInstr){.instr = instr_ebreak()};
    } else if (str_eq(first, str("csrrc"))) {
        instr = compile_instr_type_csr(st, instr_csrrc);
    } else if (str_eq(first, str("csrrci"))) {
        instr = compile_instr_type_csri(st, instr_csrrci);
    } else if (str_eq(first, str("csrrs"))) {
        instr = compile_instr_type_csr(st, instr_csrrs);
    } else if (str_eq(first, str("csrrsi"))) {
        instr = compile_instr_type_csri(st, instr_csrrsi);
    } else if (str_eq(first, str("csrrw"))) {
        instr = compile_instr_type_csr(st, instr_csrrw);
    } else if (str_eq(first, str("csrrwi"))) {
        instr = compile_instr_type_csri(st, instr_csrrwi);
    } else if (str_eq(first, str("wfi"))) {
        instr = (CompiledInstr){.instr = instr_wfi()};
    } else {
        print_error("Unknown instruction: %.*s\n",
                (int)first.len, first.data);
    }
    assert(instr.instr != 0);

    uint32_t *output_ptr = output32(out, instr.instr);

    if (instr.replace_imm) {
        if (st->n_unknowns < MAX_UNKNOWNS) {
            instr.unknown_value.ptr = output_ptr;
            st->unknowns[st->n_unknowns] = instr.unknown_value;
            st->n_unknowns++;
        } else {
            abort();
        }
    }
}

static LabelValue *
get_label(const State *st, Str label)
{
    for (size_t i = 0; i < st->n_labels; i++) {
        LabelValue *lab = &st->labels[i];
        if (str_eq(lab->label, label)) {
            return lab;
        }
    }
    return NULL;
}

static void
compile(const char *code, size_t code_size, Target target)
{
    Output out = {0};
    State st = {
        .code = {code, code_size},
        .i = 0,
    };

    for (;;) {
        Str first = read_token(&st);
        if (first.len == 0) {
            // Nothing more to read.
            break;
        }
        if (first.len && first.data[0] == '\n') {
            // End of line. Read next instruction.
            continue;
        }
        State st_tmp = st;
        Str second = read_token(&st_tmp);
        if (str_eq(second, str(":"))) {
            st = st_tmp;
            if (st.n_labels >= ARR_SIZE(st.labels)) {
                print_error("Too many labels");
                break;
            }
            st.labels[st.n_labels] = (LabelValue) {
                .label = first,
                .value = st.pc,
            };
            st.n_labels++;
        } else {
            compile_inst(&out, &st, first, target);
            st.pc += 4;
        }
    }

    // Fill in the unknown (but now known) values.
    for (size_t i = 0; i < st.n_unknowns; i++) {
        UnknownValue *ukv = &st.unknowns[i];
        int64_t diff = (int64_t)get_label(&st, ukv->label)->value
            - ukv->relative_to;
        switch (ukv->type) {
        case INSTR_I:
            *ukv->ptr |= bits(diff, 11, 0) << 20;
            break;
        case INSTR_J:
            *ukv->ptr |= bits(diff, 20, 20) << 31
                | bits(diff, 10, 1) << 21
                | bits(diff, 11, 11) << 20
                | bits(diff, 19, 12) << 12;
            break;
        case INSTR_B:
            *ukv->ptr |= bits(diff, 12, 12) << 31
                | bits(diff, 10, 5) << 25
                | bits(diff, 4, 1) << 8
                | bits(diff, 11, 11) << 7;
            break;
        }
    }

    write(1, out.output_data, out.output_len * sizeof *out.output_data);
}

int
main(int argc, char **argv)
{
    Target target = TARGET_RV64;
    if (argc != 2) {
        fprintf(stderr, "Usage: rvas input-file\n");
        return 1;
    }
    char *filename = argv[1];
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Could not open file.\n");
        return 1;
    }
    off_t size = lseek(fd, 0, SEEK_END);
    int prot = PROT_READ;
    int flags = MAP_PRIVATE;
    void *data = mmap(0, size, prot, flags, fd, 0);
    if (!data) {
        fprintf(stderr, "Could not read file.\n");
        return 1;
    }
    compile((char *)data, size, target);
}
