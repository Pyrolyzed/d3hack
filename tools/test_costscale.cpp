// d3hack-custom: offline test for the paragon XP cost scaler in d3/hooks/util.hpp.
//
// The scaler cannot be exercised in game without a 20000-paragon hero, so the arithmetic is
// verified here instead. The expressions below are copied VERBATIM from DeliverInner -- if you
// change one there, change it here and re-run:
//
//     docker exec -w /work d3hack-build bash -lc \
//       "g++ -O2 -std=c++20 -o /tmp/tcs tools/test_costscale.cpp && /tmp/tcs"
//
// This proves the maths, NOT that the hook is installed. Installation is proved by the
// costscale= field in the boot line and the cost= field in the ledger.
#include <cstdio>
#include <cstdint>

using u64 = uint64_t;
using s32 = int32_t;

static u64 s_uCostRem = 0;
static u64 s_uCostPct = 100;

// --- verbatim from DeliverInner, with uWant as the only input -------------------------------
static unsigned __int128 Scale(unsigned __int128 uWant, bool bPara, s32 nLevel, int nFrom, int nPer) {
    s_uCostPct = 100ull;
    if (bPara && nPer > 0 && nFrom >= 0 && nLevel > nFrom) {
        const u64 uAbove = static_cast<u64>(nLevel - nFrom);
        const u64 uDen   = 100000ull;
        const u64 uNum   = uDen + uAbove * static_cast<u64>(nPer);
        s_uCostPct       = (uNum * 100ull) / uDen;
        if (uWant > (static_cast<unsigned __int128>(1) << 100)) {
            uWant = (uWant / uNum) * uDen;
        } else {
            const unsigned __int128 t = uWant * uDen + s_uCostRem;
            uWant      = t / uNum;
            s_uCostRem = static_cast<u64>(t % uNum);
        }
    }
    return uWant;
}
// --------------------------------------------------------------------------------------------

static int g_fail = 0;

static void Check(bool ok, const char *what, unsigned long long got, unsigned long long want) {
    if (!ok) {
        std::printf("  FAIL  %-52s got %llu want %llu\n", what, got, want);
        ++g_fail;
    } else {
        std::printf("  ok    %-52s %llu\n", what, got);
    }
}

int main() {
    const int kFrom = 20000;
    const int kPer  = 100;   // +100% per 1000 levels, the shipped value

    std::printf("cost factor (ParagonXpCostFrom=%d ParagonXpCostPer1000=%d)\n", kFrom, kPer);

    // 1. Off below and exactly at the start level -- no cliff, no off-by-one.
    s_uCostRem = 0;
    Scale(1000, true, 19999, kFrom, kPer);
    Check(s_uCostPct == 100, "paragon 19999 -> 1.00x", s_uCostPct, 100);
    Scale(1000, true, 20000, kFrom, kPer);
    Check(s_uCostPct == 100, "paragon 20000 -> 1.00x (boundary, not scaled)", s_uCostPct, 100);

    // 2. Turns on smoothly one level in, rather than jumping.
    Scale(1000, true, 20001, kFrom, kPer);
    Check(s_uCostPct == 100, "paragon 20001 -> 1.001x, reported as 100%", s_uCostPct, 100);

    // 3. The documented points on the curve.
    Scale(1000, true, 21000, kFrom, kPer);
    Check(s_uCostPct == 200, "paragon 21000 -> 2.00x", s_uCostPct, 200);
    Scale(1000, true, 30000, kFrom, kPer);
    Check(s_uCostPct == 1100, "paragon 30000 -> 11.00x", s_uCostPct, 1100);

    // 4. Non-paragon characters are never touched, whatever the level.
    s_uCostRem       = 0;
    unsigned __int128 v = Scale(12345, false, 30000, kFrom, kPer);
    Check(static_cast<u64>(v) == 12345 && s_uCostPct == 100, "non-paragon hero untouched", static_cast<u64>(v), 12345);

    // 5. Halving at 21000: a stream of grants delivers half, and NOTHING is lost.
    s_uCostRem = 0;
    unsigned __int128 in = 0, out = 0;
    for (int i = 0; i < 10000; ++i) {
        in  += 1000;
        out += Scale(1000, true, 21000, kFrom, kPer);
    }
    Check(static_cast<u64>(out) == 5000000, "10000 grants of 1000 at 2x -> exactly half", static_cast<u64>(out), 5000000);

    // 6. THE CRATE TEST. 1-XP grants must not floor to zero and disappear: 190 of them
    //    happened in one real 80-minute session. At 2x, 2000 of them must yield 1000.
    s_uCostRem = 0;
    out = 0;
    for (int i = 0; i < 2000; ++i)
        out += Scale(1, true, 21000, kFrom, kPer);
    Check(static_cast<u64>(out) == 1000, "2000 grants of 1 XP at 2x -> 1000, none lost", static_cast<u64>(out), 1000);

    // 7. Same at 11x, where a single 1-XP grant rounds to zero on its own.
    s_uCostRem = 0;
    out = 0;
    for (int i = 0; i < 11000; ++i)
        out += Scale(1, true, 30000, kFrom, kPer);
    Check(static_cast<u64>(out) == 1000, "11000 grants of 1 XP at 11x -> 1000, none lost", static_cast<u64>(out), 1000);

    // 8. Extreme paragon: the shipped cap is 2e9. Must not wrap, must still divide.
    s_uCostRem = 0;
    v = Scale(1000000000000ull, true, 2000000000, kFrom, kPer);
    const bool bBig = (s_uCostPct > 100) && (v < 1000000000000ull);
    Check(bBig, "paragon 2e9 divides without wrapping", static_cast<u64>(v), 0);

    // 9. The >2^100 branch: huge uWant must not overflow the 128-bit intermediate.
    s_uCostRem                = 0;
    unsigned __int128 huge    = static_cast<unsigned __int128>(1) << 110;
    unsigned __int128 scaled  = Scale(huge, true, 21000, kFrom, kPer);
    Check(scaled < huge && scaled >(huge >> 2), "2^110 uses the divide-first branch, no wrap",
          static_cast<u64>(scaled >> 64), static_cast<u64>((huge / 2) >> 64));

    // 10. Remainder stays bounded by the divisor -- it is not a second carry.
    s_uCostRem = 0;
    for (int i = 0; i < 100000; ++i)
        Scale(7, true, 30000, kFrom, kPer);
    Check(s_uCostRem < 1100000ull, "remainder stays below the divisor after 100k grants", s_uCostRem, 1100000);

    std::printf("\n%s\n", g_fail == 0 ? "ALL PASS" : "FAILURES ABOVE");
    return g_fail != 0;
}
