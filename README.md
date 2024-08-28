# s2valijson

The [Valijson JSON Schema library](https://github.com/tristanpenman/valijson) packaged as a SingleStore extension! You
might find this extension useful if you need to validate JSON against a schema. Supports
[Draft 7 JSON Schema](https://json-schema.org/draft-07/json-schema-release-notes).

🚧 Use it at insertion time to raise exceptions on schema validation failure!

🔎 Find documents that match or don't match arbitrary schemas!

📋 Enjoy detailed error messages identifying exactly how a document fails validation!

## Installation

Install or upgrade `s2valijson` on SingleStore 8.5 or higher with the following SQL. This method of installation
(extension + wrappers) enables zero-downtime upgrade or rollback.

```sql
-- versioned s2valijson extension
CREATE EXTENSION s2valijson_2024_08_27
FROM HTTP
'https://github.com/jasonthorsness/s2valijson/raw/main/dist/2024_08_27.tar';

-- s2valijson_validate_json
DELIMITER //
CREATE OR REPLACE FUNCTION s2valijson_validate_json(
  input LONGTEXT,
  sch LONGTEXT)
  RETURNS TINYINT(1) AS
BEGIN
  RETURN s2valijson_2024_08_27_validate_json(input, sch);
END //
DELIMITER ;

-- s2valijson_validate_json_raise
DELIMITER //
CREATE OR REPLACE FUNCTION s2valijson_validate_json_raise(
  input LONGTEXT,
  sch LONGTEXT)
  RETURNS LONGTEXT AS
BEGIN
  RETURN s2valijson_2024_08_27_validate_json_raise(input, sch);
END //
DELIMITER ;

-- s2valijson_validate_json_errors
DELIMITER //
CREATE OR REPLACE FUNCTION s2valijson_validate_json_errors(
  input LONGTEXT,
  sch LONGTEXT)
  RETURNS LONGTEXT AS
BEGIN
  RETURN s2valijson_2024_08_27_validate_json_errors(input, sch);
END //
DELIMITER ;
```

## Usage

The extension includes three commands: `s2valijson_validate_json`, `s2valijson_validate_json_raise`, and
`s2valijson_validate_json_errors`.

These functions are optimized for use with a constant schema within a given query. If the schema used varies across rows
(like the schema itself is stored in a column in the same table as the document being tested) execution will be slower.

Setup for the examples below:

```sql
-- documents being tested
CREATE TABLE tbl(col JSON COLLATE utf8mb4_bin NOT NULL);
INSERT INTO tbl VALUES
('{"a":"foo","b":"bar"}'),
('{"a":"foo"}'),
('{"a":"zzz","b":1}');

-- schemas
CREATE TABLE sch(id BIGINT NOT NULL PRIMARY KEY, col JSON COLLATE utf8mb4_bin NOT NULL);
INSERT INTO sch VALUES
(1, '
{
  "type": "object",
  "properties": {
    "a": {
      "type": "string",
      "enum": ["foo", "baz", "cat"]
    },
    "b": {
      "type": "string"
    }
  },
  "required": [ "a"]
}
')
```

### `s2valijson_validate_json`

`s2valijson_validate_json` takes an input string and a schema. If either is NULL, it returns NULL. Otherwise, it returns
1 if the input validates against the schema or 0 otherwise.

Example:

```sql
SELECT col FROM tbl WHERE s2valijson_validate_json(col, (SELECT col FROM sch WHERE id = 1));

-- {"a":"foo","b":"bar"}
-- {"a":"foo"}
```

### `s2valijson_validate_json_raise`

`s2valijson_validate_json_raise` takes an input string and a schema. If either is NULL, it returns NULL. Otherwise, it
returns the input string if the input validates against the schema or raises an exception otherwise.

Examples:

```sql
-- invalid blocks insert
WITH row AS (SELECT * FROM TABLE([
  s2valijson_validate_json_raise(
    '{"a":"yyy"}',
    (SELECT col FROM sch WHERE id = 1))
]))
INSERT INTO tbl SELECT * FROM row;

-- ERROR 2242 ER_USER_RAISE: Unhandled exception Type: ER_USER_RAISE (2242)
-- Message:
-- <root>.[a]: Failed to match against any enum values.
-- <root>: Failed to validate against schema associated with property name 'a'.
```

```sql
-- valid allowed
WITH row AS (SELECT * FROM TABLE([
  s2valijson_validate_json_raise(
    '{"a":"baz", "t":true}',
    (SELECT col FROM sch WHERE id = 1))
]))
INSERT INTO tbl SELECT * FROM row;

-- inserted one row
```

### `s2valijson_validate_json_errors`

`s2valijson_validate_json_errors` takes an input string and a schema. If either is NULL, it returns NULL. Otherwise, it
returns NULL if the input validates against the schema or an error string otherwise.

Examples:

```sql
-- return errors for each row
SELECT col, s2valijson_validate_json_errors(col, (SELECT col FROM sch WHERE id = 1)) FROM tbl;
```

```text
{"a":"zzz","b":1}
  <root>.[a]: Failed to match against any enum values.
  <root>: Failed to validate against schema associated with property name 'a'.
  <root>.[b]: Value type not permitted by 'type' constraint.
  <root>: Failed to validate against schema associated with property name 'b'.
{"a":"baz","t":true}
  NULL
{"a":"foo"}
  NULL
{"a":"foo","b":"bar"}
  NULL
```

## Development

To build, you'll need to have Docker installed and a Linux-compatible shell. The build process is performed within a
Docker image -- see the [/docker](/docker/) folder for sources and the Dockerfile.

To build and publish, run [build.sh](/build.sh).

Edit the version at the top of build.sh to make a new version.

## Testing

After building, you can install by running the contents of /build/install.sql. To do this easily locally, run the docker
version of SingleStore

```
docker run --rm -d \
 --name singlestoredb-dev \
 -e ROOT_PASSWORD="password123" \
 -p 3306:3306 -p 8080:8080 -p 9000:9000 \
 ghcr.io/singlestore-labs/singlestoredb-dev:latest
```

Then browse to https://localhost:8080, enter root/password123, paste the contents of install.sql into the SQL editor,
and run it.

## Publishing

The build process produces a .tar file under [/dist](/dist/); this is ready for users to install as an extension. It can
be installed directly from GitHub or the file can be uploaded to any other location or just passed as a base64 string
(see the
[documentation](https://docs.singlestore.com/cloud/reference/sql-reference/procedural-sql-reference/extensions/) for
more options).
