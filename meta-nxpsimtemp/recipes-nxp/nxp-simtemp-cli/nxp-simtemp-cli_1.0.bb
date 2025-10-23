SUMMARY = "My Test Reader Application"
SECTION = "utils"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS_prepend := "${THISDIR}:"

SRC_URI = "file://lib.h \
	   file://lib.cpp \
	   file://main.cpp \
	   file://run_demo_sc203e.sh \
           file://Makefile"

S = "${WORKDIR}"

# Link against the library
TARGET_CC_ARCH += "${LDFLAGS}"

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${bindir}
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 cli_nxp_simtemp ${D}${bindir}
    install -m 0755 run_demo_sc203e.sh ${D}${sysconfdir}/init.d
}
