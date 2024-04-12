# Painless solution for PostgreSQL unconvertable charaters problem

## Features:
1) Prevents encoding translation errors when no reflection exists between byte encoding and UNICODE.
2) Allows to use custom code pages for standard PostgreSQL encodings.
3) Allows to define %-based fprintf() format conversion for non-reflecting characters.

## Prepare system for build
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

## Build
### Using make
```bash
make # build conv_friendly_8bit.so shared library
make rebuild # forcedly rebuild all modules
make clean # remove object files and library
```

### Using **build.sh** script
```bash
./build.sh
```

### Using docker container
```bash
./build.sh --docker # Build for current platform
./build.sh --docker --platform linux/amd64 # Build for x86/64 platform
```

# PostgreSQL configuration parameters (postgresql.conf)

|Parameter|Type|Required|Default|Description
|-|-|-|-|-|
|friendly_8bit.directory|string (path)|Yes|-|Directory path where friendly_8bit project finds codepage configuration files|
|friendly_8bit.default_byte|UTF8 string|No|Pirate flag Emoji symbol|UTF-8 sequence to be used instead of any character absent in byte-to-UNICODE conversion. Allows to use single integer type compatible %-modifier in C printf() format such as %d or %x like "[U+0%X]".|
|friendly_8bit.default_unicode|byte string|No|"?"|Byte character string to be used instead of any character absent in UNICODE-to-byte conversion. Allows to use single integer type compatible %-modifier in C printf() format such as %d or %x like "[\\\\x%02X]".|
|friendly_8bit.trace|string (path)|No|-|Is set, sets full path to trace file where trace to be save|

# Database setup
## Create conversion functions
```PostgreSQL
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
```PostgreSQL
UPDATE pg_conversion SET condefault = false WHERE conname IN ('windows_1251_to_utf8', 'utf8_to_windows_1251');
```
## Create custom conversions
```PostgreSQL
CREATE DEFAULT CONVERSION win1251kz_to_utf8
    FOR 'UTF8' TO 'WIN1251' FROM friendly_utf8_to_byte;

CREATE DEFAULT CONVERSION utf8_to_win1251kz
    FOR 'WIN1251' TO 'UTF8' FROM friendly_byte_to_utf8;

```
