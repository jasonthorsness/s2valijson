ARG VALIJSON_TAG=v1.0.2

# start with the emscripten builder image
FROM emscripten/emsdk:3.1.65

# get some tools
RUN apt-get update && \
    apt-get install -y wabt

# get valijson
ARG VALIJSON_TAG
RUN git clone --recurse-submodules --branch ${VALIJSON_TAG} \
    https://github.com/tristanpenman/valijson /valijson

# fix valijson (https://github.com/tristanpenman/valijson/issues/195)
RUN sed -i 's/return std::distance(m_itr, other\.itr);/return std::distance(m_itr, other\.m_itr);/' /valijson/include/valijson/adapters/rapidjson_adapter.hpp

# ValidateJSON
COPY main.cpp /main.cpp
COPY mem.cpp /mem.cpp

RUN emcc -DCOMPILE_VALIDATE_JSON -o /validate_json.js \
    /main.cpp /mem.cpp \
    -O3 -s STANDALONE_WASM -s MALLOC=emmalloc -s EVAL_CTORS=2 \
    -fno-rtti -fno-exceptions -flto --no-entry \
    -s EXPORTED_FUNCTIONS='[ \
    "_canonical_abi_realloc", \
    "_canonical_abi_free", \
    "_ValidateJSON"]' \
    -I/valijson/include \
    -I/valijson/thirdparty/rapidjson/include

RUN emcc -DCOMPILE_VALIDATE_JSON_ERRORS -o /validate_json_errors.js \
    /main.cpp /mem.cpp \
    -O3 -s STANDALONE_WASM -s MALLOC=emmalloc -s EVAL_CTORS=2 \
    -fno-rtti -fno-exceptions -flto --no-entry \
    -s EXPORTED_FUNCTIONS='[ \
    "_canonical_abi_realloc", \
    "_canonical_abi_free", \
    "_ValidateJSONErrors"]' \
    -I/valijson/include \
    -I/valijson/thirdparty/rapidjson/include

# extract imports/exports for debugging
RUN wasm2wat /validate_json.wasm | \
    grep -E "(import|export)" \
    > /validate_json.txt
RUN wasm2wat /validate_json_errors.wasm | \
    grep -E "(import|export)" \
    > /validate_json_errors.txt