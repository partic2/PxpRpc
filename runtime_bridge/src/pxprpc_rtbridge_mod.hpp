
#pragma once

#include "pxprpc_rtbridge_base.hpp"

#if PXPRPC_RTBRIDGE_HAVE_JNI_H
#include "../java/jnibridge.cpp"
#endif

typedef void (*pxprpc_rtbirdge_mod_init_func)();

static pxprpc_rtbirdge_mod_init_func pxprpc_rtbridge_mod_init[] ={
    &pxprpc_rtbirdge_base::init,
    nullptr
};

