// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <fcntl.h>
#include <unistd.h>

#include <fbl/macros.h>
#include <fbl/unique_fd.h>
#include <fbl/unique_ptr.h>
#include <utility>
#include <zircon/types.h>

// Namespace these classes to prevent collisions with minfs's FileWrapper.
namespace fvm::host {

// FileWrapper exposes a file-like interface to abstract away the actual
// file system implementation. Interface matches POSIX functions of the same
// name.
class FileWrapper {
public:
    virtual ~FileWrapper() {}
    virtual ssize_t Read(void* buffer, size_t count) = 0;
    virtual ssize_t Write(void* buffer, size_t count) = 0;
    virtual ssize_t Seek(off_t offset, int whence) = 0;
    virtual ssize_t Size() = 0;
    virtual ssize_t Tell() = 0;

    // Truncates file to |size| bytes.
    // Returns ZX_OK on success, ZX_ERR_NOT_SUPPORTED if truncate is
    // not supported, or -1 if truncate operation fails.
    virtual zx_status_t Truncate(size_t size) = 0;
};

// Implementation of FileWrapper that wraps a raw file descripter.
// File descriptor is expected to refer to a file that is compatible
// with standard POSIX file functions, e.g. read, write, ftruncate, etc.
class FdWrapper : public FileWrapper {
public:
    FdWrapper(int fd)
        : fd_(fd) {}

    ssize_t Read(void* buffer, size_t count) override;
    ssize_t Write(void* buffer, size_t count) override;
    ssize_t Seek(off_t offset, int whence) override;
    ssize_t Size() override;
    ssize_t Tell() override;
    zx_status_t Truncate(size_t size) override;

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FdWrapper);

private:
    int fd_;
};

// Implementation of FileWrapper that takes ownership of a unique_fd.
// File descriptor is expected to refer to a file that is compatible
// with standard POSIX file functions, e.g. read, write, ftruncate, etc.
class UniqueFdWrapper : public FileWrapper {
public:
    static zx_status_t Open(
        const char* path, int flags, mode_t mode, fbl::unique_ptr<UniqueFdWrapper>* out);
    UniqueFdWrapper(fbl::unique_fd fd)
        : fd_(std::move(fd)) {}

    ssize_t Read(void* buffer, size_t count) override;
    ssize_t Write(void* buffer, size_t count) override;
    ssize_t Seek(off_t offset, int whence) override;
    ssize_t Size() override;
    ssize_t Tell() override;
    zx_status_t Truncate(size_t size) override;

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(UniqueFdWrapper);

private:
    fbl::unique_fd fd_;
};

} // namespace fvm::host
