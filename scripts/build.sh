#!/bin/bash

# Enhanced build script with comprehensive error handling
set -e  # Exit on any error
set -u  # Exit on undefined variables

# Configuration
BUILD_DIR="../kernel"
CLI_DIR="../user/cli"
CLEAN_ONLY=false


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

# Help function
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  -c, --clean      Only clean, don't build"
    echo "  -h, --help       Show this help message"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -c|--clean)
            CLEAN_ONLY=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done
# Function to run make command with error handling
run_make() {
    local dir="$1"
    local component="$2"
    local target="$3"
    
    log_info "Running 'make $target' in $component directory..."

    if output=$(make -C "$dir" "$target" 2>&1); then
        log_success "Make $target completed successfully in $component directory"
        return 0
    else
        log_error "Make $target failed in $component directory"
        log_error "$output"
        return 1
    fi
}

# Function to build a component
build_component() {
    local dir="$1"
    local component="$2"
    
    log_info "Building $component component..."
    
    # Clean first
    if ! run_make "$dir" "$component" "clean"; then
        return 1
    fi
    
    # Build if not clean-only mode
    if [[ "$CLEAN_ONLY" != "true" ]]; then
        if ! run_make "$dir" "$component" "all"; then
            return 1
        fi
    fi
    
    return 0
}

# Main execution
main() {
    log_info "Starting build process..."
    
    # Build kernel component
    if ! build_component "$BUILD_DIR" "kernel"; then
        return 1
    fi

    # Build user component
    if ! build_component "$CLI_DIR" "user"; then
        return 1
    fi
    
    # Summary
    log_success "Build process completed successfully!"
    
    exit 0
}

# Run main function
main "$@"
