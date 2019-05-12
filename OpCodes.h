#ifndef OPCODES
#define OPCODES

enum EOpCode
{
    addi,
    addic,
    addicR,
    addis,
    b,
    bc,
    bcctr,
    bclr,
    cmp,
    cmpi,
    cmpli,
    cror,
    crxor,
    extsb,
    fabs_,
    fadd,
    fadds,
    fcmpo,
    fctiwz,
    fdiv,
    fdivs,
    fmadd,
    fmadds,
    fmr,
    fmul,
    fmuls,
    fmsub,
    fmsubs,
    fneg,
    fnmsub,
    fnmsubs,
    frsp,
    frsqrte,
    fsel,
    fsub,
    fsubs,
    lbz,
    lfd,
    lfs,
    lfsu,
    lhz,
    lmw,
    lwz,
    lwzu,
    mfspr,
    mtspr,
    neg,
    or_,
    ori,
    oris,
    psq_l,
    psq_st,
    psq_stx,
    rlwimi,
    rlwinm,
    slw,
    srw,
    stb,
    stfd,
    stfs,
    stfsu,
    sth,
    stmw,
    stw,
    stwu,
    subf,
    subfic,
    xoris,
    UnknownOpCode
};

#endif // OPCODES

