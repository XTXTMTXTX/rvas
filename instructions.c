static uint32_t
bits(uint32_t n, uint32_t hi, uint32_t lo)
{
    assert(hi < 32);
    assert(lo < 32);
    assert(hi >= lo);
    return n >> lo & ((1 << (hi - lo + 1)) - 1);
}


static uint32_t
instr_type_r(Reg rd, Reg rs1, Reg rs2)
{
    return rs2 << 20 | rs1 << 15 | rd << 7;
}

static uint32_t
instr_type_i(Reg rd, Reg rs1, int32_t imm)
{
    return bits(imm, 11, 0) << 20 | rs1 << 15 | rd << 7;
}

static uint32_t
instr_type_u(Reg rd, int32_t imm)
{
    return bits(imm, 31, 12) << 12 | rd << 7;
}

static uint32_t
instr_type_s(Reg rs1, Reg rs2, int32_t imm)
{
    return bits(imm, 11, 5) << 25
        |  rs2 << 20
        |  rs1 << 15
        |  bits(imm, 4, 0) << 7;
}

static uint32_t
instr_type_j(Reg rd, int32_t imm)
{
    return bits(imm, 20, 20) << 31
        |  bits(imm, 10, 1) << 21
        |  bits(imm, 11, 11) << 20
        |  bits(imm, 19, 12) << 12
        |  rd << 7;
}

static uint32_t
instr_type_csr(Reg rd, Csr csr, Reg rs1)
{
    return csr << 20 | rs1 << 15 | rd << 7;
}

static uint32_t
instr_type_csri(Reg rd, Csr csr, int32_t imm)
{
    return csr << 20 | bits(imm, 4, 0) << 15 | rd << 7;
}


static uint32_t
instr_add(Reg rd, Reg rs1, Reg rs2)
{
    return instr_type_r(rd, rs1, rs2) | 0b0110011;
}

static uint32_t
instr_addi(Reg rd, Reg rs1, int32_t imm)
{
    return instr_type_i(rd, rs1, imm) | 0b0010011;
}

static uint32_t
instr_and(Reg rd, Reg rs1, Reg rs2)
{
    return instr_type_r(rd, rs1, rs2) | 0b111000000110011;
}

static uint32_t
instr_andi(Reg rd, Reg rs1, int32_t imm)
{
    return instr_type_i(rd, rs1, imm) | 0b111000000010011;
}

static uint32_t
instr_auipc(Reg rd, int32_t imm)
{
    return instr_type_u(rd, imm) | 0b0010111;
}

static uint32_t
instr_lui(Reg rd, int32_t imm)
{
    return instr_type_u(rd, imm) | 0b0110111;
}

static uint32_t
instr_csrrc(Reg rd, Csr csr, Reg rs1)
{
    return instr_type_csr(rd, csr, rs1) | 0b11000001110011;
}

static uint32_t
instr_csrrci(Reg rd, Csr csr, int32_t imm)
{
    return instr_type_csri(rd, csr, imm) | 0b111000001110011;
}

static uint32_t
instr_csrrs(Reg rd, Csr csr, Reg rs1)
{
    return instr_type_csr(rd, csr, rs1) | 0b10000001110011;
}

static uint32_t
instr_csrrsi(Reg rd, Csr csr, int32_t imm)
{
    return instr_type_csri(rd, csr, imm) | 0b110000001110011;
}

static uint32_t
instr_ebreak()
{
    return 0b100000000000001110011;
}

static uint32_t
instr_ecall()
{
    return 0b1110011;
}

static uint32_t
instr_jal(Reg rd, int32_t imm)
{
    return instr_type_j(rd, imm) | 0b1101111;
}

static uint32_t
instr_lb(Reg rd, Reg rs1, int32_t imm)
{
    return instr_type_i(rd, rs1, imm) | 0b11;
}

static uint32_t
instr_lw(Reg rd, Reg rs1, int32_t imm)
{
    return instr_type_i(rd, rs1, imm) | 0b10000000000011;
}

static uint32_t
instr_sb(Reg rs1, Reg rs2, int32_t imm)
{
    return instr_type_s(rs1, rs2, imm) | 0b0100011;
}

static uint32_t
instr_sw(Reg rs1, Reg rs2, int32_t imm)
{
    return instr_type_s(rs1, rs2, imm) | 0b10000000100011;
}

static uint32_t
instr_wfi()
{
    return 0b10000010100000000000001110011;
}
