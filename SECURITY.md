# Security Policy

NVIDIA BIOS Reader parses untrusted binary files. Malformed-input crashes,
out-of-bounds reads, hangs, and unexpected file writes are considered security
issues even though the application does not run with elevated privileges.

Please report a suspected vulnerability privately through GitHub's security
advisory feature once the repository is published. Include a minimal
reproducer or SHA-256-identified sample when redistribution is not permitted.

Do not include private ROM dumps, serial numbers, or proprietary NVIDIA
diagnostic software in a public report.

Only the latest published release will receive security fixes during the
project's experimental phase.
