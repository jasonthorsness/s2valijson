#!/bin/bash
set -e

# Update to create a new version
VERSION=2024_08_27

rm -rf ./build
mkdir -p ./build
docker build -t s2valijson_$VERSION ./docker/
docker create --name s2valijson_$VERSION s2valijson_$VERSION
docker cp s2valijson_$VERSION:/validate_json.wasm ./build/validate_json.wasm
docker cp s2valijson_$VERSION:/validate_json.txt ./build/validate_json.txt
docker cp s2valijson_$VERSION:/validate_json_errors.wasm ./build/validate_json_errors.wasm
docker cp s2valijson_$VERSION:/validate_json_errors.txt ./build/validate_json_errors.txt
docker rm s2valijson_$VERSION

echo "CREATE FUNCTION s2valijson_${VERSION}_validate_json_inner(
    input LONGTEXT NOT NULL,
    sch LONGTEXT NOT NULL)
    RETURNS TINYINT(1) NOT NULL
    AS WASM
    FROM LOCAL INFILE 'validate_json.wasm'
    USING EXPORT 'ValidateJSON';

DELIMITER //
CREATE FUNCTION s2valijson_${VERSION}_validate_json(
    input LONGTEXT NULL,
    sch LONGTEXT NULL)
RETURNS TINYINT(1) AS
BEGIN
    IF ISNULL(input) OR ISNULL(sch) THEN
        RETURN NULL;
    END IF;
    RETURN s2valijson_${VERSION}_validate_json_inner(input, sch);
END //
DELIMITER ;

CREATE FUNCTION s2valijson_${VERSION}_validate_json_errors_inner(
    input LONGTEXT NOT NULL,
    sch LONGTEXT NOT NULL)
    RETURNS RECORD(success TINYINT(1) NOT NULL, errors LONGTEXT NOT NULL)
    AS WASM
    FROM LOCAL INFILE 'validate_json_errors.wasm'
    USING EXPORT 'ValidateJSONErrors';

DELIMITER //
CREATE FUNCTION s2valijson_${VERSION}_validate_json_errors(
    input LONGTEXT NULL,
    sch LONGTEXT NULL)
RETURNS LONGTEXT AS
DECLARE
    result RECORD(success TINYINT(1) NOT NULL, errors LONGTEXT NOT NULL);
BEGIN
    IF ISNULL(input) OR ISNULL(sch) THEN
        RETURN NULL;
    END IF;
    result = s2valijson_${VERSION}_validate_json_errors_inner(input, sch);
    IF result.success = 1 THEN
        RETURN NULL;
    ELSE
        RETURN result.errors;
    END IF;
END //
DELIMITER ;

DELIMITER //
CREATE FUNCTION s2valijson_${VERSION}_validate_json_raise(
    input LONGTEXT NULL,
    sch LONGTEXT NULL)
RETURNS LONGTEXT AS
DECLARE
    result RECORD(success TINYINT(1) NOT NULL, errors LONGTEXT NOT NULL);
BEGIN
    IF ISNULL(input) OR ISNULL(sch) THEN
        RETURN NULL;
    END IF;
    result = s2valijson_${VERSION}_validate_json_errors_inner(input, sch);
    IF result.success = 1 THEN
        RETURN input;
    ELSE
        RAISE USER_EXCEPTION(result.errors);
    END IF;
END //
DELIMITER ;
" \
> ./build/s2valijson_${VERSION}.sql

tar -cf ./dist/${VERSION}.tar -C ./build/ \
validate_json.wasm validate_json_errors.wasm s2valijson_${VERSION}.sql

BASE64=$(base64 -w 0 ./dist/${VERSION}.tar)

echo "
DROP EXTENSION IF EXISTS s2valijson_${VERSION};
CREATE EXTENSION s2valijson_${VERSION} FROM BASE64 '$BASE64';

-- s2valijson_validate_json
DELIMITER //
CREATE OR REPLACE FUNCTION s2valijson_validate_json(
  input LONGTEXT,
  sch LONGTEXT)
  RETURNS TINYINT(1) AS
BEGIN
  RETURN s2valijson_${VERSION}_validate_json(input, sch);
END //
DELIMITER ;

-- s2valijson_validate_json_raise
DELIMITER //
CREATE OR REPLACE FUNCTION s2valijson_validate_json_raise(
  input LONGTEXT,
  sch LONGTEXT)
  RETURNS LONGTEXT AS
BEGIN
  RETURN s2valijson_${VERSION}_validate_json_raise(input, sch);
END //
DELIMITER ;

-- s2valijson_validate_json_errors
DELIMITER //
CREATE OR REPLACE FUNCTION s2valijson_validate_json_errors(
  input LONGTEXT,
  sch LONGTEXT)
  RETURNS LONGTEXT AS
BEGIN
  RETURN s2valijson_${VERSION}_validate_json_errors(input, sch);
END //
DELIMITER ;
" \
> ./build/install.sql
