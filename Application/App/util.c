// Copyright 2022 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "app.h"

// Convert a byte to 2 hex digits, null-terminating it
void utilHTOA8(unsigned char n, char *p)
{
    unsigned char nibble = (n >> 4) & 0xf;
    if (nibble >= 10) {
        *p++ = 'a' + (nibble-10);
    } else {
        *p++ = '0' + nibble;
    }
    nibble = n & 0xf;
    if (nibble >= 10) {
        *p++ = 'a' + (nibble-10);
    } else {
        *p++ = '0' + nibble;
    }
    *p = '\0';
}

// Convert an address to hex
void utilAddressToText(const uint8_t *address, char *buf, uint32_t buflen)
{
    *buf = '\0';
    for (int i=ADDRESS_LEN-1; i>=0; i--) {
        char hex[3];
        utilHTOA8(address[i], hex);
        strlcat(buf, hex, buflen);
    }
}

// Given an env var with a name and potentially a location in parens, extract them,
// and the name buf must be SENSOR_NAME_MAX.
void extractNameComponents(char *in, char *namebuf, char *olcbuf, uint32_t olcbuflen)
{

    // Skip leading blanks
    while (*in == ' ' && *in != '\0') {
        in++;
    }

    // Copy name until a paren
    uint32_t namelen = 0;
    while (*in != '\0' && *in != '(') {
        if (namebuf != NULL && namelen < (SENSOR_NAME_MAX-1)) {
            namebuf[namelen++] = *in;
        }
        in++;
    }

    // Remove trailing whitespace and null-terminate
    if (namebuf != NULL) {
        while (namelen > 0 && namebuf[namelen-1] == ' ') {
            namelen--;
        }
        namebuf[namelen] = '\0';
    }

    // Extract OLC
    if (olcbuf != NULL) {
        *olcbuf = '\0';
        if (*in == '[') {
            size_t olclen = 0;
            while (true) {
                ++in;
                if (*in == '\0' || *in == ']') {
                    break;
                }
                if (olclen < (olcbuflen-1)) {
                    olcbuf[olclen++] = *in;
                }
            }
            olcbuf[olclen] = '\0';
        }
    }

}
