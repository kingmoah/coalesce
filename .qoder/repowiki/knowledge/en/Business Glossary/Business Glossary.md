---
kind: business_term
name: Business Glossary
category: business_term
scope:
    - '**'
---

### bare minimum features
- Definition：The minimal set of SSH-2.0 capabilities required for coalesce to function as a working client and server — specifically ed25519-only host keys and user keys, curve25519-sha256 key exchange, AES-CTR encryption with HMAC-SHA2-256, and password/public-key authentication — deliberately excluding RSA, channels, and shell subsystems.
- Aliases：M0/M1/M2 scope、minimal viable SSH

### Phase 1 / Phase 2 / Phase 3 / Phase 4 tests
- Definition：Incremental test suites in `tests/test_phaseN.c` that verify each layer of the SSH stack in isolation: Phase 1 covers buffer framing, Phase 2 validates hand-rolled crypto primitives (SHA-256/512, HMAC, X25519, Ed25519, AES-CTR, base64, RNG), Phase 3 exercises the session layer (banner exchange, plaintext/encrypted framing, sequence numbers), and Phase 4 targets the KEX handshake driver and hostkey module.
- Aliases：test_phase1.c、test_phase2.c、test_phase3.c、test_phase4.c

### TOFU known_hosts
- Definition：Trust-on-first-use handling for host key verification: on first connection to a new host, accept the presented host key without prompting and record it locally, so subsequent connections can detect key changes. Planned as part of the M2 KEX/hostkey work.
- Aliases：TOFU、trust on first use
