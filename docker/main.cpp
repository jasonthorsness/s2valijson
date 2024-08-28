#include <emscripten.h>

#include <valijson/adapters/rapidjson_adapter.hpp>

#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

#include <rapidjson/document.h>

// Globals to save a compiled schema across invocations.
//
std::string_view latchedSchemaRaw;
valijson::Schema* latchedSchema;

struct ValidateResult
{
    bool success;
    char* errorsPtr;
    size_t errorsLen;
};

ValidateResult validateResult;

#ifdef COMPILE_VALIDATE_JSON
extern "C" EMSCRIPTEN_KEEPALIVE bool ValidateJSON(
    char* testPtr, size_t testLen, char* schemaPtr, size_t schemaLen)
{
    std::string_view test(testPtr, testLen);
    std::string_view schemaRaw(schemaPtr, schemaLen);

    rapidjson::Document testDoc;
    testDoc.Parse(test.data(), test.size());
    valijson::adapters::RapidJsonAdapter testAdapter(testDoc);
    valijson::Schema* schema = nullptr;
    valijson::Schema ephemeralSchema;
    valijson::Validator validator;

    if (latchedSchema != nullptr && schemaRaw != latchedSchemaRaw)
    {
        rapidjson::Document schemaDoc;
        schemaDoc.Parse(schemaRaw.data(), schemaRaw.size());
        valijson::adapters::RapidJsonAdapter schemaAdapter(schemaDoc);
        valijson::SchemaParser parser;
        parser.populateSchema(schemaAdapter, ephemeralSchema);
        schema = &ephemeralSchema;
    }

    if (latchedSchema == nullptr)
    {
        rapidjson::Document schemaDoc;
        schemaDoc.Parse(schemaRaw.data(), schemaRaw.size());
        valijson::adapters::RapidJsonAdapter schemaAdapter(schemaDoc);
        latchedSchema = new valijson::Schema();
        latchedSchemaRaw = schemaRaw;
        valijson::SchemaParser parser;
        parser.populateSchema(schemaAdapter, *latchedSchema);
        schemaPtr = nullptr;
    }

    if (schema == nullptr)
    {
        schema = latchedSchema;
    }

    bool result = validator.validate(*schema, testAdapter, nullptr);
    if (schemaPtr != nullptr)
    {
        free(schemaPtr);
    }
    free(testPtr);
    return result;
}
#endif

#ifdef COMPILE_VALIDATE_JSON_ERRORS
extern "C" EMSCRIPTEN_KEEPALIVE ValidateResult* ValidateJSONErrors(
    char* testPtr, size_t testLen, char* schemaPtr, size_t schemaLen)
{
    std::string_view test(testPtr, testLen);
    std::string_view schemaRaw(schemaPtr, schemaLen);

    rapidjson::Document testDoc;
    testDoc.Parse(test.data(), test.size());
    valijson::adapters::RapidJsonAdapter testAdapter(testDoc);
    valijson::Validator validator;
    valijson::ValidationResults results;
    valijson::Schema* schema = nullptr;
    valijson::Schema ephemeralSchema;

    if (latchedSchema != nullptr && schemaRaw != latchedSchemaRaw)
    {
        rapidjson::Document schemaDoc;
        schemaDoc.Parse(schemaRaw.data(), schemaRaw.size());
        valijson::adapters::RapidJsonAdapter schemaAdapter(schemaDoc);
        valijson::SchemaParser parser;
        parser.populateSchema(schemaAdapter, ephemeralSchema);
        schema = &ephemeralSchema;
    }

    if (latchedSchema == nullptr)
    {
        rapidjson::Document schemaDoc;
        schemaDoc.Parse(schemaRaw.data(), schemaRaw.size());
        valijson::adapters::RapidJsonAdapter schemaAdapter(schemaDoc);
        latchedSchema = new valijson::Schema();
        latchedSchemaRaw = schemaRaw;
        valijson::SchemaParser parser;
        parser.populateSchema(schemaAdapter, *latchedSchema);
        schemaPtr = nullptr;
    }

    if (schema == nullptr)
    {
        schema = latchedSchema;
    }

    if (validator.validate(*schema, testAdapter, &results))
    {
        validateResult = {true, nullptr, 0};
        if (schemaPtr != nullptr)
        {
            free(schemaPtr);
        }
        free(testPtr);
        return &validateResult;
    }

    std::string errors;
    for (const valijson::ValidationResults::Error& error : results)
    {
        std::string delim("");
        for (const std::string& segment : error.context)
        {
            errors += delim;
            delim = ".";
            errors += segment;
        }
        errors += ": " + error.description + "\n";
    }

    char* errorsPtr = static_cast<char*>(malloc(errors.size()));
    memcpy(errorsPtr, errors.data(), errors.size());
    validateResult = {false, errorsPtr, errors.size()};

    if (schemaPtr != nullptr)
    {
        free(schemaPtr);
    }
    free(testPtr);
    return &validateResult;
}
#endif