// =============================================================================
// Eclipse32 - HTTP/1.0 GET client
// Resolves hostname via DNS, connects via TCP, sends GET, reads response body.
// =============================================================================
#pragma once
#include "../kernel.h"

// Download a URL into a heap-allocated buffer. Caller must kfree() it.
// Returns NULL on failure. Sets *out_len to body length.
// url must be http:// (no HTTPS - we have no TLS).
uint8_t *http_get(const char *url, uint32_t *out_len);

// Download a URL directly to a file on FAT32.
// Shows progress to terminal if show_progress is true.
// Returns 0 on success, -1 on failure.
int http_get_to_file(const char *url, const char *dest_path, bool show_progress);
