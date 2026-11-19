# Safety of use

ARINC Data Loader is free software with no warranty. It is a preliminary release, and nothing in it has been checked against real equipment. This page says what the programs are for, and what not to rely on them for.

## What they are for

- Learning how an ARINC 615A loader and a target talk, and how an ARINC 665 media set is laid out.
- Making and checking media sets on a bench, and trying them on the mock target.
- Trying other loaders and tools on the sample media sets.
- Building your own tools on the libraries.

## What not to rely on them for

Do not rely on the programs, unless your own programme has assessed them for that use, to:

- find an error in software or data that will be loaded into equipment in service, or to show that a load or a media set is correct;
- leave software or data unchanged between the file you chose and the file the target receives;
- load equipment in service;
- show that a target behaves as ARINC 615A says it should.

Why:

- The programs have been tested against a simulated target and a mock target that the project wrote from its own reading of the standards. Both ends of those tests share that reading, so the tests cannot show that the reading is right ([What has been checked](../status.md)).
- The places where the standards are unclear were settled by the project's own choices ([Limits and known issues](limits.md#not-checked)).
- The check the loader makes of a media set is what the code does: it reads the set, checks its lengths, CRCs and check values, and reports departures from the standard's recommendations as warnings. It is not a review of the load's content.
- A checksum in a load detects damage. It does not detect tampering ([Security and threat model](../security.md)).
- Nothing about the programs has been reviewed by anyone independent of the person who wrote them.

## What to do

- Check what you load by a method you trust, as well as by this one.
- If you use the programs in work that needs evidence about the tool, your own programme decides how the tool is used and what assessment that needs. [Assurance](../assurance.md) says what evidence exists and what does not.
- Report a problem you find ([Support](../support.md)).
