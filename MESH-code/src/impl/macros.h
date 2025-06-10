//
// Created by vm on 25.10.6.
//

#ifndef MESH_CODE_MACROS_H
#define MESH_CODE_MACROS_H

// ✨ macro magic ✨
#define FE_0(WHAT)
#define FE_1(WHAT, X) WHAT(X)
#define FE_2(WHAT, X, ...) WHAT(X)FE_1(WHAT, __VA_ARGS__)
#define FE_3(WHAT, X, ...) WHAT(X)FE_2(WHAT, __VA_ARGS__)
#define FE_4(WHAT, X, ...) WHAT(X)FE_3(WHAT, __VA_ARGS__)
#define FE_5(WHAT, X, ...) WHAT(X)FE_4(WHAT, __VA_ARGS__)

#define GET_MACRO(_0, _1, _2, _3, _4, _5, NAME, ...) NAME
#define FOR_EACH(action, ...) \
  GET_MACRO(_0,__VA_ARGS__,FE_5,FE_4,FE_3,FE_2,FE_1,FE_0)(action,__VA_ARGS__)

#endif //MESH_CODE_MACROS_H
