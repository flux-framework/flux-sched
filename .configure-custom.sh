#!/bin/sh

ENABLE_VARS="code-coverage|On|ENABLE_COVERAGE docs|On|ENABLE_DOCS sanitize-address|On|SANITIZE_ADDRESS sanitize-undefined|On|SANITIZE_UNDEFINED"
DISABLE_VARS="docs|Off|ENABLE_DOCS"
ENABLE_ENABLE_COVERAGE_DOC="enable coverage analysis"
ENABLE_ENABLE_DOCS_DOC="enable building manpages"
ENABLE_DISABLE_DOCS_DOC="disable building manpages"
ENABLE_SANITIZE_ADDRESS_DOC="build sanitized targets with AddressSanitizer"
ENABLE_SANITIZE_UNDEFINED_DOC="build sanitized targets with UndefinedBehaviorSanitizer"
WITH_VARS="systemdsystemunitdir|/etc/systemd/system|FLUX_SYSTEMD_SYSTEM_UNIT_DIR"
WITH_FLUX_SYSTEMD_SYSTEM_UNIT_DIR_DOC="systemd system directory"
