
### MaNGOS Strings enums generator for C++ source (`Language.h`)
----

There are lots of enums in the source files of *MaNGOS* project.

The biggest file is `Language.h` header file that is more hundreds of enum declarations.

All the enum tags in this `Language.h` refers to an entry (integer) which is stored in the `mangos_string` table in the *world* database.

The main goal of this tool is to help MaNGOS devs & contributors to maintain `Language.h`file easily by pulling available mangos strings from DB, instead of upatding teh file manually.
Other advantages are :
- All mangos strings in DB are present in `Language.h`
- Deleted mangos strings from DB would also be deleted from the file
- Default English content would always be displayed as a comment after the enum int value
- The `Language.h` file will be more homogeneous since all `=` signs will be aligned

### Prerequisites
----


