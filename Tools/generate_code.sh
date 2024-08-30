#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(cd -- "${SCRIPT_DIR}/../" && pwd)
TOOLS_DIR=$(cd -- "${REPO_ROOT}/Tools/" && pwd)
THIRD_PARTY_DIR=$(cd -- "${REPO_ROOT}/Source/ThirdParty/" && pwd)

# ====================================================================================

echo "Retrieving input arguments..."

# Get path of proto files from input args
if [[ -z $1 || -z $2 ]]; then
    echo "Error: missing arguments"
    echo "Usage: generate_code.sh <path_to_proto_dir> <proto_service>"
    echo "eg. generate_code.sh C:/dev/proto render/v1"
    exit
fi

input_proto_dir=$1
input_proto_service=$2

if [[ ! -d $1 ]]; then
    echo "$1 is not a directory!"
    exit
fi

input_proto_service_dir=$1/$2

if [[ ! -d $input_proto_service_dir ]]; then
    echo "$input_proto_service_dir is not a directory!"
    exit
fi

files=($input_proto_service_dir/*)    
input_proto_file=${files[0]}

if [[ ! -f $input_proto_file ]]; then
    echo "$input_proto_file is not a file!"
    exit
fi

# ====================================================================================

echo "Creating output directories..."

# Setup output directories for protoc and turbolink
turbolink_output_dir="${REPO_ROOT}/Output"
cpp_output_dir="${turbolink_output_dir}/Private/pb"

# Wipe any existing output directory and create a new one
if [[ -d "${turbolink_output_dir}" ]]; then
    rm -rf "${turbolink_output_dir}"
fi
mkdir "${turbolink_output_dir}"

# Make the pb output directory for grpc_cpp output
if [[ ! -d "${cpp_output_dir}" ]]; then
    mkdir -p "${cpp_output_dir}"
fi

# ====================================================================================

echo "Running protoc code generation..."

# Establish proto paths
protoc_exe="${THIRD_PARTY_DIR}/protobuf/bin/protoc.exe"
grpc_cpp_plugin_exe="${THIRD_PARTY_DIR}/grpc/bin/grpc_cpp_plugin.exe"
turbolink_plugin_exe="${TOOLS_DIR}/protoc-gen-turbolink.exe"
protobuf_inc_dir="${THIRD_PARTY_DIR}/protobuf/include"

if [[ ! -e $protoc_exe ]]; then
    echo "$protoc_exe does not exist!"
    exit
fi

if [[ ! -e $grpc_cpp_plugin_exe ]]; then
    echo "$grpc_cpp_plugin_exe does not exist!"
    exit
fi

if [[ ! -e $turbolink_plugin_exe ]]; then
    echo "$turbolink_plugin_exe does not exist!"
    exit
fi

if [[ ! -d $protobuf_inc_dir ]]; then
    echo "$protobuf_inc_dir is not a directory!"
    exit
fi

# Run protoc codegen with plugins
"${protoc_exe}" \
    --proto_path="${protobuf_inc_dir}" --proto_path="${input_proto_dir}" \
    --cpp_out="${cpp_output_dir}" \
    --plugin=protoc-gen-grpc="${grpc_cpp_plugin_exe}" --grpc_out="${cpp_output_dir}" \
    --plugin=protoc-gen-turbolink="${turbolink_plugin_exe}" --turbolink_out="${turbolink_output_dir}" \
    --turbolink_opt="GenerateJsonCode=true" \
    "${input_proto_file}"

# ====================================================================================

# Add pragma fixes to pb files
fix_proto_cpp="${TOOLS_DIR}/fix_proto_cpp.txt"
fix_proto_h="${TOOLS_DIR}/fix_proto_h.txt"

for file in ${cpp_output_dir}/${input_proto_service}/*.h;do
    if [ -f "$file" ]; then
        cat ${fix_proto_h} $file > "${file}.temp"
        mv "${file}.temp" $file
    fi
done

for file in ${cpp_output_dir}/${input_proto_service}/*.cc;do
    if [ -f "$file" ]; then
        cat ${fix_proto_cpp} $file > "${file}.temp"
        mv "${file}.temp" $file
    fi
done

