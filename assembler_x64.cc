// Copyright (c) 2013, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "assembler.h"
#include "globals.h"

Assembler::Assembler() {}

void Assembler::InitializeMemoryWithBreakpoints(uword data, intptr_t length) {
  memset(reinterpret_cast<void *>(data), Instr::kBreakPointInstruction, length);
}

void Assembler::call(Label *label) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  static const int kSize = 5;
  EmitUint8(0xE8);
  EmitLabel(label, kSize);
}

void Assembler::call(const ExternalLabel *label) {
  { // Encode movq(TMP, Immediate(label->address())), but always as imm64.
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    EmitRegisterREX(TMP, REX_W);
    EmitUint8(0xB8 | (TMP & 7));
    EmitInt64(label->address());
  }
  call(TMP);
}

void Assembler::pushq(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitRegisterREX(reg, REX_NONE);
  EmitUint8(0x50 | (reg & 7));
}

void Assembler::pushq(const Immediate &imm) {
  if (imm.is_int8()) {
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    EmitUint8(0x6A);
    EmitUint8(imm.value() & 0xFF);
  } else if (imm.is_int32()) {
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    EmitUint8(0x68);
    EmitImmediate(imm);
  } else {
    movq(TMP, imm);
    pushq(TMP);
  }
}

void Assembler::popq(Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitRegisterREX(reg, REX_NONE);
  EmitUint8(0x58 | (reg & 7));
}

void Assembler::setcc(Condition condition, ByteRegister dst) {
  ASSERT(dst != kNoByteRegister);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (dst >= 8) {
    EmitUint8(REX_PREFIX | (((dst & 0x08) != 0) ? REX_B : REX_NONE));
  }
  EmitUint8(0x0F);
  EmitUint8(0x90 + condition);
  EmitUint8(0xC0 + (dst & 0x07));
}

void Assembler::EmitQ(int reg, const Address &address, int opcode, int prefix2,
                      int prefix1) {
  ASSERT(reg <= XMM15);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (prefix1 >= 0) {
    EmitUint8(prefix1);
  }
  EmitOperandREX(reg, address, REX_W);
  if (prefix2 >= 0) {
    EmitUint8(prefix2);
  }
  EmitUint8(opcode);
  EmitOperand(reg & 7, address);
}

void Assembler::EmitL(int reg, const Address &address, int opcode, int prefix2,
                      int prefix1) {
  ASSERT(reg <= XMM15);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (prefix1 >= 0) {
    EmitUint8(prefix1);
  }
  EmitOperandREX(reg, address, REX_NONE);
  if (prefix2 >= 0) {
    EmitUint8(prefix2);
  }
  EmitUint8(opcode);
  EmitOperand(reg & 7, address);
}

void Assembler::EmitW(Register reg, const Address &address, int opcode,
                      int prefix2, int prefix1) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (prefix1 >= 0) {
    EmitUint8(prefix1);
  }
  EmitOperandSizeOverride();
  EmitOperandREX(reg, address, REX_NONE);
  if (prefix2 >= 0) {
    EmitUint8(prefix2);
  }
  EmitUint8(opcode);
  EmitOperand(reg & 7, address);
}

void Assembler::movl(Register dst, const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  Operand operand(dst);
  EmitOperandREX(0, operand, REX_NONE);
  EmitUint8(0xC7);
  EmitOperand(0, operand);
  ASSERT(imm.is_int32());
  EmitImmediate(imm);
}

void Assembler::movl(const Address &dst, const Immediate &imm) {
  movl(TMP, imm);
  movl(dst, TMP);
}

void Assembler::movb(const Address &dst, const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitOperandREX(0, dst, REX_NONE);
  EmitUint8(0xC6);
  EmitOperand(0, dst);
  ASSERT(imm.is_int8());
  EmitUint8(imm.value() & 0xFF);
}

void Assembler::movw(Register dst, const Address &src) {
  FATAL("Use movzxw or movsxw instead.");
}

void Assembler::movw(const Address &dst, const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitOperandSizeOverride();
  EmitOperandREX(0, dst, REX_NONE);
  EmitUint8(0xC7);
  EmitOperand(0, dst);
  EmitUint8(imm.value() & 0xFF);
  EmitUint8((imm.value() >> 8) & 0xFF);
}

void Assembler::movq(Register dst, const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (imm.is_uint32()) {
    // Pick single byte B8 encoding if possible. If dst < 8 then we also omit
    // the Rex byte.
    EmitRegisterREX(dst, REX_NONE);
    EmitUint8(0xB8 | (dst & 7));
    EmitUInt32(imm.value());
  } else if (imm.is_int32()) {
    // Sign extended C7 Cx encoding if we have a negative input.
    Operand operand(dst);
    EmitOperandREX(0, operand, REX_W);
    EmitUint8(0xC7);
    EmitOperand(0, operand);
    EmitImmediate(imm);
  } else {
    // Full 64 bit immediate encoding.
    EmitRegisterREX(dst, REX_W);
    EmitUint8(0xB8 | (dst & 7));
    EmitImmediate(imm);
  }
}

void Assembler::movq(const Address &dst, const Immediate &imm) {
  if (imm.is_int32()) {
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    EmitOperandREX(0, dst, REX_W);
    EmitUint8(0xC7);
    EmitOperand(0, dst);
    EmitImmediate(imm);
  } else {
    movq(TMP, imm);
    movq(dst, TMP);
  }
}

void Assembler::EmitSimple(int opcode, int opcode2) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(opcode);
  if (opcode2 != -1) {
    EmitUint8(opcode2);
  }
}

void Assembler::EmitQ(int dst, int src, int opcode, int prefix2, int prefix1) {
  ASSERT(src <= XMM15);
  ASSERT(dst <= XMM15);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (prefix1 >= 0) {
    EmitUint8(prefix1);
  }
  EmitRegRegRex(dst, src, REX_W);
  if (prefix2 >= 0) {
    EmitUint8(prefix2);
  }
  EmitUint8(opcode);
  EmitRegisterOperand(dst & 7, src);
}

void Assembler::EmitL(int dst, int src, int opcode, int prefix2, int prefix1) {
  ASSERT(src <= XMM15);
  ASSERT(dst <= XMM15);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (prefix1 >= 0) {
    EmitUint8(prefix1);
  }
  EmitRegRegRex(dst, src);
  if (prefix2 >= 0) {
    EmitUint8(prefix2);
  }
  EmitUint8(opcode);
  EmitRegisterOperand(dst & 7, src);
}

void Assembler::EmitW(Register dst, Register src, int opcode, int prefix2,
                      int prefix1) {
  ASSERT(src <= R15);
  ASSERT(dst <= R15);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (prefix1 >= 0) {
    EmitUint8(prefix1);
  }
  EmitOperandSizeOverride();
  EmitRegRegRex(dst, src);
  if (prefix2 >= 0) {
    EmitUint8(prefix2);
  }
  EmitUint8(opcode);
  EmitRegisterOperand(dst & 7, src);
}

void Assembler::CmpPS(XmmRegister dst, XmmRegister src, int condition) {
  EmitL(dst, src, 0xC2, 0x0F);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(condition);
}

void Assembler::set1ps(XmmRegister dst, Register tmp1, const Immediate &imm) {
  // Load 32-bit immediate value into tmp1.
  movl(tmp1, imm);
  // Move value from tmp1 into dst.
  movd(dst, tmp1);
  // Broadcast low lane into other three lanes.
  shufps(dst, dst, Immediate(0x0));
}

void Assembler::shufps(XmmRegister dst, XmmRegister src, const Immediate &imm) {
  EmitL(dst, src, 0xC6, 0x0F);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  ASSERT(imm.is_uint8());
  EmitUint8(imm.value());
}

void Assembler::shufpd(XmmRegister dst, XmmRegister src, const Immediate &imm) {
  EmitL(dst, src, 0xC6, 0x0F, 0x66);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  ASSERT(imm.is_uint8());
  EmitUint8(imm.value());
}

void Assembler::roundsd(XmmRegister dst, XmmRegister src, RoundingMode mode) {
  ASSERT(src <= XMM15);
  ASSERT(dst <= XMM15);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0x66);
  EmitRegRegRex(dst, src);
  EmitUint8(0x0F);
  EmitUint8(0x3A);
  EmitUint8(0x0B);
  EmitRegisterOperand(dst & 7, src);
  // Mask precision exeption.
  EmitUint8(static_cast<uint8_t>(mode) | 0x8);
}

void Assembler::fldl(const Address &src) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xDD);
  EmitOperand(0, src);
}

void Assembler::fstpl(const Address &dst) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xDD);
  EmitOperand(3, dst);
}

void Assembler::ffree(intptr_t value) {
  ASSERT(value < 7);
  EmitSimple(0xDD, 0xC0 + value);
}

void Assembler::testb(const Address &address, const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitOperandREX(0, address, REX_NONE);
  EmitUint8(0xF6);
  EmitOperand(0, address);
  ASSERT(imm.is_int8());
  EmitUint8(imm.value() & 0xFF);
}

void Assembler::testb(const Address &address, Register reg) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitOperandREX(reg, address, REX_NONE);
  EmitUint8(0x84);
  EmitOperand(reg & 7, address);
}

void Assembler::testq(Register reg, const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (imm.is_uint8()) {
    // Use zero-extended 8-bit immediate.
    if (reg >= 4) {
      // We need the Rex byte to give access to the SIL and DIL registers (the
      // low bytes of RSI and RDI).
      EmitRegisterREX(reg, REX_NONE, /* force = */ true);
    }
    if (reg == RAX) {
      EmitUint8(0xA8);
    } else {
      EmitUint8(0xF6);
      EmitUint8(0xC0 + (reg & 7));
    }
    EmitUint8(imm.value() & 0xFF);
  } else if (imm.is_uint32()) {
    if (reg == RAX) {
      EmitUint8(0xA9);
    } else {
      EmitRegisterREX(reg, REX_NONE);
      EmitUint8(0xF7);
      EmitUint8(0xC0 | (reg & 7));
    }
    EmitUInt32(imm.value());
  } else {
    // Sign extended version of 32 bit test.
    ASSERT(imm.is_int32());
    EmitRegisterREX(reg, REX_W);
    if (reg == RAX) {
      EmitUint8(0xA9);
    } else {
      EmitUint8(0xF7);
      EmitUint8(0xC0 | (reg & 7));
    }
    EmitImmediate(imm);
  }
}

void Assembler::AluL(uint8_t modrm_opcode, Register dst, const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitRegisterREX(dst, REX_NONE);
  EmitComplex(modrm_opcode, Operand(dst), imm);
}

void Assembler::AluB(uint8_t modrm_opcode, const Address &dst,
                     const Immediate &imm) {
  ASSERT(imm.is_uint8() || imm.is_int8());
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitOperandREX(modrm_opcode, dst, REX_NONE);
  EmitUint8(0x80);
  EmitOperand(modrm_opcode, dst);
  EmitUint8(imm.value() & 0xFF);
}

void Assembler::AluW(uint8_t modrm_opcode, const Address &dst,
                     const Immediate &imm) {
  ASSERT(imm.is_int16() || imm.is_uint16());
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitOperandSizeOverride();
  EmitOperandREX(modrm_opcode, dst, REX_NONE);
  if (imm.is_int8()) {
    EmitSignExtendedInt8(modrm_opcode, dst, imm);
  } else {
    EmitUint8(0x81);
    EmitOperand(modrm_opcode, dst);
    EmitUint8(imm.value() & 0xFF);
    EmitUint8((imm.value() >> 8) & 0xFF);
  }
}

void Assembler::AluL(uint8_t modrm_opcode, const Address &dst,
                     const Immediate &imm) {
  ASSERT(imm.is_int32());
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitOperandREX(modrm_opcode, dst, REX_NONE);
  EmitComplex(modrm_opcode, dst, imm);
}

void Assembler::AluQ(uint8_t modrm_opcode, uint8_t opcode, Register dst,
                     const Immediate &imm) {
  Operand operand(dst);
  if (modrm_opcode == 4 && imm.is_uint32()) {
    // We can use andl for andq.
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    EmitRegisterREX(dst, REX_NONE);
    // Would like to use EmitComplex here, but it doesn't like uint32
    // immediates.
    if (imm.is_int8()) {
      EmitSignExtendedInt8(modrm_opcode, operand, imm);
    } else {
      if (dst == RAX) {
        EmitUint8(0x25);
      } else {
        EmitUint8(0x81);
        EmitOperand(modrm_opcode, operand);
      }
      EmitUInt32(imm.value());
    }
  } else if (imm.is_int32()) {
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    EmitRegisterREX(dst, REX_W);
    EmitComplex(modrm_opcode, operand, imm);
  } else {
    ASSERT(dst != TMP);
    movq(TMP, imm);
    EmitQ(dst, TMP, opcode);
  }
}

void Assembler::AluQ(uint8_t modrm_opcode, uint8_t opcode, const Address &dst,
                     const Immediate &imm) {
  if (imm.is_int32()) {
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    EmitOperandREX(modrm_opcode, dst, REX_W);
    EmitComplex(modrm_opcode, dst, imm);
  } else {
    movq(TMP, imm);
    EmitQ(TMP, dst, opcode);
  }
}

void Assembler::cqo() {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitRegisterREX(RAX, REX_W);
  EmitUint8(0x99);
}

void Assembler::EmitUnaryQ(Register reg, int opcode, int modrm_code) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitRegisterREX(reg, REX_W);
  EmitUint8(opcode);
  EmitOperand(modrm_code, Operand(reg));
}

void Assembler::EmitUnaryL(Register reg, int opcode, int modrm_code) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitRegisterREX(reg, REX_NONE);
  EmitUint8(opcode);
  EmitOperand(modrm_code, Operand(reg));
}

void Assembler::EmitUnaryQ(const Address &address, int opcode, int modrm_code) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  Operand operand(address);
  EmitOperandREX(modrm_code, operand, REX_W);
  EmitUint8(opcode);
  EmitOperand(modrm_code, operand);
}

void Assembler::EmitUnaryL(const Address &address, int opcode, int modrm_code) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  Operand operand(address);
  EmitOperandREX(modrm_code, operand, REX_NONE);
  EmitUint8(opcode);
  EmitOperand(modrm_code, operand);
}

void Assembler::imull(Register reg, const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  Operand operand(reg);
  EmitOperandREX(reg, operand, REX_NONE);
  EmitUint8(0x69);
  EmitOperand(reg & 7, Operand(reg));
  EmitImmediate(imm);
}

void Assembler::imulq(Register reg, const Immediate &imm) {
  if (imm.is_int32()) {
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    Operand operand(reg);
    EmitOperandREX(reg, operand, REX_W);
    EmitUint8(0x69);
    EmitOperand(reg & 7, Operand(reg));
    EmitImmediate(imm);
  } else {
    ASSERT(reg != TMP);
    movq(TMP, imm);
    imulq(reg, TMP);
  }
}

void Assembler::MulImmediate(Register reg, const Immediate &imm,
                             OperandWidth width) {
  if (imm.is_int32()) {
    if (width == k32Bit) {
      imull(reg, imm);
    } else {
      imulq(reg, imm);
    }
  } else {
    ASSERT(reg != TMP);
    ASSERT(width != k32Bit);
    movq(TMP, imm);
    imulq(reg, TMP);
  }
}

void Assembler::shll(Register reg, const Immediate &imm) {
  EmitGenericShift(false, 4, reg, imm);
}

void Assembler::shll(Register operand, Register shifter) {
  EmitGenericShift(false, 4, operand, shifter);
}

void Assembler::shrl(Register reg, const Immediate &imm) {
  EmitGenericShift(false, 5, reg, imm);
}

void Assembler::shrl(Register operand, Register shifter) {
  EmitGenericShift(false, 5, operand, shifter);
}

void Assembler::sarl(Register reg, const Immediate &imm) {
  EmitGenericShift(false, 7, reg, imm);
}

void Assembler::sarl(Register operand, Register shifter) {
  EmitGenericShift(false, 7, operand, shifter);
}

void Assembler::shldl(Register dst, Register src, const Immediate &imm) {
  EmitL(src, dst, 0xA4, 0x0F);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  ASSERT(imm.is_int8());
  EmitUint8(imm.value() & 0xFF);
}

void Assembler::shlq(Register reg, const Immediate &imm) {
  EmitGenericShift(true, 4, reg, imm);
}

void Assembler::shlq(Register operand, Register shifter) {
  EmitGenericShift(true, 4, operand, shifter);
}

void Assembler::shrq(Register reg, const Immediate &imm) {
  EmitGenericShift(true, 5, reg, imm);
}

void Assembler::shrq(Register operand, Register shifter) {
  EmitGenericShift(true, 5, operand, shifter);
}

void Assembler::sarq(Register reg, const Immediate &imm) {
  EmitGenericShift(true, 7, reg, imm);
}

void Assembler::sarq(Register operand, Register shifter) {
  EmitGenericShift(true, 7, operand, shifter);
}

void Assembler::shldq(Register dst, Register src, const Immediate &imm) {
  EmitQ(src, dst, 0xA4, 0x0F);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  ASSERT(imm.is_int8());
  EmitUint8(imm.value() & 0xFF);
}

void Assembler::btq(Register base, int bit) {
  ASSERT(bit >= 0 && bit < 64);
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  Operand operand(base);
  EmitOperandREX(4, operand, bit >= 32 ? REX_W : REX_NONE);
  EmitUint8(0x0F);
  EmitUint8(0xBA);
  EmitOperand(4, operand);
  EmitUint8(bit);
}

void Assembler::enter(const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  EmitUint8(0xC8);
  ASSERT(imm.is_uint16());
  EmitUint8(imm.value() & 0xFF);
  EmitUint8((imm.value() >> 8) & 0xFF);
  EmitUint8(0x00);
}

void Assembler::nop(int size) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  // There are nops up to size 15, but for now just provide up to size 8.
  ASSERT(0 < size && size <= MAX_NOP_SIZE);
  switch (size) {
  case 1:
    EmitUint8(0x90);
    break;
  case 2:
    EmitUint8(0x66);
    EmitUint8(0x90);
    break;
  case 3:
    EmitUint8(0x0F);
    EmitUint8(0x1F);
    EmitUint8(0x00);
    break;
  case 4:
    EmitUint8(0x0F);
    EmitUint8(0x1F);
    EmitUint8(0x40);
    EmitUint8(0x00);
    break;
  case 5:
    EmitUint8(0x0F);
    EmitUint8(0x1F);
    EmitUint8(0x44);
    EmitUint8(0x00);
    EmitUint8(0x00);
    break;
  case 6:
    EmitUint8(0x66);
    EmitUint8(0x0F);
    EmitUint8(0x1F);
    EmitUint8(0x44);
    EmitUint8(0x00);
    EmitUint8(0x00);
    break;
  case 7:
    EmitUint8(0x0F);
    EmitUint8(0x1F);
    EmitUint8(0x80);
    EmitUint8(0x00);
    EmitUint8(0x00);
    EmitUint8(0x00);
    EmitUint8(0x00);
    break;
  case 8:
    EmitUint8(0x0F);
    EmitUint8(0x1F);
    EmitUint8(0x84);
    EmitUint8(0x00);
    EmitUint8(0x00);
    EmitUint8(0x00);
    EmitUint8(0x00);
    EmitUint8(0x00);
    break;
  default:
    UNIMPLEMENTED();
  }
}

void Assembler::j(Condition condition, Label *label, bool near) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (label->IsBound()) {
    static const int kShortSize = 2;
    static const int kLongSize = 6;
    intptr_t offset = label->Position() - buffer_.Size();
    ASSERT(offset <= 0);
    if (Utils::IsInt(8, offset - kShortSize)) {
      EmitUint8(0x70 + condition);
      EmitUint8((offset - kShortSize) & 0xFF);
    } else {
      EmitUint8(0x0F);
      EmitUint8(0x80 + condition);
      EmitInt32(offset - kLongSize);
    }
  } else if (near) {
    EmitUint8(0x70 + condition);
    EmitNearLabelLink(label);
  } else {
    EmitUint8(0x0F);
    EmitUint8(0x80 + condition);
    EmitLabelLink(label);
  }
}

void Assembler::jmp(Label *label, bool near) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  if (label->IsBound()) {
    static const int kShortSize = 2;
    static const int kLongSize = 5;
    intptr_t offset = label->Position() - buffer_.Size();
    ASSERT(offset <= 0);
    if (Utils::IsInt(8, offset - kShortSize)) {
      EmitUint8(0xEB);
      EmitUint8((offset - kShortSize) & 0xFF);
    } else {
      EmitUint8(0xE9);
      EmitInt32(offset - kLongSize);
    }
  } else if (near) {
    EmitUint8(0xEB);
    EmitNearLabelLink(label);
  } else {
    EmitUint8(0xE9);
    EmitLabelLink(label);
  }
}

void Assembler::jmp(const ExternalLabel *label) {
  { // Encode movq(TMP, Immediate(label->address())), but always as imm64.
    AssemblerBuffer::EnsureCapacity ensured(&buffer_);
    EmitRegisterREX(TMP, REX_W);
    EmitUint8(0xB8 | (TMP & 7));
    EmitInt64(label->address());
  }
  jmp(TMP);
}

void Assembler::CompareRegisters(Register a, Register b) { cmpq(a, b); }

void Assembler::MoveRegister(Register to, Register from) {
  if (to != from) {
    movq(to, from);
  }
}

void Assembler::PushRegister(Register r) { pushq(r); }

void Assembler::PopRegister(Register r) { popq(r); }

void Assembler::Drop(intptr_t stack_elements, Register tmp) {
  ASSERT(stack_elements >= 0);
  if (stack_elements <= 4) {
    for (intptr_t i = 0; i < stack_elements; i++) {
      popq(tmp);
    }
    return;
  }
  addq(RSP, Immediate(stack_elements * kWordSize));
}

void Assembler::Bind(Label *label) {
  intptr_t bound = buffer_.Size();
  ASSERT(!label->IsBound()); // Labels can only be bound once.
  while (label->IsLinked()) {
    intptr_t position = label->LinkPosition();
    intptr_t next = buffer_.Load<int32_t>(position);
    buffer_.Store<int32_t>(position, bound - (position + 4));
    label->position_ = next;
  }
  while (label->HasNear()) {
    intptr_t position = label->NearPosition();
    intptr_t offset = bound - (position + 1);
    ASSERT(Utils::IsInt(8, offset));
    buffer_.Store<int8_t>(position, offset);
  }
  label->BindTo(bound);
}

const int kMinimumAlignment = 16;

void Assembler::ReserveAlignedFrameSpace(intptr_t frame_space) {
  // Reserve space for arguments and align frame before entering
  // the C++ world.
  if (frame_space != 0) {
    subq(RSP, Immediate(frame_space));
  }
  // TODO(max): Per-platform activation frame alignment
  andq(RSP, Immediate(~(kMinimumAlignment - 1)));
}

void Assembler::Align(int alignment, intptr_t offset) {
  ASSERT(Utils::IsPowerOfTwo(alignment));
  intptr_t pos = offset + buffer_.GetPosition();
  int mod = pos & (alignment - 1);
  if (mod == 0) {
    return;
  }
  intptr_t bytes_needed = alignment - mod;
  while (bytes_needed > MAX_NOP_SIZE) {
    nop(MAX_NOP_SIZE);
    bytes_needed -= MAX_NOP_SIZE;
  }
  if (bytes_needed) {
    nop(bytes_needed);
  }
  ASSERT(((offset + buffer_.GetPosition()) & (alignment - 1)) == 0);
}

void Assembler::EmitOperand(int rm, const Operand &operand) {
  ASSERT(rm >= 0 && rm < 8);
  const intptr_t length = operand.length_;
  ASSERT(length > 0);
  // Emit the ModRM byte updated with the given RM value.
  ASSERT((operand.encoding_[0] & 0x38) == 0);
  EmitUint8(operand.encoding_[0] + (rm << 3));
  // Emit the rest of the encoded operand.
  for (intptr_t i = 1; i < length; i++) {
    EmitUint8(operand.encoding_[i]);
  }
}

void Assembler::EmitRegisterOperand(int rm, int reg) {
  Operand operand;
  operand.SetModRM(3, static_cast<Register>(reg));
  EmitOperand(rm, operand);
}

void Assembler::EmitImmediate(const Immediate &imm) {
  if (imm.is_int32()) {
    EmitInt32(static_cast<int32_t>(imm.value()));
  } else {
    EmitInt64(imm.value());
  }
}

void Assembler::EmitSignExtendedInt8(int rm, const Operand &operand,
                                     const Immediate &immediate) {
  EmitUint8(0x83);
  EmitOperand(rm, operand);
  EmitUint8(immediate.value() & 0xFF);
}

void Assembler::EmitComplex(int rm, const Operand &operand,
                            const Immediate &immediate) {
  ASSERT(rm >= 0 && rm < 8);
  ASSERT(immediate.is_int32());
  if (immediate.is_int8()) {
    EmitSignExtendedInt8(rm, operand, immediate);
  } else if (operand.IsRegister(RAX)) {
    // Use short form if the destination is rax.
    EmitUint8(0x05 + (rm << 3));
    EmitImmediate(immediate);
  } else {
    EmitUint8(0x81);
    EmitOperand(rm, operand);
    EmitImmediate(immediate);
  }
}

void Assembler::EmitLabel(Label *label, intptr_t instruction_size) {
  if (label->IsBound()) {
    intptr_t offset = label->Position() - buffer_.Size();
    ASSERT(offset <= 0);
    EmitInt32(offset - instruction_size);
  } else {
    EmitLabelLink(label);
  }
}

void Assembler::EmitLabelLink(Label *label) {
  ASSERT(!label->IsBound());
  intptr_t position = buffer_.Size();
  EmitInt32(label->position_);
  label->LinkTo(position);
}

void Assembler::EmitNearLabelLink(Label *label) {
  ASSERT(!label->IsBound());
  intptr_t position = buffer_.Size();
  EmitUint8(0);
  label->NearLinkTo(position);
}

void Assembler::EmitGenericShift(bool wide, int rm, Register reg,
                                 const Immediate &imm) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  ASSERT(imm.is_int8());
  if (wide) {
    EmitRegisterREX(reg, REX_W);
  } else {
    EmitRegisterREX(reg, REX_NONE);
  }
  if (imm.value() == 1) {
    EmitUint8(0xD1);
    EmitOperand(rm, Operand(reg));
  } else {
    EmitUint8(0xC1);
    EmitOperand(rm, Operand(reg));
    EmitUint8(imm.value() & 0xFF);
  }
}

void Assembler::EmitGenericShift(bool wide, int rm, Register operand,
                                 Register shifter) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  ASSERT(shifter == RCX);
  EmitRegisterREX(operand, wide ? REX_W : REX_NONE);
  EmitUint8(0xD3);
  EmitOperand(rm, Operand(operand));
}
