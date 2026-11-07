#pragma once

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
 #include <xmmintrin.h>
 #define VM_HAS_SSE_CSR 1
#else
 #define VM_HAS_SSE_CSR 0
#endif

namespace vm
{
    // Enables flush-to-zero and denormals-are-zero for the current scope, then
    // restores the previous state. Put one at the top of every audio callback.
    class ScopedDenormalGuard
    {
    public:
        ScopedDenormalGuard() noexcept
        {
           #if VM_HAS_SSE_CSR
            saved = _mm_getcsr();
            _mm_setcsr (saved | 0x8040u); // FTZ (bit 15) | DAZ (bit 6)
           #endif
        }

        ~ScopedDenormalGuard() noexcept
        {
           #if VM_HAS_SSE_CSR
            _mm_setcsr (saved);
           #endif
        }

        ScopedDenormalGuard (const ScopedDenormalGuard&) = delete;
        ScopedDenormalGuard& operator= (const ScopedDenormalGuard&) = delete;

    private:
       #if VM_HAS_SSE_CSR
        unsigned int saved = 0;
       #endif
    };
}
