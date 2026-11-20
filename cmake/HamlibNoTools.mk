# Stands in for Hamlib's tests/ and doc/ Makefiles in the Mac build
# (BuildHamlib.cmake): Ragtime links only libhamlib, and the utilities in
# tests/ (rigctl, rigctld...) don't compile against the macOS 15 SDK's
# getopt.
all install clean:
.PHONY: all install clean
