FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://0001-nxp-simtemp-driver.patch"
SRC_URI += "file://0002-update-driver-files.patch"
SRC_URI += "file://0003-update-sampling-us-default.patch"
SRC_URI += "file://0004-update-platform-driver-locking-model.patch"
SRC_URI += "file://0005-add-kfifo-out-handling.patch"
SRC_URI += "file://nxp-simtemp-kernel.cfg"
