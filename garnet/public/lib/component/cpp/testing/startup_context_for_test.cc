// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/component/cpp/testing/startup_context_for_test.h"

#include <lib/async/default.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/fdio.h>
#include <lib/fdio/directory.h>

namespace component {
namespace testing {

StartupContextForTest::StartupContextForTest(
    zx::channel service_root_client, zx::channel service_root_server,
    zx::channel directory_request_client, zx::channel directory_request_server)
    : StartupContext(std::move(service_root_client),
                     std::move(directory_request_server)),
      controller_(this),
      service_root_vfs_(async_get_default_dispatcher()),
      service_root_dir_(fbl::AdoptRef(new fs::PseudoDir())) {
  outgoing_public_services_.Bind(
      ChannelConnectAt(directory_request_client.get(), "public"));

  // TODO(CP-57): simplify this
  zx_status_t status = controller_.AddService(
      fbl::AdoptRef(new fs::Service([&](zx::channel channel) {
        fake_launcher_.Bind(
            fidl::InterfaceRequest<fuchsia::sys::Launcher>(std::move(channel)));
        return ZX_OK;
      })),
      FakeLauncher::Name_);
  ZX_ASSERT(status == ZX_OK);

  status = service_root_vfs_.ServeDirectory(service_root_dir_,
                                            std::move(service_root_server));
  ZX_ASSERT(status == ZX_OK);
}

std::unique_ptr<StartupContextForTest> StartupContextForTest::Create() {
  // TODO(CP-46): implement /svc instrumentation
  zx::channel service_root_client, service_root_server;
  zx_status_t status =
      zx::channel::create(0, &service_root_client, &service_root_server);
  ZX_ASSERT(status == ZX_OK);

  zx::channel directory_request_client, directory_request_server;
  status = zx::channel::create(0, &directory_request_client,
                               &directory_request_server);
  ZX_ASSERT(status == ZX_OK);

  return std::make_unique<StartupContextForTest>(
      std::move(service_root_client), std::move(service_root_server),
      std::move(directory_request_client), std::move(directory_request_server));
}

zx::channel StartupContextForTest::ChannelConnectAt(zx_handle_t root,
                                                    const char* path) {
  zx::channel client, server;
  zx_status_t status = zx::channel::create(0, &client, &server);
  ZX_ASSERT(status == ZX_OK);

  status = fdio_service_connect_at(root, path, server.release());
  ZX_ASSERT(status == ZX_OK);

  return client;
}

}  // namespace testing
}  // namespace component
