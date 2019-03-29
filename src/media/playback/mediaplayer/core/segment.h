// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_MEDIA_PLAYBACK_MEDIAPLAYER_CORE_SEGMENT_H_
#define SRC_MEDIA_PLAYBACK_MEDIAPLAYER_CORE_SEGMENT_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/media/playback/cpp/fidl.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include "src/lib/fxl/logging.h"
#include "src/media/playback/mediaplayer/graph/graph.h"
#include "src/media/playback/mediaplayer/graph/metadata.h"

namespace media_player {

// A graph segment.
//
// A graph segment is initially unprovisioned, meaning that the |graph| and
// |dispatcher| methods may not be called, and |provisioned| returns false.
// When it's provisioned, the |DidProvision| method is called, at which time
// the |graph| and |dispatcher| methods are valid to call, and |provisioned|
// returns true. Before the segment is deprovisioned, the |WillDeprovision|
// method is called.
class Segment {
 public:
  Segment();

  virtual ~Segment();

  // Provides the graph and task runner for this segment. |update_callback|
  // is optional and is called whenever the player should reinterrogate the
  // segment for state changes. The update callback is used to notify of changes
  // to the value returned by problem(). Subclasses of Segment may use this
  // callback to signal additional changes.
  void Provision(Graph* graph, async_dispatcher_t* dispatcher,
                 fit::closure update_callback);

  // Revokes the graph, task runner and update callback provided in a previous
  // call to |Provision|.
  virtual void Deprovision();

  // Indicates whether the segment is provisioned.
  bool provisioned() const { return graph_ != nullptr; }

  // Sets the update callback. |update_callback| may be null.
  void SetUpdateCallback(fit::closure update_callback);

  // Returns the current problem preventing intended operation or nullptr if
  // there is no such problem.
  const fuchsia::media::playback::Problem* problem() const {
    return problem_.get();
  }

 protected:
  Graph& graph() {
    FXL_DCHECK(graph_) << "graph() called on unprovisioned segment.";
    return *graph_;
  }

  async_dispatcher_t* dispatcher() {
    FXL_DCHECK(dispatcher_) << "dispatcher() called on unprovisioned segment.";
    return dispatcher_;
  }

  // Notifies the player of state updates (calls the update callback).
  void NotifyUpdate() const;

  // Report a problem.
  void ReportProblem(const std::string& type, const std::string& details);

  // Clear any prior problem report.
  void ReportNoProblem();

  // Called when the segment has been provisioned. The default implementation
  // does nothing.
  virtual void DidProvision() {}

  // Called when the segment is about to be deprovisioned. The default
  // implementation does nothing.
  virtual void WillDeprovision() {}

 private:
  Graph* graph_ = nullptr;
  async_dispatcher_t* dispatcher_ = nullptr;
  fit::closure update_callback_;
  fuchsia::media::playback::ProblemPtr problem_;
};

}  // namespace media_player

#endif  // SRC_MEDIA_PLAYBACK_MEDIAPLAYER_CORE_SEGMENT_H_
