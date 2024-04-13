# Painless solution for PostgreSQL unconvertable charaters problem

## Features:
1) Prevents encoding translation errors when no reflection exists between byte encoding and UNICODE.
2) Allows to use custom code pages for standard PostgreSQL encodings.
3) Allows to define %-based fprintf() format conversion for non-reflecting characters.

## Prepare system for build
Skip this unit if you decide to build using docker. Assume docker is already instaled.
### macOS
```bash
brew install gcc postgresql@15
```
### Debian/Ubuntu
```bash
sudo apt-get install -y build-essential wget lsb-release
sudo bash -c 'echo "deb https://apt.postgresql.org/pub/repos/apt $(lsb_release -cs)-pgdg main" >> /etc/apt/sources.list.d/pgdg.list'
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt-get update -y
sudo apt-get install -y postgresql-server-dev-15
```

### Fedora/CentOS/RedHat/Oracle Linux
Please refer to [officiasl PostgreSQL installation guides](https://www.postgresql.org/download/linux/redhat/).
```bash
sudo yum install -y build-essential gcc git
sudo yum install -y https://download.postgresql.org/pub/repos/yum/reporpms/EL-7-x86_64/pgdg-redhat-repo-latest.noarch.rpm
sudo yum install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm # CentOS-specific
sudo yum install -y llvm5.0-devel # CentOS-specific
sudo yum install -y centos-release-scl-rh # CentOS-specific
sudo yum install -y postgresql15-devel
echo 'export PATH=$PATH:/usr/pgsql-15/bin/' >> ~/.bash_profile
```
## Obtain sources
### Using git
```bash
git clone https://github.com/vsevolod-volkov/postgresql-friendly-8bit.git
cd postgresql-friendly-8bit
```

### With zip archive
First download srouce code zip archive from: https://github.com/vsevolod-volkov/postgresql-friendly-8bit/releases/latest then unpack it with command line:
```bash
unzip 0.1.zip
cd postgresql-friendly-8bit-0.1
```

## Build **conv_friendly_8bit.so**
### Using make
```bash
make # build conv_friendly_8bit.so shared library
make rebuild # forcedly rebuild all modules
make clean # remove object files and library
```

### Using **build.sh** script
```bash
./build.sh # Native build
./build.sh --docker # Build with docker for default architecture
./build.sh --docker --platform linux/amd64 # Build with docker for x86/64 platform
```

### Using docker container
```bash
docker run --platform linux/amd64 --interactive --tty --rm --volume $(pwd):/project $(docker build --platform linux/amd64 --quiet docker/) make #
```

# PostgreSQL configuration parameters (postgresql.conf)

|Parameter|Type|Required|Default|Description
|-|-|-|-|-|
|friendly_8bit.directory|string (path)|Yes|-|Directory path where friendly_8bit project finds codepage configuration files.|
|friendly_8bit.default_byte|UTF8 string|No|Pirate flag Emoji symbol|UTF-8 sequence to be used instead of any character absent in byte-to-UNICODE conversion. Allows to use single integer type compatible %-modifier in C printf() format such as %d or %x like "[U+0%X]".|
|friendly_8bit.default_unicode|byte string|No|"?"|Byte character string to be used instead of any character absent in UNICODE-to-byte conversion. Allows to use single integer type compatible %-modifier in C printf() format such as %d or %x like "[\\\\x%02X]".|
|friendly_8bit.trace|string (path)|No|-|Is set, sets full path to trace file where trace to be save|
|friendly_8bit.trace_level|integer|No|0|Trace level from 0=off to 10=verbose.|

## Sample postgresql.conf
```
...

friendly_8bit.directory = '/full/path/to/codepages/directory'
friendly_8bit.trace = '/full/path/to/trace/trace.txt'
friendly_8bit.trace_level = 1
friendly_8bit.default_byte = '[\\x%02X]'
friendly_8bit.default_unicode = '[U+0%X]'
```
# Codepages directory
Place all your codepage files in to the same directory and give its name in postgresql.conf friendly_8bit.directory parameter.
Each file must have name exactly matching PostgreSQL encoding name (left table column here: https://www.postgresql.org/docs/16/multibyte.html#CHARSET-TABLE) and have **.f8b** suffix.

For example, file with name **WIN1251.f8b** represents codepage for WIN1251 encoding.

## Codepage file format
Codepage file must contains lines in format of two integers separated by space. First integer - byte character code, second integer - UNICODE character code. Ehter integer may be represented as decimal number without leading zeroes, 0x-prefixed hexadecimal number or 0-prefixed octal number. fscanf() %i format used to read both character codes.

For example, all three cases below represent the same character code of decimal 123:
- 123 - decimal
- 0x7B - hexadecimal
- 0173

Any line in codepage file that does not contain two character codes in specified format will be ignored. So you may prepend comment lines with sharp sign and write freeform remarks or separate codepage section with empty lines.

There is allowed to specify multiple UNICODE codes for the same byte code in separate lines even specify multiple byte codes for same UNICODE code. In such cases when translating last line containing suitable character code will be used. It allows to define multiple-to-one conversions in each direction.

Please find codepage file example [here](codepages/WIN1251.f8b).

# Database setup
## Put shared library to PostgreSQL server
**conv_friendly_8bit.so** file produced as result of source build must be placed to porper directory and must be accessable to PostgreSQL server processes. Please read https://www.postgresql.org/docs/current/sql-load.html topic to get more information about shared library loading in PostgreSQL.

## Create conversion functions
```sql
CREATE FUNCTION friendly_byte_to_utf8(
    integer,  -- source encoding ID
    integer,  -- destination encoding ID
    cstring,  -- source string (null terminated C string)
    internal, -- destination (fill with a null terminated C string)
    integer,  -- source string length
    boolean   -- if true, don't throw an error if conversion fails
) RETURNS integer
     AS '/full/path/to/shared/library/conv_friendly_8bit', 'byte_to_utf8' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION friendly_utf8_to_byte(
    integer,  -- source encoding ID
    integer,  -- destination encoding ID
    cstring,  -- source string (null terminated C string)
    internal, -- destination (fill with a null terminated C string)
    integer,  -- source string length
    boolean   -- if true, don't throw an error if conversion fails
) RETURNS integer
     AS '/full/path/to/shared/library/conv_friendly_8bit', 'utf8_to_byte' LANGUAGE C STRICT;
```
## Turn off default flag of existent conversions
```sql
UPDATE pg_conversion SET condefault = false WHERE conname IN ('windows_1251_to_utf8', 'utf8_to_windows_1251');
```
## Create custom conversions
```sql
CREATE DEFAULT CONVERSION win1251kz_to_utf8
    FOR 'UTF8' TO 'WIN1251' FROM friendly_utf8_to_byte;

CREATE DEFAULT CONVERSION utf8_to_win1251kz
    FOR 'WIN1251' TO 'UTF8' FROM friendly_byte_to_utf8;

```
