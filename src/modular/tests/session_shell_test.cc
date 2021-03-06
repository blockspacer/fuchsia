// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/modular/testing/cpp/fidl.h>
#include <lib/fsl/vmo/strings.h>
#include <lib/modular_test_harness/cpp/fake_component.h>
#include <lib/modular_test_harness/cpp/test_harness_fixture.h>
#include <peridot/lib/modular_config/modular_config.h>
#include <peridot/lib/modular_config/modular_config_constants.h>
#include <peridot/lib/testing/session_shell_base.h>
#include <sdk/lib/sys/cpp/component_context.h>
#include <sdk/lib/sys/cpp/service_directory.h>
#include <sdk/lib/sys/cpp/testing/test_with_environment.h>

#include "gmock/gmock.h"

using fuchsia::modular::AddMod;
using fuchsia::modular::StoryCommand;
using fuchsia::modular::StoryInfo;
using fuchsia::modular::StoryState;
using fuchsia::modular::StoryVisibilityState;
using fuchsia::modular::ViewIdentifier;
using testing::IsNull;
using testing::Not;

constexpr char kFakeModuleUrl[] =
    "fuchsia-pkg://example.com/FAKE_MODULE_PKG/fake_module.cmx";

namespace {

// Session shell mock that provides access to the StoryProvider and the
// SessionShellContext. Also acts as a StoryProviderWatcher, and will run a
// lambda function that can be provided by a test when it sees a change in story
// state.
//
// TODO(MF-386): Factor this out into a generally usable mock session shell.
class MockSessionShell : public modular::testing::FakeComponent,
                         fuchsia::modular::StoryProviderWatcher {
 public:
  MockSessionShell() : story_provider_watcher_(this) {}

  fuchsia::modular::StoryProvider* GetStoryProvider() {
    return story_provider_.get();
  }

  fuchsia::modular::SessionShellContext* GetSessionShellContext() {
    return session_shell_context_.get();
  }

  modular::testing::SessionShellImpl* GetSessionShellImpl() {
    return &session_shell_impl_;
  }

  using OnChangeFunction =
      fit::function<void(StoryInfo, StoryState, StoryVisibilityState)>;

  void SetOnChange(OnChangeFunction on_change) {
    on_change_ = std::move(on_change);
  }

 private:
  // |modular::testing::FakeComponent|
  void OnCreate(fuchsia::sys::StartupInfo startup_info) override {
    component_context()->svc()->Connect(session_shell_context_.NewRequest());
    session_shell_context_->GetStoryProvider(story_provider_.NewRequest());

    component_context()->outgoing()->AddPublicService(
        session_shell_impl_.GetHandler());

    story_provider_->GetStories(
        story_provider_watcher_.NewBinding(),
        [](std::vector<fuchsia::modular::StoryInfo> stories) {});
  }

  // |fuchsia::modular::StoryProviderWatcher|
  void OnChange(StoryInfo story_info, StoryState story_state,
                StoryVisibilityState story_visibility_state) override {
    on_change_(std::move(story_info), story_state, story_visibility_state);
  }

  // |fuchsia::modular::StoryProviderWatcher|
  void OnDelete(std::string story_id) override {}

  fidl::Binding<StoryProviderWatcher> story_provider_watcher_;
  // Optional user-provided lambda that will run with each OnChange(). Defaults
  // to doing nothing.
  OnChangeFunction on_change_ =
      [](StoryInfo story_info, StoryState story_state,
         StoryVisibilityState story_visibility_state) {};

  modular::testing::SessionShellImpl session_shell_impl_;
  fuchsia::modular::SessionShellContextPtr session_shell_context_;
  fuchsia::modular::StoryProviderPtr story_provider_;
};

class SessionShellTest : public modular::testing::TestHarnessFixture {
 protected:
  // Shared boilerplate for configuring the test harness to intercept the
  // session shell, setting up the session shell mock object, running the test
  // harness, and waiting for the session shell to be successfully intercepted.
  // Note that this method blocks the thread until the session shell has started
  // up.
  //
  // Not done in SetUp() or the constructor to let the test reader know that
  // this is happening. Also, certain tests may want to change this flow.
  void RunHarnessAndInterceptSessionShell() {
    modular::testing::TestHarnessBuilder builder;
    builder.InterceptSessionShell(
        mock_session_shell_.GetOnCreateHandler(),
        {.sandbox_services = {"fuchsia.modular.SessionShellContext",
                              "fuchsia.modular.PuppetMaster"}});

    test_harness().events().OnNewComponent =
        builder.BuildOnNewComponentHandler();
    test_harness()->Run(builder.BuildSpec());

    // Wait for our session shell to start.
    RunLoopUntil([&] { return mock_session_shell_.is_running(); });
  }

  MockSessionShell mock_session_shell_;
};

TEST_F(SessionShellTest, DISABLED_GetPackageName) {
  fuchsia::modular::testing::TestHarnessSpec spec;
  test_harness()->Run(std::move(spec));

  fuchsia::modular::ComponentContextPtr component_context;
  fuchsia::modular::testing::TestHarnessService svc;
  svc.set_component_context(component_context.NewRequest());
  test_harness()->GetService(std::move(svc));

  bool got_name = false;
  component_context->GetPackageName([&got_name](fidl::StringPtr name) {
    EXPECT_THAT(name, Not(IsNull()));
    got_name = true;
  });

  ASSERT_TRUE(RunLoopWithTimeoutOrUntil([&] { return got_name; }, zx::sec(10)));
}

TEST_F(SessionShellTest, DISABLED_GetStoryInfoNonexistentStory) {
  RunHarnessAndInterceptSessionShell();

  fuchsia::modular::StoryProvider* story_provider =
      mock_session_shell_.GetStoryProvider();
  ASSERT_TRUE(story_provider != nullptr);

  bool tried_get_story_info = false;
  story_provider->GetStoryInfo(
      "X", [&tried_get_story_info](fuchsia::modular::StoryInfoPtr story_info) {
        EXPECT_THAT(story_info, IsNull());
        tried_get_story_info = true;
      });

  ASSERT_TRUE(RunLoopWithTimeoutOrUntil([&] { return tried_get_story_info; },
                                        zx::sec(10)));
}

TEST_F(SessionShellTest, DISABLED_GetLink) {
  RunHarnessAndInterceptSessionShell();

  fuchsia::modular::SessionShellContext* session_shell_context;
  session_shell_context = mock_session_shell_.GetSessionShellContext();
  ASSERT_TRUE(session_shell_context != nullptr);

  fuchsia::modular::LinkPtr session_shell_link;
  session_shell_context->GetLink(session_shell_link.NewRequest());
  bool called_get_link = false;
  session_shell_link->Get(
      nullptr, [&called_get_link](std::unique_ptr<fuchsia::mem::Buffer> value) {
        called_get_link = true;
      });

  ASSERT_TRUE(
      RunLoopWithTimeoutOrUntil([&] { return called_get_link; }, zx::sec(10)));
}

TEST_F(SessionShellTest, DISABLED_GetStoriesEmpty) {
  RunHarnessAndInterceptSessionShell();

  fuchsia::modular::StoryProvider* story_provider =
      mock_session_shell_.GetStoryProvider();
  ASSERT_TRUE(story_provider != nullptr);

  bool called_get_stories = false;
  story_provider->GetStories(
      nullptr,
      [&called_get_stories](std::vector<fuchsia::modular::StoryInfo> stories) {
        EXPECT_THAT(stories, testing::IsEmpty());
        called_get_stories = true;
      });

  ASSERT_TRUE(RunLoopWithTimeoutOrUntil([&] { return called_get_stories; },
                                        zx::sec(10)));
}

TEST_F(SessionShellTest, DISABLED_StartAndStopStoryWithExtraInfoMod) {
  RunHarnessAndInterceptSessionShell();

  // Create a new story using PuppetMaster and launch a new story shell,
  // including a mod with extra info.
  fuchsia::modular::PuppetMasterPtr puppet_master;
  fuchsia::modular::StoryPuppetMasterPtr story_master;

  fuchsia::modular::testing::TestHarnessService svc;
  svc.set_puppet_master(puppet_master.NewRequest());
  test_harness()->GetService(std::move(svc));

  fuchsia::modular::StoryProvider* story_provider =
      mock_session_shell_.GetStoryProvider();
  ASSERT_TRUE(story_provider != nullptr);
  const char kStoryId[] = "my_story";

  // Have the mock session_shell record the sequence of story states it sees,
  // and confirm that it only sees the correct story id.
  std::vector<StoryState> sequence_of_story_states;
  mock_session_shell_.SetOnChange(
      [&sequence_of_story_states, kStoryId](StoryInfo story_info,
                                            StoryState story_state,
                                            StoryVisibilityState _) {
        EXPECT_EQ(story_info.id, kStoryId);
        sequence_of_story_states.push_back(story_state);
      });
  puppet_master->ControlStory(kStoryId, story_master.NewRequest());

  AddMod add_mod;
  add_mod.mod_name_transitional = "mod1";
  add_mod.intent.handler = kFakeModuleUrl;
  fuchsia::modular::IntentParameter param;
  param.name = "root";
  fsl::SizedVmo vmo;
  const std::string initial_json = R"({"created-with-info": true})";
  ASSERT_TRUE(fsl::VmoFromString(initial_json, &vmo));
  param.data.set_json(std::move(vmo).ToTransport());
  add_mod.intent.parameters.push_back(std::move(param));

  StoryCommand command;
  command.set_add_mod(std::move(add_mod));

  std::vector<StoryCommand> commands;
  commands.push_back(std::move(command));

  story_master->Enqueue(std::move(commands));
  bool execute_called = false;
  story_master->Execute(
      [&execute_called](fuchsia::modular::ExecuteResult result) {
        execute_called = true;
      });
  ASSERT_TRUE(
      RunLoopWithTimeoutOrUntil([&] { return execute_called; }, zx::sec(10)));

  // Stop the story. Check that the story went through the correct sequence
  // of states (see StoryState FIDL file for valid state transitions). Since we
  // started it, ran it, and stopped it, the sequence is STOPPED -> RUNNING ->
  // STOPPING -> STOPPED.
  fuchsia::modular::StoryControllerPtr story_controller;
  story_provider->GetController(kStoryId, story_controller.NewRequest());
  bool stop_called = false;
  story_controller->Stop([&stop_called] { stop_called = true; });
  ASSERT_TRUE(
      RunLoopWithTimeoutOrUntil([&] { return stop_called; }, zx::sec(10)));
  // Run the loop until there are the expected number of state changes;
  // having called Stop() is not enough to guarantee seeing all updates.
  ASSERT_TRUE(RunLoopWithTimeoutOrUntil(
      [&] { return sequence_of_story_states.size() == 4; }, zx::sec(10)));
  EXPECT_THAT(sequence_of_story_states,
              testing::ElementsAre(StoryState::STOPPED, StoryState::RUNNING,
                                   StoryState::STOPPING, StoryState::STOPPED));
}

TEST_F(SessionShellTest, DISABLED_StoryInfoBeforeAndAfterDelete) {
  RunHarnessAndInterceptSessionShell();

  // Create a new story using PuppetMaster and launch a new story shell.
  fuchsia::modular::PuppetMasterPtr puppet_master;
  fuchsia::modular::StoryPuppetMasterPtr story_master;

  fuchsia::modular::testing::TestHarnessService svc;
  svc.set_puppet_master(puppet_master.NewRequest());
  test_harness()->GetService(std::move(svc));

  fuchsia::modular::StoryProvider* story_provider =
      mock_session_shell_.GetStoryProvider();
  ASSERT_TRUE(story_provider != nullptr);
  const char kStoryId[] = "my_story";
  puppet_master->ControlStory(kStoryId, story_master.NewRequest());

  AddMod add_mod;
  add_mod.mod_name_transitional = "mod1";
  add_mod.intent.handler = kFakeModuleUrl;

  StoryCommand command;
  command.set_add_mod(std::move(add_mod));

  std::vector<StoryCommand> commands;
  commands.push_back(std::move(command));

  story_master->Enqueue(std::move(commands));

  bool execute_and_get_story_info_called = false;
  story_master->Execute(
      [&execute_and_get_story_info_called, kStoryId,
       story_provider](fuchsia::modular::ExecuteResult result) {
        // Verify that the newly created story returns something for
        // GetStoryInfo().
        story_provider->GetStoryInfo(
            kStoryId, [&execute_and_get_story_info_called,
                       kStoryId](fuchsia::modular::StoryInfoPtr story_info) {
              ASSERT_THAT(story_info, Not(IsNull()));
              EXPECT_EQ(story_info->id, kStoryId);
              execute_and_get_story_info_called = true;
            });
      });
  ASSERT_TRUE(RunLoopWithTimeoutOrUntil(
      [&] { return execute_and_get_story_info_called; }, zx::sec(10)));

  // Delete the story and confirm that the story info is null now.
  bool delete_called = false;
  puppet_master->DeleteStory(
      kStoryId, [&delete_called, kStoryId, story_provider] {
        story_provider->GetStoryInfo(
            kStoryId, [](fuchsia::modular::StoryInfoPtr story_info) {
              EXPECT_THAT(story_info, IsNull());
            });
        delete_called = true;
      });
  ASSERT_TRUE(
      RunLoopWithTimeoutOrUntil([&] { return delete_called; }, zx::sec(10)));
}

TEST_F(SessionShellTest, DISABLED_KindOfProtoStoryNotInStoryList) {
  RunHarnessAndInterceptSessionShell();

  // Create a new story using PuppetMaster and launch a new story shell,
  // adding the kind of proto option.
  fuchsia::modular::PuppetMasterPtr puppet_master;
  fuchsia::modular::StoryPuppetMasterPtr story_master;

  fuchsia::modular::testing::TestHarnessService svc;
  svc.set_puppet_master(puppet_master.NewRequest());
  test_harness()->GetService(std::move(svc));

  fuchsia::modular::StoryProvider* story_provider =
      mock_session_shell_.GetStoryProvider();
  ASSERT_TRUE(story_provider != nullptr);

  const char kStoryId[] = "my_story";
  puppet_master->ControlStory(kStoryId, story_master.NewRequest());

  fuchsia::modular::StoryOptions story_options;
  story_options.kind_of_proto_story = true;
  story_master->SetCreateOptions(std::move(story_options));

  bool called_get_stories = false;
  story_master->Execute([&called_get_stories, story_provider](
                            fuchsia::modular::ExecuteResult result) {
    // Confirm that even after the story is created, GetStories() returns
    // empty.
    story_provider->GetStories(
        nullptr, [&called_get_stories](
                     std::vector<fuchsia::modular::StoryInfo> stories) {
          EXPECT_THAT(stories, testing::IsEmpty());
          called_get_stories = true;
        });
  });

  ASSERT_TRUE(RunLoopWithTimeoutOrUntil([&] { return called_get_stories; },
                                        zx::sec(10)));
}

TEST_F(SessionShellTest, DISABLED_AttachesAndDetachesView) {
  RunHarnessAndInterceptSessionShell();

  // Create a new story using PuppetMaster and start a new story shell.
  // Confirm that AttachView() is called.
  fuchsia::modular::PuppetMasterPtr puppet_master;
  fuchsia::modular::StoryPuppetMasterPtr story_master;

  fuchsia::modular::testing::TestHarnessService svc;
  svc.set_puppet_master(puppet_master.NewRequest());
  test_harness()->GetService(std::move(svc));

  fuchsia::modular::StoryProvider* story_provider =
      mock_session_shell_.GetStoryProvider();
  ASSERT_TRUE(story_provider != nullptr);

  const char kStoryId[] = "my_story";
  // Have the mock session_shell record the sequence of story states it sees,
  // and confirm that it only sees the correct story id.
  std::vector<StoryState> sequence_of_story_states;
  mock_session_shell_.SetOnChange(
      [&sequence_of_story_states, kStoryId](StoryInfo story_info,
                                            StoryState story_state,
                                            StoryVisibilityState _) {
        EXPECT_EQ(story_info.id, kStoryId);
        sequence_of_story_states.push_back(story_state);
      });
  puppet_master->ControlStory(kStoryId, story_master.NewRequest());

  AddMod add_mod;
  add_mod.mod_name_transitional = "mod1";
  add_mod.intent.handler = kFakeModuleUrl;

  StoryCommand command;
  command.set_add_mod(std::move(add_mod));

  std::vector<StoryCommand> commands;
  commands.push_back(std::move(command));

  story_master->Enqueue(std::move(commands));
  story_master->Execute([](fuchsia::modular::ExecuteResult result) {});

  bool called_attach_view = false;
  mock_session_shell_.GetSessionShellImpl()->set_on_attach_view(
      [&called_attach_view](ViewIdentifier) { called_attach_view = true; });

  ASSERT_TRUE(RunLoopWithTimeoutOrUntil([&] { return called_attach_view; },
                                        zx::sec(10)));

  // Stop the story. Confirm that:
  //  a. DetachView() was called.
  //  b. The story went through the correct sequence
  // of states (see StoryState FIDL file for valid state transitions). Since we
  // started it, ran it, and stopped it, the sequence is STOPPED -> RUNNING ->
  // STOPPING -> STOPPED.
  bool called_detach_view = false;
  mock_session_shell_.GetSessionShellImpl()->set_on_detach_view(
      [&called_detach_view](ViewIdentifier) { called_detach_view = true; });
  fuchsia::modular::StoryControllerPtr story_controller;
  story_provider->GetController(kStoryId, story_controller.NewRequest());
  bool stop_called = false;
  story_controller->Stop([&stop_called] { stop_called = true; });
  ASSERT_TRUE(
      RunLoopWithTimeoutOrUntil([&] { return stop_called; }, zx::sec(10)));
  // Run the loop until there are the expected number of state changes;
  // having called Stop() is not enough to guarantee seeing all updates.
  ASSERT_TRUE(RunLoopWithTimeoutOrUntil(
      [&] { return sequence_of_story_states.size() == 4; }, zx::sec(10)));
  EXPECT_TRUE(called_detach_view);
  EXPECT_THAT(sequence_of_story_states,
              testing::ElementsAre(StoryState::STOPPED, StoryState::RUNNING,
                                   StoryState::STOPPING, StoryState::STOPPED));
}

TEST_F(SessionShellTest, DISABLED_StoryStopDoesntWaitOnDetachView) {
  RunHarnessAndInterceptSessionShell();

  // Create a new story using PuppetMaster and start a new story shell.
  // Confirm that AttachView() is called.
  fuchsia::modular::PuppetMasterPtr puppet_master;
  fuchsia::modular::StoryPuppetMasterPtr story_master;

  fuchsia::modular::testing::TestHarnessService svc;
  svc.set_puppet_master(puppet_master.NewRequest());
  test_harness()->GetService(std::move(svc));

  fuchsia::modular::StoryProvider* story_provider =
      mock_session_shell_.GetStoryProvider();
  ASSERT_TRUE(story_provider != nullptr);
  const char kStoryId[] = "my_story";

  // Have the mock session_shell record the sequence of story states it sees,
  // and confirm that it only sees the correct story id.
  std::vector<StoryState> sequence_of_story_states;
  mock_session_shell_.SetOnChange(
      [&sequence_of_story_states, kStoryId](StoryInfo story_info,
                                            StoryState story_state,
                                            StoryVisibilityState _) {
        EXPECT_EQ(story_info.id, kStoryId);
        sequence_of_story_states.push_back(story_state);
      });
  puppet_master->ControlStory(kStoryId, story_master.NewRequest());

  AddMod add_mod;
  add_mod.mod_name_transitional = "mod1";
  add_mod.intent.handler = kFakeModuleUrl;

  StoryCommand command;
  command.set_add_mod(std::move(add_mod));

  std::vector<StoryCommand> commands;
  commands.push_back(std::move(command));

  story_master->Enqueue(std::move(commands));
  story_master->Execute([](fuchsia::modular::ExecuteResult result) {});

  bool called_attach_view = false;
  mock_session_shell_.GetSessionShellImpl()->set_on_attach_view(
      [&called_attach_view](ViewIdentifier) { called_attach_view = true; });

  ASSERT_TRUE(RunLoopWithTimeoutOrUntil([&] { return called_attach_view; },
                                        zx::sec(10)));

  // Stop the story. Confirm that:
  //  a. The story stopped, even though it didn't see the DetachView() response
  //   (it was artificially delayed for 1hr).
  //  b. The story went through the correct sequence of states (see StoryState
  //   FIDL file for valid state transitions). Since we started it, ran it, and
  //   stopped it, the sequence is STOPPED -> RUNNING -> STOPPING -> STOPPED.
  mock_session_shell_.GetSessionShellImpl()->set_detach_delay(zx::sec(60 * 60));
  fuchsia::modular::StoryControllerPtr story_controller;
  story_provider->GetController(kStoryId, story_controller.NewRequest());
  bool stop_called = false;
  story_controller->Stop([&stop_called] { stop_called = true; });

  ASSERT_TRUE(
      RunLoopWithTimeoutOrUntil([&] { return stop_called; }, zx::sec(10)));
  // Run the loop until there are the expected number of state changes;
  // having called Stop() is not enough to guarantee seeing all updates.
  ASSERT_TRUE(RunLoopWithTimeoutOrUntil(
      [&] { return sequence_of_story_states.size() == 4; }, zx::sec(10)));
  EXPECT_THAT(sequence_of_story_states,
              testing::ElementsAre(StoryState::STOPPED, StoryState::RUNNING,
                                   StoryState::STOPPING, StoryState::STOPPED));
}

// TODO(MF-399): Add a test that ensures DetachView() is not called on logout.
// This will likely require mocking the base shell as well.
}  // namespace
