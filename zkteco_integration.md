# ZKTeco Integration Guide

This project currently includes a `ZKTecoScanner` stub for later hardware integration.

## Required SDK / Hardware Integration

When the actual ZKTeco device arrives, implement the scanner integration in `ZKTecoScanner`.

Required functions:

- `connect()`
- `startEnrollment(const std::string& memberId)`
- `verifyFingerprint(std::string& memberId)`
- `disconnect()`

## Integration Points

- `FingerprintScannerInterface` defines the scanner abstraction.
- `FingerprintService` uses the scanner interface to perform enrollment and verification.
- `main.cpp` constructs and checks scanner status at startup.

## Suggested SDK behavior

- `connect()` should attempt to open a network session to the ZKTeco device.
- `startEnrollment(memberId)` should send the enrollment request to the scanner.
- `verifyFingerprint(memberId)` should confirm the captured template belongs to the pending member and return the member ID.
- `disconnect()` should close any open network session cleanly.

## Configuration

The scanner is configured via `config.json`:

```json
{
  "scanner_ip": "192.168.1.100",
  "scanner_port": 4370,
  "enrollment_timeout": 30
}
```

## Current stub behavior

- `connect()` currently prints a placeholder message and returns success.
- `startEnrollment()` prints the configured IP/port and member ID.
- `verifyFingerprint()` returns the pending member ID.

## Future work

- Add `disconnect()` support
- Add repeated health checking loop
- Replace placeholder prints with real SDK/network calls
- Handle enrollment timeout and error recovery
