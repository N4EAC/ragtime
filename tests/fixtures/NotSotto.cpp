// A loadable library without sotto's API, to exercise symbol rejection
// without depending on an OS-specific system-library name or location.
extern "C" int ragtime_not_sotto()
{
    return 0;
}
