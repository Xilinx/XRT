#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

# Script to download and install amdxdna archives from VTD GitHub repository
# Usage: install-amdxdna-archive.sh <platform-name> <xrt-version> [install-dir]

set -e

SCRIPT_NAME=$(basename "$0")
VTD_REPO_BASE="https://raw.githubusercontent.com/Xilinx/VTD/main/archive"

usage() {
    echo "Usage: ${SCRIPT_NAME} <archive-name> <xrt-version> [install-dir]"
    echo ""
    echo "Download and install amdxdna archive from VTD GitHub repository."
    echo ""
    echo "Arguments:"
    echo "  archive-name     Archive name (xrt_smi_phx.a, xrt_smi_strx.a, xrt_smi_npu3.a, or xrt_smi_ve2.a)"
    echo "  xrt-version      XRT version in format major.minor.patch (e.g., 2.21.0)"
    echo "  install-dir      Optional: Custom installation directory"
    echo "                   Default: \$HOME/.local/share/xrt/<xrt-version>/amdxdna/bins"
    echo ""
    echo "Examples:"
    echo "  ${SCRIPT_NAME} xrt_smi_phx.a 2.21.0"
    echo "  ${SCRIPT_NAME} xrt_smi_strx.a 2.21.0 /custom/path"
    echo ""
    echo "Available archives: xrt_smi_phx.a, xrt_smi_strx.a, xrt_smi_npu3.a, xrt_smi_ve2.a"
    echo ""
    exit 1
}

# Check for required arguments
if [ $# -lt 2 ]; then
    usage
fi

ARCHIVE_NAME="$1"
XRT_VERSION="$2"
INSTALL_DIR="$3"

# Extract platform subdirectory from archive name and validate
case "${ARCHIVE_NAME}" in
    xrt_smi_phx.a)
        PLATFORM_DIR="phx"
        ;;
    xrt_smi_strx.a)
        PLATFORM_DIR="strx"
        ;;
    xrt_smi_npu3.a)
        PLATFORM_DIR="npu3"
        ;;
    xrt_smi_ve2.a)
        PLATFORM_DIR="ve2"
        ;;
    *)
        echo "ERROR: Invalid archive name '${ARCHIVE_NAME}'. Must be one of: xrt_smi_phx.a, xrt_smi_strx.a, xrt_smi_npu3.a, xrt_smi_ve2.a" >&2
        exit 1
        ;;
esac

# Validate XRT version format
if ! [[ "${XRT_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "ERROR: Invalid XRT version format '${XRT_VERSION}'. Expected format: major.minor.patch (e.g., 2.21.0)" >&2
    exit 1
fi

# Set default installation directory if not provided
if [ -z "${INSTALL_DIR}" ]; then
    if [ -z "${HOME}" ]; then
        echo "ERROR: HOME environment variable is not set and no custom install directory provided" >&2
        exit 1
    fi
    INSTALL_DIR="${HOME}/.local/share/xrt/${XRT_VERSION}/amdxdna/bins"
fi

# Archive filename and URL (archives are in platform-specific subdirectories)
ARCHIVE_FILE="${ARCHIVE_NAME}"
ARCHIVE_URL="${VTD_REPO_BASE}/${PLATFORM_DIR}/${ARCHIVE_FILE}"
TARGET_PATH="${INSTALL_DIR}/${ARCHIVE_FILE}"

echo "Archive name: ${ARCHIVE_NAME}"
echo "XRT Version: ${XRT_VERSION}"
echo "Installation directory: ${INSTALL_DIR}"
echo "Archive URL: ${ARCHIVE_URL}"
echo ""

# Create installation directory if it doesn't exist
if [ ! -d "${INSTALL_DIR}" ]; then
    echo "Creating installation directory: ${INSTALL_DIR}"
    mkdir -p "${INSTALL_DIR}" || {
        echo "ERROR: Failed to create directory: ${INSTALL_DIR}" >&2
        exit 1
    }
fi

# Check if archive already exists
if [ -f "${TARGET_PATH}" ]; then
    echo "WARNING: Archive already exists at: ${TARGET_PATH}"
    read -p "Overwrite? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Installation cancelled"
        exit 0
    fi
fi

# Check if curl or wget is available
if command -v curl &> /dev/null; then
    DOWNLOAD_CMD="curl"
elif command -v wget &> /dev/null; then
    DOWNLOAD_CMD="wget"
else
    echo "ERROR: Neither curl nor wget found. Please install one of them." >&2
    exit 1
fi

# Download the archive
echo "Downloading archive from ${ARCHIVE_URL}..."
TEMP_FILE=$(mktemp)
trap "rm -f ${TEMP_FILE}" EXIT

if [ "${DOWNLOAD_CMD}" = "curl" ]; then
    if ! curl -fsSL "${ARCHIVE_URL}" -o "${TEMP_FILE}"; then
        echo "ERROR: Failed to download archive. Please check the platform name and network connection." >&2
        exit 1
    fi
else
    if ! wget -q "${ARCHIVE_URL}" -O "${TEMP_FILE}"; then
        echo "ERROR: Failed to download archive. Please check the platform name and network connection." >&2
        exit 1
    fi
fi

# Verify the downloaded file is not empty
if [ ! -s "${TEMP_FILE}" ]; then
    echo "ERROR: Downloaded file is empty. The archive may not exist at ${ARCHIVE_URL}" >&2
    exit 1
fi

# Move the archive to the target location
echo "Installing archive to ${TARGET_PATH}..."
mv "${TEMP_FILE}" "${TARGET_PATH}" || {
    echo "ERROR: Failed to move archive to ${TARGET_PATH}" >&2
    exit 1
}

# Set appropriate permissions
chmod 644 "${TARGET_PATH}" 2>/dev/null || true

echo ""
echo "Successfully installed ${ARCHIVE_FILE}"
echo "Archive location: ${TARGET_PATH}"

exit 0
