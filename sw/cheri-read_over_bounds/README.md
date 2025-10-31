# CHERI ISAv9 "Read over Bounds" example

This simple program shows how CHERI limits the range of addresses that can be dereferenced.
Reading a value outside this range results in a "CHERI exception: LengthViolation."


You must have a CHERI ISAv9 Purecap toolchain in your executable search path!

 1. Create the toolchain with [cheribuild](https://github.com/CTSRD-CHERI/cheribuild) (tested with git-hash dbafb3f67e)
 2. Add the sdk directory to you ```PATH``` environment variable. For example:
```
export PATH=/home/user/cheri/output/sdk/bin:$PATH
```
