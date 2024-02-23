# ABI to bind zig driver code to our kernel frameworks
NOTE: this is experimental
Here we try to create a nice binding for zig to integrate with our kernel driver frameworks. This would enable
us to create external device drivers in zig, for aniva, which would enhance driver development productivity I feel =)

# NOTES
- It seems like functions with inline assembly are not translated propperly into the ABI
