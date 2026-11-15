// Passbands: where Data2G's signals sit in the radio's audio, for the
// Ear's green bands. Data2G centres every mode on FCENTER, 1500 Hz
// (Data2G's native/core/generated/config.hpp at v0.3.0); its wide modes
// fill 2300 Hz and its narrow ones 500 Hz, so they span 350 to 2650 Hz
// and 1250 to 1750 Hz. Its narrowest, the 200 Hz modes, sit in the
// middle of the 500 Hz ones.
#ifndef RAGTIME_EAR_PASSBANDS_H
#define RAGTIME_EAR_PASSBANDS_H

namespace ragtime
{
namespace ear
{

constexpr double DATA2G_CENTRE_HZ = 1500.0;
constexpr double DATA2G_WIDE_HZ = 2300.0;
constexpr double DATA2G_NARROW_HZ = 500.0;
constexpr double DATA2G_NARROWEST_HZ = 200.0;

struct Passband
{
    double lowHz;
    double highHz;
};

constexpr Passband data2gPassband(bool narrow)
{
    const double half = (narrow ? DATA2G_NARROW_HZ : DATA2G_WIDE_HZ) / 2;
    return {DATA2G_CENTRE_HZ - half, DATA2G_CENTRE_HZ + half};
}

// What of Data2G's passband a receive filter must leave whole: all of
// the wide one; with the 500 Hz switch on, only the 200 Hz in the
// middle, so a narrow filter about the 500 Hz modes is no cut (Jeff,
// 2026-11-15: "narrow filters should not be rejected if 500 Hz mode is
// selected").
constexpr Passband data2gKept(bool narrow)
{
    const double half = (narrow ? DATA2G_NARROWEST_HZ : DATA2G_WIDE_HZ) / 2;
    return {DATA2G_CENTRE_HZ - half, DATA2G_CENTRE_HZ + half};
}

} // namespace ear
} // namespace ragtime

#endif
