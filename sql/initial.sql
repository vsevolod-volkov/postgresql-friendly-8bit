-- IMPORTANT: Have to add friendly_8bit.directory = '/path/to/codepages/directory' parameter to postgresql.conf

DROP CONVERSION IF EXISTS win1251kz_to_utf8;
DROP CONVERSION IF EXISTS utf8_to_win1251kz;

DROP FUNCTION IF EXISTS friendly_byte_to_utf8(
    integer,  -- source encoding ID
    integer,  -- destination encoding ID
    cstring,  -- source string (null terminated C string)
    internal, -- destination (fill with a null terminated C string)
    integer,  -- source string length
    boolean   -- if true, don't throw an error if conversion fails
) CASCADE;

DROP FUNCTION IF EXISTS friendly_utf8_to_byte(
    integer,  -- source encoding ID
    integer,  -- destination encoding ID
    cstring,  -- source string (null terminated C string)
    internal, -- destination (fill with a null terminated C string)
    integer,  -- source string length
    boolean   -- if true, don't throw an error if conversion fails
) CASCADE;

CREATE FUNCTION friendly_byte_to_utf8(
    integer,  -- source encoding ID
    integer,  -- destination encoding ID
    cstring,  -- source string (null terminated C string)
    internal, -- destination (fill with a null terminated C string)
    integer,  -- source string length
    boolean   -- if true, don't throw an error if conversion fails
) RETURNS integer
     AS '/Users/vmvolkov/Work/postgresql-friendly-8bit/conv_friendly_8bit', 'byte_to_utf8' LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION friendly_utf8_to_byte(
    integer,  -- source encoding ID
    integer,  -- destination encoding ID
    cstring,  -- source string (null terminated C string)
    internal, -- destination (fill with a null terminated C string)
    integer,  -- source string length
    boolean   -- if true, don't throw an error if conversion fails
) RETURNS integer
     AS '/Users/vmvolkov/Work/postgresql-friendly-8bit/conv_friendly_8bit', 'utf8_to_byte' LANGUAGE C STRICT;

UPDATE pg_conversion SET condefault = false WHERE conname IN ('windows_1251_to_utf8', 'utf8_to_windows_1251');

CREATE DEFAULT CONVERSION win1251kz_to_utf8
    FOR 'UTF8' TO 'WIN1251' FROM friendly_utf8_to_byte;

CREATE DEFAULT CONVERSION utf8_to_win1251kz
    FOR 'WIN1251' TO 'UTF8' FROM friendly_byte_to_utf8;
