#ifdef FSTREE_ENABLE_JOLT_REMOTE

#include "remote_jolt.hpp"

#include "filesystem.hpp"
#include "hash.hpp"
#include "service_auth.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>

namespace fstree {

remote_jolt::remote_jolt(const url& address) : _address(address.string()) {
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

  // tls:// URIs use TLS, and only TLS connections get the token, unless
  // sending it without TLS is allowed
  auto auth = service_auth::from_environment();
  std::shared_ptr<grpc::ChannelCredentials> channel_creds;
  if (address.scheme() == "tls") {
    grpc::SslCredentialsOptions ssl_options;
    ssl_options.pem_root_certs = auth.ca_cert_pem();
    channel_creds = grpc::SslCredentials(ssl_options);
    const std::string& token = auth.token(true, address.string());
    if (!token.empty()) {
      channel_creds = grpc::CompositeChannelCredentials(channel_creds, grpc::AccessTokenCredentials(token));
    }
  } else {
    channel_creds = grpc::InsecureChannelCredentials();
    _insecure_token = auth.token(false, address.string());
  }
  _channel = grpc::CreateCustomChannel(address.host(), channel_creds, args);
  _client = fstree::CacheService::NewStub(_channel);
}

void remote_jolt::connect() {
  // Not std::call_once, which can't pass on exceptions with all standard libraries
  std::lock_guard<std::mutex> lock(_connect_mutex);
  if (_connected) {
    return;
  }

  int timeout = 60;
  if (const char* value = std::getenv("FSTREE_CONNECT_TIMEOUT")) {
    timeout = std::atoi(value);
  }
  if (_channel->WaitForConnected(std::chrono::system_clock::now() + std::chrono::seconds(timeout))) {
    _connected = true;
    return;
  }

  // A call that doesn't wait for the server to be ready tells why the
  // channel isn't
  grpc::ClientContext context;
  authenticate(context);
  context.set_wait_for_ready(false);
  context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
  fstree::HasObjectRequest request;
  fstree::HasObjectResponse response;
  auto status = _client->HasObject(&context, request, &response);
  if (status.error_code() == grpc::StatusCode::UNAVAILABLE ||
      status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
    throw std::runtime_error("failed to connect to " + _address + ": " + status.error_message());
  }
  _connected = true;
}

void remote_jolt::authenticate(grpc::ClientContext& context) const {
  if (!_insecure_token.empty()) {
    context.AddMetadata("authorization", "Bearer " + _insecure_token);
  }
}

void remote_jolt::write_object(const fstree::digest& hash, const std::filesystem::path& path) {
  connect();

  // Set up gRPC client
  grpc::ClientContext context;
  authenticate(context);

  // Create a blob write request message
  fstree::WriteObjectRequest request;
  request.set_digest(hash.string());

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
    const fstree::digest& hash, const std::filesystem::path& path, const std::filesystem::path& temp) {
  connect();

  // Set up gRPC client
  grpc::ClientContext context;
  authenticate(context);

  // Create a blob read request message
  fstree::ReadObjectRequest request;
  request.set_digest(hash.string());

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
      int err = errno;
      fclose(file);
      std::filesystem::remove(temp_path);

      // Abandon the call so the service stops sending the rest
      context.TryCancel();
      reader->Finish();

      throw std::runtime_error(
          "failed to write to temporary file: " + temp_path.string() + ": " + std::strerror(err));
    }
  }

  grpc::Status status = reader->Finish();
  if (!status.ok()) {
    fclose(file);
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to read cache object: " + hash.string() + ": " + status.error_message());
  }

  // Closing flushes what fwrite buffered, which for a small object is all of it
  if (fclose(file) != 0) {
    int err = errno;
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to write to temporary file: " + temp_path.string() + ": " + std::strerror(err));
  }

  // The service checks objects when they are written, but they may have
  // been damaged in its storage since.
  verify_download(hash, temp_path);

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

void remote_jolt::has_tree(const std::vector<fstree::digest>& hashes, const missing_callback& callback) {
  connect();

  // Set up gRPC client
  grpc::ClientContext context;
  authenticate(context);

  // Create a HasTree request message
  fstree::HasTreeRequest request;
  for (const auto& hash : hashes) {
    request.add_digest(hash.string());
  }

  // Create a response message
  fstree::HasTreeResponse response;

  // Call the RPC
  auto server = _client->HasTree(&context, request);

  // Pass on the missing trees and objects of each response as it arrives
  try {
    while (server->Read(&response)) {
      std::vector<fstree::digest> missing_trees, missing_objects;
      for (int i = 0; i < response.missing_trees_size(); i++) {
        missing_trees.push_back(fstree::digest::parse(response.missing_trees(i)));
      }
      for (int i = 0; i < response.missing_objects_size(); i++) {
        missing_objects.push_back(fstree::digest::parse(response.missing_objects(i)));
      }
      callback(missing_trees, missing_objects);
    }
  }
  catch (...) {
    // Abandon the call before passing on the error
    context.TryCancel();
    server->Finish();
    throw;
  }

  auto status = server->Finish();
  if (!status.ok()) {
    throw std::runtime_error("failed to lookup tree in remote cache: " + status.error_message());
  }
}

bool remote_jolt::has_object(const fstree::digest& hash) {
  // call has_objects with a vector of one hash
  std::vector<fstree::digest> hashes = {hash};
  std::vector<bool> presence;
  has_objects(hashes, presence);
  return presence[0];
}

void remote_jolt::has_objects(const std::vector<fstree::digest>& hashes, std::vector<bool>& presence) {
  connect();

  // Set up gRPC client
  grpc::ClientContext context;
  authenticate(context);

  // Create a blob has request message
  fstree::HasObjectRequest request;
  for (const auto& hash : hashes) {
    request.add_digest(hash.string());
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

#endif // FSTREE_ENABLE_JOLT_REMOTE
