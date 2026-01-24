#include "remote_jolt.hpp"

#include "filesystem.hpp"

#include <cstdio>
#include <fstream>

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>

namespace fstree {

remote_jolt::remote_jolt(const url& address) {
  grpc::ChannelArguments args;

  // Set the default compression algorithm for the channel.
  args.SetCompressionAlgorithm(GRPC_COMPRESS_GZIP);

  // Set service config with retry policy for the channel.
  args.SetServiceConfigJSON(R"(
    {
      "methodConfig": [
        {
          "name": [
            { "service": "fstree.CacheService" }
          ],
          "waitForReady": true,
          "retryPolicy": {
            "maxAttempts": 5,
            "initialBackoff": "0.1s",
            "maxBackoff": "30s",
            "backoffMultiplier": 2,
            "retryableStatusCodes": [ "UNAVAILABLE" ]
          }
        }
      ]
    }
  )");

  auto channel_creds = grpc::InsecureChannelCredentials();
  _channel = grpc::CreateCustomChannel(address.host(), channel_creds, args);
  _client = fstree::CacheService::NewStub(_channel);
}

void remote_jolt::write_object(const std::string& hash, const std::filesystem::path& path) {
  // Set up gRPC client
  grpc::ClientContext context;

  // Create a blob write request message
  fstree::WriteObjectRequest request;
  request.set_digest(hash);

  // Create a blob write response message
  fstree::WriteObjectResponse response;

  // Open the file
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("failed to open cache object: " + path.string());
  }

  // Call the RPC
  auto writer = _client->WriteObject(&context, &response);

  // Read the file into the request, 64KB at a time
  char buffer[64 * 1024];
  do {
    file.read(buffer, sizeof(buffer));
    request.set_data(buffer, file.gcount());

    // Send the request
    if (!writer->Write(request)) {
      break;
    }
  } while (file);

  // Finish the RPC
  writer->WritesDone();

  grpc::Status status = writer->Finish();

  if (!status.ok()) {
    if (status.error_code() != grpc::StatusCode::ALREADY_EXISTS) {
      throw std::runtime_error("failed to write cache object: " + path.string() + ": " + status.error_message());
    }
  }
}

void remote_jolt::read_object(
    const std::string& hash, const std::filesystem::path& path, const std::filesystem::path& temp) {
  // Set up gRPC client
  grpc::ClientContext context;

  // Create a blob read request message
  fstree::ReadObjectRequest request;
  request.set_digest(hash);

  // Create a blob read response message
  fstree::ReadObjectResponse response;

  // Create a temporary file using mkstemp
  std::filesystem::path temp_path = temp;
  FILE* file = fstree::mkstemp(temp_path);
  if (!file) {
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to create temporary file: " + temp_path.string() + ": " + std::strerror(errno));
  }
  // Call the RPC
  auto reader = _client->ReadObject(&context, request);

  // Read the blob data
  while (reader->Read(&response)) {
    size_t ret = fwrite(response.data().data(), 1, response.data().size(), file);
    if (ret != response.data().size()) {
      fclose(file);
      std::filesystem::remove(temp_path);
      throw std::runtime_error(
          "failed to write to temporary file: " + temp_path.string() + ": " + std::strerror(errno));
    }
  }

  grpc::Status status = reader->Finish();
  if (!status.ok()) {
    fclose(file);
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to read cache object: " + hash + ": " + status.error_message());
  }

  fclose(file);

  // Move the temporary file to the final path
  std::error_code ec;
  if (!std::filesystem::exists(path.parent_path(), ec)) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      std::filesystem::remove(temp_path);
      throw std::runtime_error("failed to create directory: " + path.parent_path().string() + ": " + ec.message());
    }
  }

  // Rename the temporary file to the final path
  std::filesystem::rename(temp_path, path, ec);
  if (ec) {
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to rename temporary file: " + temp_path.string() + ": " + ec.message());
  }
}

void remote_jolt::has_tree(
    const std::string& hash, std::vector<std::string>& missing_trees, std::vector<std::string>& missing_objects) {
  // Set up gRPC client
  grpc::ClientContext context;

  // Create a HasTree request message
  fstree::HasTreeRequest request;
  request.add_digest(hash);

  // Create a response message
  fstree::HasTreeResponse response;

  // Call the RPC
  auto server = _client->HasTree(&context, request);

  // Read the responses
  while (server->Read(&response)) {
    // Copy the missing trees and objects vectors
    for (int i = 0; i < response.missing_trees_size(); i++) {
      missing_trees.push_back(response.missing_trees(i));
    }
    for (int i = 0; i < response.missing_objects_size(); i++) {
      missing_objects.push_back(response.missing_objects(i));
    }
  }

  auto status = server->Finish();
  if (!status.ok()) {
    throw std::runtime_error("failed to lookup tree in remote cache: " + status.error_message());
  }
}

bool remote_jolt::has_object(const std::string& hash) {
  // call has_objects with a vector of one hash
  std::vector<std::string> hashes = {hash};
  std::vector<bool> presence;
  has_objects(hashes, presence);
  return presence[0];
}

void remote_jolt::has_objects(const std::vector<std::string>& hashes, std::vector<bool>& presence) {
  // Set up gRPC client
  grpc::ClientContext context;

  // Create a blob has request message
  fstree::HasObjectRequest request;
  for (const auto& hash : hashes) {
    request.add_digest(hash);
  }

  // Create a blob has response message
  fstree::HasObjectResponse response;

  // Call the RPC
  grpc::Status status = _client->HasObject(&context, request, &response);
  if (!status.ok()) {
    throw std::runtime_error("failed to lookup objects in remote cache: " + status.error_message());
  }

  // Copy the presence vector
  for (int i = 0; i < response.present_size(); i++) {
    presence.push_back(response.present(i));
  }
}

}  // namespace fstree
