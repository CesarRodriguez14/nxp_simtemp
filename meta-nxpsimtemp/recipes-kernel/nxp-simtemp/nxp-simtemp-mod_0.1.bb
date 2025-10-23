SUMMARY = "NXP-SIMTEMP MODULE RECIPE"
DESCRIPTION = "${SUMMARY}"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module

SRC_URI = "file://Makefile \
           file://gaussian_random.c \
           file://gaussian_random.h \
           file://hrtimer_example.c \
           file://nxp_simtemp.c \
           file://simtemp.h \
           file://COPYING \
          "

S = "${WORKDIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.

RPROVIDES_${PN} += "kernel-module-nxp-simtemp-drv"
