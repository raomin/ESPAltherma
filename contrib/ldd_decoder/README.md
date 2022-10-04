Quick and dirty LDD decoder to get Rotex/Daikin protocol from official Dchecker label definition files.
Relies directly on Dchecker functions.

LDD files are compressed (deflated) binary serialization of C# structures, representing Rotex/Daikin protocols definitions for various devices. Using the official Dchecker binary as external reference allows easily using its internal function to decode and manipulate those.

Jetbrains dotPeek tool is your best friend to browse Dchecker binaries.
