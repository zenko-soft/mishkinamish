/*
 * Intel IPP stub header
 * Provides minimal type definitions and function declarations for compilation
 */
#ifndef __IPP_H__
#define __IPP_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef int IppStatus;
typedef float Ipp32f;
typedef uint8_t Ipp8u;
typedef int IppEnum;
typedef double Ipp64f;
typedef int IppHintAlgorithm;

#define ippStsNoErr 0
#define ippAlgHintNone 0
#define ippAlgHintAccurate 0
#define IPP_MAX_8U 255
#define IPP_FFT_NODIV_BY_ANY 0

#define ippsMalloc_8u(size) ((Ipp8u*)malloc(size))
#define ippFree(ptr) free(ptr)

static inline void ippInit() {}

typedef struct {
    int size;
} IppsDFTSpec_R_32f;

typedef struct {
    int size;
} IppsDFTSpec_C_32fc;

typedef struct {
    int size;
} IppsDCTFwdSpec_32f;

typedef struct {
    int size;
} IppsDCTInvSpec_32f;

static inline IppStatus ippsDFTGetSize_C_32fc(int len, int, IppHintAlgorithm, int* pSpecSize, int* pSpecBufferSize, int* pBufferSize) { 
    *pSpecSize = 64; 
    *pSpecBufferSize = 0; 
    *pBufferSize = 64; 
    return ippStsNoErr; 
}

static inline IppStatus ippsDFTGetSize_C_32f(int, int*, int*, int*) { return ippStsNoErr; }
static inline IppStatus ippsDFTInit_R_32f(int len, int flag, int hint, IppsDFTSpec_R_32f* pDFTSpec, Ipp8u* pInitBuf) { return ippStsNoErr; }
static inline IppStatus ippsDFTFwd_RToCCS_32f(const Ipp32f*, Ipp32f*, IppsDFTSpec_R_32f*, Ipp8u*) { return ippStsNoErr; }
static inline IppStatus ippsDFTInv_CCSToR_32f(const Ipp32f*, Ipp32f*, IppsDFTSpec_R_32f*, Ipp8u*) { return ippStsNoErr; }

static inline IppStatus ippsDCTFwdGetSize_32f(int len, int hint, int* pSpecSize, int* pInitBufSize, int* pBufferSize) { 
    *pSpecSize = 64; 
    *pInitBufSize = 0; 
    *pBufferSize = 64;
    return ippStsNoErr; 
}

static inline IppStatus ippsDCTFwdInit_32f(IppsDCTFwdSpec_32f** ppDCTSpec, int len, int hint, Ipp8u* pSpec, Ipp8u* pBuffer) { return ippStsNoErr; }
static inline IppStatus ippsDCTFwd_32f_I(Ipp32f*, IppsDCTFwdSpec_32f*, Ipp8u*) { return ippStsNoErr; }

static inline IppStatus ippsDCTInvGetSize_32f(int, int, IppHintAlgorithm, int*, int*) { return ippStsNoErr; }
static inline IppStatus ippsDCTInvInit_32f(int, int, IppHintAlgorithm, IppsDCTInvSpec_32f*) { return ippStsNoErr; }
static inline IppStatus ippsDCTInv_32f_I(Ipp32f*, IppsDCTInvSpec_32f*, Ipp8u*) { return ippStsNoErr; }

static inline IppStatus ippsZero_8u(Ipp8u*, int) { return ippStsNoErr; }
static inline IppStatus ippsZero_32f(Ipp32f*, int) { return ippStsNoErr; }
static inline IppStatus ippsCopy_32f(Ipp32f*, const Ipp32f*, int) { return ippStsNoErr; }
static inline IppStatus ippsMul_32f_I(Ipp32f, Ipp32f*, int) { return ippStsNoErr; }
static inline IppStatus ippsAdd_32f_I(const Ipp32f*, Ipp32f*, int) { return ippStsNoErr; }

#endif
