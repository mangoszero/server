
## MaNGOS Strings enums generator for C++ source (`Language.h`)
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

## Prerequisites
----

### PHP version

- PHP must be installed *(These scripts have been tested sith PHP 7.3.5)*
- PHP executable must be added to your PATH variable (Warning Windows user!) - To check that, type `php -v`in a command prompt, if it returns teh PHP version, you're good to go !
- PHP PDO extension must be enabled *(most of the time it is the case in most recent installations)*.

## How to use ?
----

This tool should be used when a you want to add new mangos string in the C++ sources.

**/!\ WARNING :** Before using the generator, the mangos string should have been added in the `mangos_string` table of teh wodl DB of the core you're targeting

### PHP version

**FIRST TIME USE :**

1. Make a copy of the `php-cli/includes/config.inc.php.default` file in the same folder and name it `config.inc.php`.
1. Edit `config.inc.php` fiel to fil your config

**REGULAR USE :**

1. Open a terminal and go in the `php-cli` folder of this project
1. Type `php GenerateLanguage.h.php`
1. Select the core version you're targeting and press enter
1. You're done, the file has been generated

Be sure the output file has been correctly generated in the correct source folder in order to rebuild the core.
If not, try to copy it manually since it has been generated in the `out` folder if no output directory has been set in the `config.inc.php` file.

## How to contribute ?
----

You can develop other versions of this generator using other languages (e.g : python, C, perl ...) 
