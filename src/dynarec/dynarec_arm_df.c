#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <pthread.h>
#include <errno.h>

#include "debug.h"
#include "box86context.h"
#include "dynarec.h"
#include "emu/x86emu_private.h"
#include "emu/x86run_private.h"
#include "x86run.h"
#include "x86emu.h"
#include "box86stack.h"
#include "callback.h"
#include "emu/x86run_private.h"
#include "x86trace.h"
#include "dynablock.h"
#include "dynablock_private.h"
#include "dynarec_arm.h"
#include "dynarec_arm_private.h"
#include "arm_printer.h"

#include "dynarec_arm_helper.h"


uintptr_t dynarecDF(dynarec_arm_t* dyn, uintptr_t addr, int ninst, int* ok, int* need_epilog)
{
    uintptr_t ip = addr-1;
    uint8_t nextop = F8;
    uint8_t u8;
    uint32_t u32;
    int32_t i32;
    int16_t i16;
    uint16_t u16;
    uint8_t gd, ed;
    uint8_t wback, wb1, wb2;
    int v1, v2, v3;
    int s0, s1;
    int d0;
    int fixedaddress;

    switch(nextop) {
        case 0xE0:
            INST_NAME("FNSTSW AX");
            LDR_IMM9(x2, xEmu, offsetof(x86emu_t, top));
            LDRH_IMM8(x1, xEmu, offsetof(x86emu_t, sw));
            AND_IMM8(x2, x2, 7);
            BFI(x1, x2, 11, 3); // inject top
            BFI(xEAX, x1, 0, 16);
            break;
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
            INST_NAME("FUCOMIP ST0, STx");
            v1 = x87_get_st(dyn, ninst, x1, x2, 0);
            v2 = x87_get_st(dyn, ninst, x1, x2, nextop&7);
            VCMP_F64(v1, v2);
            FCOMI(x1, x2);
            x87_do_pop(dyn, ninst);
            break;
        case 0xF0:
        case 0xF1:
        case 0xF2:
        case 0xF3:
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
            INST_NAME("FCOMIP ST0, STx");
            v1 = x87_get_st(dyn, ninst, x1, x2, 0);
            v2 = x87_get_st(dyn, ninst, x1, x2, nextop&7);
            VCMP_F64(v1, v2);
            FCOMI(x1, x2);
            x87_do_pop(dyn, ninst);
            break;

        case 0xC0:
        case 0xC1:
        case 0xC2:
        case 0xC3:
        case 0xC4:
        case 0xC5:
        case 0xC6:
        case 0xC7:
        case 0xC8:
        case 0xC9:
        case 0xCA:
        case 0xCB:
        case 0xCC:
        case 0xCD:
        case 0xCE:
        case 0xCF:
        case 0xD0:
        case 0xD1:
        case 0xD2:
        case 0xD3:
        case 0xD4:
        case 0xD5:
        case 0xD6:
        case 0xD7:
        case 0xD8:
        case 0xD9:
        case 0xDA:
        case 0xDB:
        case 0xDC:
        case 0xDD:
        case 0xDE:
        case 0xDF:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE4:
        case 0xE5:
        case 0xE6:
        case 0xE7:
        case 0xF8:
        case 0xF9:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0xFD:
        case 0xFE:
        case 0xFF:
            *ok = 0;
            DEFAULT;
            break;

        default:
            switch((nextop>>3)&7) {
                case 0:
                    INST_NAME("FILD ST0, Ew");
                    v1 = x87_do_push(dyn, ninst);
                    addr = geted(dyn, addr, ninst, nextop, &wback, x3, &fixedaddress);
                    LDRSH_IMM8(x1, wback, 0);
                    s0 = x87_get_scratch_single(0);
                    VMOVtoV(s0, x1);
                    VCVT_F64_S32(v1, s0);
                    break;
                case 1:
                    INST_NAME("FISTTP Ew, ST0");
                    v1 = x87_get_st(dyn, ninst, x1, x2, 0);
                    u8 = x87_setround(dyn, ninst, x1, x2, x3);
                    addr = geted(dyn, addr, ninst, nextop, &wback, x2, &fixedaddress);
                    ed = x1;
                    s0 = x87_get_scratch_single(0);
                    VCVT_S32_F64(s0, v1);
                    VMOVfrV(ed, s0);
                    MOVW(x12, 0x7fff);
                    CMPS_REG_LSL_IMM8(ed, x12, 0);
                    i32 = GETMARK2-(dyn->arm_size+8);
                    Bcond(cGE, i32);
                    MOV32(x12, 0xffff8000);
                    CMPS_REG_LSL_IMM8(ed, x12, 0);
                    i32 = GETMARK-(dyn->arm_size+8);
                    Bcond(cGE, i32);
                    MARK2;
                    MOV32(ed, 0xffff8000);
                    MARK;
                    // STRH doesn't seems to correctly store large negative value (and probably large int value neither)
                    STRH_IMM8(ed, wback, 0);
                    x87_do_pop(dyn, ninst);
                    x87_restoreround(dyn, ninst, u8);
                    break;
                case 2:
                    INST_NAME("FIST Ew, ST0");
                    v1 = x87_get_st(dyn, ninst, x1, x2, 0);
                    u8 = x87_setround(dyn, ninst, x1, x2, x3);
                    addr = geted(dyn, addr, ninst, nextop, &wback, x2, &fixedaddress);
                    ed = x1;
                    s0 = x87_get_scratch_single(0);
                    VCVTR_S32_F64(s0, v1);
                    VMOVfrV(ed, s0);
                    MOVW(x12, 0x7fff);
                    CMPS_REG_LSL_IMM8(ed, x12, 0);
                    i32 = GETMARK2-(dyn->arm_size+8);
                    Bcond(cGE, i32);
                    MOV32(x12, 0xffff8000);
                    CMPS_REG_LSL_IMM8(ed, x12, 0);
                    i32 = GETMARK-(dyn->arm_size+8);
                    Bcond(cGE, i32);
                    MARK2;
                    MOV32(ed, 0xffff8000);
                    MARK;
                    // STRH doesn't seems to correctly store large negative value (and probably large int value neither)
                    STRH_IMM8(ed, wback, 0);
                    x87_restoreround(dyn, ninst, u8);
                    break;
                case 3:
                    INST_NAME("FISTP Ew, ST0");
                    v1 = x87_get_st(dyn, ninst, x1, x2, 0);
                    u8 = x87_setround(dyn, ninst, x1, x2, x3);
                    addr = geted(dyn, addr, ninst, nextop, &wback, x2, &fixedaddress);
                    ed = x1;
                    s0 = x87_get_scratch_single(0);
                    VCVTR_S32_F64(s0, v1);
                    VMOVfrV(ed, s0);
                    MOVW(x12, 0x7fff);
                    CMPS_REG_LSL_IMM8(ed, x12, 0);
                    i32 = GETMARK2-(dyn->arm_size+8);
                    Bcond(cGE, i32);
                    MOV32(x12, 0xffff8000);
                    CMPS_REG_LSL_IMM8(ed, x12, 0);
                    i32 = GETMARK-(dyn->arm_size+8);
                    Bcond(cGE, i32);
                    MARK2;
                    MOV32(ed, 0xffff8000);
                    MARK;
                    // STRH doesn't seems to correctly store large negative value (and probably large int value neither)
                    STRH_IMM8(ed, wback, 0);
                    x87_do_pop(dyn, ninst);
                    x87_restoreround(dyn, ninst, u8);
                    break;
                default:
                    *ok = 0;
                    DEFAULT;
            }
    }
    return addr;
}

