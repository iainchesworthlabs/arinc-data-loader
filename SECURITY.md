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

Today the code parses and builds ARINC 665 data in memory; it opens no files of
its own beyond what a caller hands it and opens no network connections. So what
counts, at present, is a crash, hang, out-of-bounds read or other memory-safety
fault that crafted load, header or media data can cause, and a flaw in the build
and workflow files in this repository.

The ARINC 615A protocol engine (TFTP over UDP) and the Qt application are not
built yet. When they are, every message received from a target is untrusted
input too, and this section will say so.

### What to expect

ARINC Data Loader is maintained by one person, so there is no guaranteed response
time and no promised schedule for a fix. Please allow time before you publish
details: the advisory is where that is agreed.

## Supported versions

There has been no release yet. When there is one, this section will say which
versions receive fixes.
