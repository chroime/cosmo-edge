#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "catch_amalgamated.hpp"
#include "flow/task/PTaskBase.h"
#include "util/dto/ActionCodes.h"

using namespace cosmo;

namespace {

constexpr std::array<std::string_view, 5> kActionCodes{PADetect_Code, PAClassify_Code, PALandmark_Code,
                                                       PARecognizer_Code, PALogicalJudgment_Code};

ActionAlgPtr MakeAlgorithm(const std::string& flow_id = "flow-a") {
    auto algorithm = std::make_shared<ActionAlg>();
    for (const auto code : kActionCodes) {
        ActionNode action;
        action.actionId     = code;
        action.flowActionId = flow_id;
        action.actionName   = "Original action";
        action.atomicCode   = "original-model";
        algorithm->workFlow.push_back(action);
    }
    return algorithm;
}

void CheckIdentity(const PActionBasePtr& instance, const std::string& task_id, const ActionNode& action) {
    REQUIRE(instance);
    CHECK(instance->GetTaskId() == task_id);
    CHECK(instance->GetActionId() == action.actionId);
    CHECK(instance->GetFlowActionId() == action.flowActionId);
    CHECK(instance->GetName() == action.actionName);
    CHECK(instance->GetAtomicCode() == action.atomicCode);
}

}  // namespace

TEST_CASE("PTaskBase reuses picture actions by action and flow keys", "[PicTaskService][PTaskOwnership]") {
    PTaskBase base;
    auto algorithm  = MakeAlgorithm();
    auto duplicates = algorithm->workFlow;
    for (auto& action : duplicates) {
        action.actionName = "Ignored duplicate name";
        action.atomicCode = "ignored-duplicate-model";
        algorithm->workFlow.push_back(action);
    }
    const auto other_flow = MakeAlgorithm("flow-b");
    algorithm->workFlow.insert(algorithm->workFlow.end(), other_flow->workFlow.begin(),
                               other_flow->workFlow.end());

    const auto task = base.TaskCreate("task-a", algorithm);
    REQUIRE(task);
    REQUIRE(task->actions.size() == 3 * kActionCodes.size());
    for (size_t i = 0; i < kActionCodes.size(); ++i) {
        CAPTURE(kActionCodes[i]);
        const auto& original = task->actions[i].actionInst;
        CheckIdentity(original, "task-a", algorithm->workFlow[i]);
        CHECK(task->actions[i + kActionCodes.size()].actionInst == original);
        const auto& separate_flow = task->actions[i + 2 * kActionCodes.size()].actionInst;
        CheckIdentity(separate_flow, "task-a", other_flow->workFlow[i]);
        CHECK(separate_flow != original);
        for (size_t j = 0; j < i; ++j) {
            CHECK(task->actions[j].actionInst != original);
        }
    }
}

TEST_CASE("PTaskBase never shares picture nodes between task objects", "[PicTaskService][PTaskOwnership]") {
    PTaskBase base;
    const auto first_algorithm     = MakeAlgorithm("bc");
    const auto first               = base.TaskCreate("a", first_algorithm);
    const auto repeated_id         = base.TaskCreate("a", first_algorithm);
    const auto collision_algorithm = MakeAlgorithm("c");
    const auto collision           = base.TaskCreate("ab", collision_algorithm);
    REQUIRE(first);
    REQUIRE(repeated_id);
    REQUIRE(collision);
    REQUIRE(first->actions.size() == kActionCodes.size());
    REQUIRE(repeated_id->actions.size() == kActionCodes.size());
    REQUIRE(collision->actions.size() == kActionCodes.size());

    // The old taskId + flowActionId cache key was "abc" for both task IDs.
    for (size_t i = 0; i < kActionCodes.size(); ++i) {
        CAPTURE(kActionCodes[i]);
        CheckIdentity(first->actions[i].actionInst, "a", first_algorithm->workFlow[i]);
        CheckIdentity(repeated_id->actions[i].actionInst, "a", first_algorithm->workFlow[i]);
        CheckIdentity(collision->actions[i].actionInst, "ab", collision_algorithm->workFlow[i]);
        CHECK(first->actions[i].actionInst != repeated_id->actions[i].actionInst);
        CHECK(first->actions[i].actionInst != collision->actions[i].actionInst);
    }
}

TEST_CASE("Picture task deletion and direct release relinquish every node",
          "[PicTaskService][PTaskOwnership]") {
    PTaskBase base;
    auto algorithm        = MakeAlgorithm();
    const auto duplicates = algorithm->workFlow;
    algorithm->workFlow.insert(algorithm->workFlow.end(), duplicates.begin(), duplicates.end());
    auto task = base.TaskCreate("task-a", algorithm);
    REQUIRE(task);
    std::vector<std::weak_ptr<PActionBase>> nodes;
    for (const auto& action : task->actions) {
        REQUIRE(action.actionInst);
        nodes.push_back(action.actionInst);
    }

    SECTION("Explicit deletion releases all repeated action entries") {
        REQUIRE(base.TaskActionDestroy(task));
        REQUIRE(base.TaskDelete(task));
    }
    SECTION("Releasing a task without TaskDelete leaves no manager owner") {
        task.reset();
    }
    for (const auto& node : nodes) {
        CHECK(node.expired());
    }
}

TEST_CASE("Picture nodes honor an external shared owner after task deletion",
          "[PicTaskService][PTaskOwnership]") {
    PTaskBase base;
    auto task = base.TaskCreate("task-a", MakeAlgorithm());
    REQUIRE(task);
    REQUIRE_FALSE(task->actions.empty());
    auto survivor                   = task->actions.front().actionInst;
    std::weak_ptr<PActionBase> node = survivor;
    REQUIRE(base.TaskDelete(task));
    CHECK_FALSE(node.expired());
    CHECK(survivor->GetTaskId() == "task-a");
    survivor.reset();
    CHECK(node.expired());
}

TEST_CASE("Failed picture task creation retries with fresh node configuration",
          "[PicTaskService][PTaskOwnership]") {
    PTaskBase base;
    auto failed_algorithm  = MakeAlgorithm();
    auto& failed_condition = failed_algorithm->workFlow.back().configObject.condition;
    failed_condition.type  = LogicType::Include;
    failed_condition.keyL  = "custom.labels";
    failed_condition.keyR  = "person";
    ActionNode unsupported;
    unsupported.actionId = "unsupported-picture-action";
    failed_algorithm->workFlow.push_back(unsupported);
    REQUIRE_FALSE(base.TaskCreate("retry", failed_algorithm));

    auto retry_algorithm = std::make_shared<ActionAlg>(*failed_algorithm);
    retry_algorithm->workFlow.pop_back();
    for (auto& action : retry_algorithm->workFlow) {
        action.actionName = "Replacement action";
        action.atomicCode = "replacement-model";
    }
    retry_algorithm->workFlow.back().configObject.condition.type = LogicType::NonInclude;
    const auto retried                                           = base.TaskCreate("retry", retry_algorithm);
    REQUIRE(retried);
    REQUIRE(retried->actions.size() == kActionCodes.size());
    for (size_t i = 0; i < kActionCodes.size(); ++i) {
        CAPTURE(kActionCodes[i]);
        CheckIdentity(retried->actions[i].actionInst, "retry", retry_algorithm->workFlow[i]);
    }

    // NonInclude succeeds for an unset custom value; the failed attempt's Include would fail.
    auto data                   = std::make_shared<AlgData>();
    data->chanDataDetect.detRet = std::make_shared<DataDetTrackClassify>();
    data->chanDataDetect.detRet->targets.emplace_back();
    REQUIRE(retried->actions.back().actionInst->HandPic(data) == util::ErrorEnum::Success);
    CHECK(data->chanDataDetect.detRet->targets.front().bLogicResult);
}
