#!/bin/sh

# Enhanced build script with comprehensive error handling
set -e  # Exit on any error
set -u  # Exit on undefined variables

# Configuration
KERNEL_DIR="/lib/modules/5.4.191/kernel/drivers/char/nxp_simtemp_driver"
MODULE_NAME="nxp_simtemp_drv"


sampling_rate_ms=100
mode=d
threshold_mC=20000

# Logging functions
log_info() {
    echo "[INFO] $1"
}

log_success() {
    echo "[SUCCESS] $1"
}

log_error() {
    echo "[ERROR] $1"
}

main() {
    log_info "Inserting kernel module"

    if ! lsmod | grep -q "^$MODULE_NAME";then
        log_info "Kernel module is not loaded, inserting..."
        if ! insmod $KERNEL_DIR/$MODULE_NAME.ko; then
            log_error "Failed to insert kernel module"
            return 1
        fi
    else
        log_info "Kernel module is already loaded"
    fi

    log_info "Running cli (will timeout after 1 minute)"

    set +e
    timeout 10 cli_nxp_simtemp -s$sampling_rate_ms -m$mode -t$threshold_mC
    local timeout_exit_code=$?
    set -e
    
    if [ $timeout_exit_code -eq 124 ]; then
        log_info "CLI timed out after 1 minute (expected)"
    elif [ $timeout_exit_code -ne 0 ]; then
        log_error "CLI exited with error code: $timeout_exit_code"
    else
        log_info "CLI completed normally"
    fi

    log_info "Removing kernel module"

    if ! rmmod nxp_simtemp_drv; then
        log_error "Failed to remove kernel module"
        return 1
    fi
    
    # Summary
    log_success "Demo run completed successfully!"
    
    exit 0
}

# Run main function
main "$@"