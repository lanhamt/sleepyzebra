The files `trickle.c` and `sleepy_trickle.c` implement the Trickle algorithm
on the TockOS platform. Place these files in the `tock/userland/examples/[sleepy_]trickle/`
directories, and build with the libtock library. Note that these files will not
compile by themselves, and require the libraries that come with Tock in order
to run. Also note that the hardware random number generator on the Imix platform
was removed from the kernel, and must be reinitalized in `tock/boards/imix/src/main.rs`.

A full working example can be found at: `https://github.com/ptcrews/tock/tree/cs244b_trickle`
