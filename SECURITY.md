# Security policy

## Reporting a vulnerability

Please report security vulnerabilities **privately**, not as a public issue or
pull request, through GitHub's private vulnerability reporting: open the
repository's **Security** tab and choose **Report a vulnerability**. It opens a
private advisory that only you and the maintainer can see.

What helps most:

- what is affected: the version or commit, the platform, and whether it is the
  ARINC 665 library, the ARINC 615A library or an application built on them;
- how to reproduce it: the smallest load, header file or message that shows it
  (a hex dump is fine), and the command line or test that feeds it in;
- what someone could do with it.

Please do not attach a load, media set or capture taken from a real aircraft or
a customer's equipment.

If you cannot use the form (you have no GitHub account, say), open an issue that
says only that you have a security matter, with no details, and the maintainer
will reply there to arrange a private channel.

### What counts

The code parses and builds ARINC 665 data, speaks ARINC 615A (TFTP over UDP, FIND
by broadcast) to a target, reads and writes media sets and downloaded files on
disk, and ships three Qt applications (the loader, the media set generator and
the mock target). So what counts is:

- a crash, hang, out-of-bounds read or other memory-safety fault that crafted
  load, header or media data can cause;
- the same from a message a target (or anything that can send UDP to the
  loader's control port) sends: every datagram, status file, file list and file
  name received from a target is untrusted input;
- a file name or path from a target or a media set that makes the loader read or
  write outside the directory it was given;
- a flaw in the build and workflow files in this repository, or in what the
  release process publishes.

The applications do not authenticate a target or encrypt anything: ARINC 615A
over TFTP has neither, and a hostile network is outside what they defend
against. A report that depends on that is a limit of the protocol, not a
vulnerability in this code.

### What to expect

ARINC Data Loader is maintained by one person, so there is no guaranteed response
time and no promised schedule for a fix. Please allow time before you publish
details: the advisory is where that is agreed.

## Supported versions

There has been no release yet. When there is one, this section will say which
versions receive fixes. Until 1.0, only the latest release does.

## Signed releases

A release's files are signed with a dedicated release key, listed in `SHA256SUMS`, and
attested by GitHub; [Verifying a release](docs/verifying-releases.md) says how to check them.
The key is:

```text
ARINC Data Loader Releases <iain.chesworth@gmail.com>
Ed25519, created 2026-10-19, expires 2028-10-18
Fingerprint: C6B4 AD93 067E B501 F450  5BCF 3FF3 6825 5329 8A53
```

The public key is [`docs/release-signing-key.asc`](docs/release-signing-key.asc). It is not on a
keyserver. A key that does not have this fingerprint did not sign a release of this project; if you
are shown one that claims to, report it as above. If the key is ever revoked or replaced, this
section says so and gives the new fingerprint.
