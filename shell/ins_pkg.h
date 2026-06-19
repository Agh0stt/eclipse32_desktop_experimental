// =============================================================================
// Eclipse32 - .INS install package (Eclipse32 Install Stream)
// =============================================================================
#pragma once

#define INS_MAX_PATH      256
#define INS_CFG_MAX_SIZE  8192
#define INS_MAGIC         0x534E4945u   // little-endian "EINS"
#define INS_VERSION       1

// ins_install_from_file: unpacks resolved path to FAT32.
// Returns 0 on success, negative on error.
int ins_install_from_file(const char *resolved_ins_path);
