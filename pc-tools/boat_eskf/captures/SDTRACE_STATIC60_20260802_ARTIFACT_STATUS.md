# SDTRACE static-60 artifact status

- RUN0003.BIN: downloaded through Web API, 0 byte.
- RUN0003.TXT: Web API returned HTTP 404; no generated TXT was available to save.
- Display image at failure: not obtained. The current firmware has no PC-accessible display capture endpoint, and no photograph was supplied.
- vfs_api.cpp missing-TXT line in the serial record was produced by the requested TXT download check.
- Full start-to-end application serial and SDTRACE are unavailable because of the USB re-enumeration capture gap; see docs/SDTRACE_STATIC60_TEST_20260802.md.