#include <algorithm>
#include <array>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "catch_amalgamated.hpp"
#include "nn/core/blob_store.h"
#include "nn/core/shared_resource.h"
#include "nn/node/node.h"
#include "nn/node/node_type_utils.h"
#include "nn/utils/model_info_utils.h"
#include "nn/utils/profiler.h"

// Exercise graph construction without requiring model weights or changing the public API.
#define private public
#include "nn/core/graph.h"
#undef private

namespace cosmo::nn {
namespace {

    struct PreprocessGraphFixture {
        PreprocessGraphFixture() {
#if defined(COSMO_NN_USE_SOPHON_BACKEND)
            graph.device_type_ = DEVICE_SOPHON_TPU;
#elif defined(COSMO_NN_USE_RKNN_BACKEND)
            graph.device_type_ = DEVICE_RKNN;
#else
            graph.device_type_ = DEVICE_CPU;
#endif
            graph.blob_store = std::make_unique<BlobStore>(graph.device_type_, 0);
            model.input_node_infos.emplace_back();
            model.input_node_infos.back().name  = "input";
            model.input_node_infos.back().shape = {1, 3, 64, 64};
        }

        Status Build(const std::vector<std::string>& previous_blobs = {},
                     const std::vector<std::string>& previous_names = {},
                     const std::vector<DimsVector>& previous_shapes = {}) {
            return graph.BuildPreprocessChain(model, 0, previous_blobs, previous_names, previous_shapes,
                                              used_outputs, outputs, skip_count);
        }

        Graph graph;
        ModelInfo model;
        std::set<size_t> used_outputs;
        std::vector<std::string> outputs;
        size_t skip_count = 0;
    };

    TEST_CASE("NN empty preprocessing creates one typed identity input", "[nn][preprocess-graph]") {
        PreprocessGraphFixture fixture;
        const bool null_operations = GENERATE(false, true);
        if (null_operations) {
            fixture.model.input_node_infos[0].ops.push_back(nullptr);
            fixture.model.input_node_infos[0].ops.push_back(nullptr);
        }
        fixture.graph.max_batch_size                = 2;
        fixture.model.input_node_infos[0].data_type = 3;

        REQUIRE(bool(fixture.Build()));
        REQUIRE(fixture.model.input_node_infos[0].ops.empty());
        REQUIRE(NodeTypeUtils::TypedNodeCount(fixture.graph.nodes, NODE_IDENTITY) == 1);
        REQUIRE(fixture.outputs.size() == 1);
        auto* identity = fixture.graph.nodes.back().get();
        CHECK(identity->first_calculate_node);
        CHECK(identity->GetTopBlobNames() == fixture.outputs);
        REQUIRE(bool(identity->InferTopShapes()));
        CHECK(identity->GetTopBlobShapes() == std::vector<DimsVector>{{2, 3, 64, 64}});
        CHECK(identity->GetTopBlobDataTypes() == std::vector<DataType>{DATA_TYPE_INT8});
        CHECK(fixture.skip_count == 0);
    }

    TEST_CASE("NN preprocessing keeps actual operations in order", "[nn][preprocess-graph]") {
        PreprocessGraphFixture fixture;
        auto& ops            = fixture.model.input_node_infos[0].ops;
        const bool crop      = GENERATE(false, true);
        const bool normalize = GENERATE(false, true);
        ops.push_back(nullptr);
        if (crop) {
            auto op        = std::make_unique<CropResize>();
            op->dsize      = {64, 64};
            op->color      = {114, 114, 114};
            op->h_top_crop = op->h_bottom_crop = op->w_left_crop = op->w_right_crop = {0.0f};
            ops.push_back(std::move(op));
        } else {
            auto op   = std::make_unique<Resize>();
            op->dsize = {64, 64};
            ops.push_back(std::move(op));
        }
        auto* first_op = ops.back().get();
        ops.push_back(nullptr);
        if (normalize) {
            auto op = std::make_unique<Normalize>();
            op->std = {1.0f, 1.0f, 1.0f};
            ops.push_back(std::move(op));
        }

        REQUIRE(bool(fixture.Build()));
        REQUIRE(ops.size() == (normalize ? 2 : 1));
        CHECK(ops.front().get() == first_op);
        CHECK(NodeTypeUtils::TypedNodeCount(fixture.graph.nodes, NODE_IDENTITY) == 0);
        std::vector<Node*> preprocess;
        for (const auto& node : fixture.graph.nodes) {
            if (!NodeTypeUtils::IsInputNode(node.get()))
                preprocess.push_back(node.get());
        }
        REQUIRE(preprocess.size() == ops.size());
        CHECK(preprocess.front()->GetNodeType() == (crop ? NODE_CROP_RESIZE : NODE_RESIZE));
        CHECK(preprocess.front()->first_calculate_node);
        if (normalize) {
            CHECK(preprocess.back()->GetNodeType() == NODE_NORMALIZE);
            CHECK_FALSE(preprocess.back()->first_calculate_node);
            CHECK(preprocess.back()->GetBottomBlobNames() == preprocess.front()->GetTopBlobNames());
        }
        CHECK(fixture.outputs == preprocess.back()->GetTopBlobNames());
        CHECK(fixture.skip_count == 0);
    }

    TEST_CASE("NN model inputs retain name and unused shape matching", "[nn][preprocess-graph]") {
        PreprocessGraphFixture fixture;
        const std::vector<std::string> previous_blobs{"encoder/0", "encoder/1"};
        const std::vector<std::string> previous_names{"other", "input"};
        const std::vector<DimsVector> previous_shapes{{1, 3, 64, 64}, {1, 16}};

        SECTION("exact name takes precedence over shape and can be reused") {
            fixture.used_outputs.insert(1);
            REQUIRE(bool(fixture.Build(previous_blobs, previous_names, previous_shapes)));
            CHECK(fixture.outputs == std::vector<std::string>{"encoder/1"});
            CHECK(fixture.graph.nodes.empty());
        }
        SECTION("shape fallback consumes each output once") {
            fixture.model.input_node_infos[0].name = "renamed";
            REQUIRE(bool(fixture.Build(previous_blobs, previous_names, previous_shapes)));
            CHECK(fixture.outputs == std::vector<std::string>{"encoder/0"});
            CHECK(fixture.used_outputs == std::set<size_t>{0});
            REQUIRE(bool(fixture.Build(previous_blobs, previous_names, previous_shapes)));
            REQUIRE(fixture.outputs.size() == 2);
            CHECK(fixture.outputs.back() == "identity_0/0");
        }
        SECTION("explicit preprocessing prevents previous-output matching") {
            auto resize   = std::make_unique<Resize>();
            resize->dsize = {64, 64};
            fixture.model.input_node_infos[0].ops.push_back(std::move(resize));
            REQUIRE(bool(fixture.Build(previous_blobs, previous_names, previous_shapes)));
            CHECK(fixture.outputs == std::vector<std::string>{"resize_0/0"});
            CHECK(fixture.used_outputs.empty());
        }
    }

    TEST_CASE("NN direct CropResize graph processes the selected image region", "[nn][preprocess-graph]") {
        PreprocessGraphFixture fixture;
        auto crop        = std::make_unique<CropResize>();
        crop->dsize      = {64, 64};
        crop->color      = {114, 114, 114};
        crop->h_top_crop = crop->h_bottom_crop = crop->w_left_crop = crop->w_right_crop = {0.0f};
        fixture.model.input_node_infos[0].ops.push_back(std::move(crop));
        REQUIRE(bool(fixture.Build()));
        fixture.graph.shared_resource = std::make_unique<SharedResource>(0);
        for (const auto& node : fixture.graph.nodes)
            node->SetSharedResource(fixture.graph.shared_resource.get());
        REQUIRE(bool(fixture.graph.InferTopDesc()));
        REQUIRE(bool(fixture.graph.blob_store->AllocaAllBlobs()));
        fixture.graph.output_blob_names = fixture.outputs;

        std::vector<uint8_t> pixels(64 * 64 * 3, 10);
        for (int y = 8; y < 56; ++y) {
            for (int x = 8; x < 56; ++x)
                std::fill_n(pixels.begin() + (y * 64 + x) * 3, 3, 77);
        }
        BlobDesc image_desc;
        image_desc.device_type  = DEVICE_NAIVE;
        image_desc.data_type    = DATA_TYPE_UINT8;
        image_desc.data_format  = DATA_FORMAT_NHWC;
        image_desc.image_format = IMAGE_BGR;
        image_desc.dims         = {1, 64, 64, 3};
        BlobHandle image_handle;
        image_handle.base = pixels.data();
        auto image        = std::make_shared<Blob>(image_desc, image_handle);
        if (!UsesHostMemory(fixture.graph.device_type_)) {
            image_desc.device_type = fixture.graph.device_type_;
            image                  = std::make_shared<Blob>(image_desc, true);
            auto device_handle     = image->GetHandle();
            REQUIRE(bool(GetDevice(fixture.graph.device_type_)
                             ->CopyToDevice(&device_handle, &image_handle, image_desc, nullptr)));
        }
        std::array<int32_t, 4> rect{16, 16, 32, 32};
        BlobDesc rect_desc;
        rect_desc.device_type = DEVICE_NAIVE;
        rect_desc.data_type   = DATA_TYPE_INT32;
        rect_desc.dims        = {1, 4};
        BlobHandle rect_handle;
        rect_handle.base = rect.data();
        auto region      = std::make_shared<Blob>(rect_desc, rect_handle);
        auto status      = fixture.graph.Forward({{image}, {region}});
        INFO(status.description());
        REQUIRE(bool(status));
        auto output = fixture.graph.Output();
        REQUIRE(output.size() == 1);
        auto desc   = output[0]->GetBlobDesc();
        auto handle = output[0]->GetHandle();
        std::vector<uint8_t> result(64 * 64 * 3);
        if (UsesHostMemory(desc.device_type)) {
            std::copy_n(static_cast<const uint8_t*>(handle.base), result.size(), result.begin());
        } else {
            BlobHandle host;
            host.base = result.data();
            REQUIRE(bool(GetDevice(desc.device_type)->CopyFromDevice(&host, &handle, desc, nullptr)));
        }
        CHECK(std::all_of(result.begin(), result.end(), [](uint8_t value) { return value == 77; }));
    }

    TEST_CASE("NN sequential models wire the previous output into the next network",
              "[nn][preprocess-graph]") {
        PreprocessGraphFixture fixture;
        CombinedModelInfo info;
        info.reduce = "sequential";
        for (int index = 0; index < 2; ++index) {
            ModelInfo model;
            model.max_batch = 1;
            model.input_node_infos.emplace_back();
            model.input_node_infos.back().name  = index == 0 ? "image" : "embedding";
            model.input_node_infos.back().shape = {1, 16};
            model.output_node_infos.emplace_back();
            model.output_node_infos.back().name  = index == 0 ? "embedding" : "result";
            model.output_node_infos.back().shape = {1, 16};
            info.models.push_back(std::move(model));
        }
        REQUIRE(bool(fixture.graph.MakeUp(info)));
        REQUIRE(NodeTypeUtils::TypedNodeCount(fixture.graph.nodes, NODE_NET) == 2);
        CHECK(NodeTypeUtils::TypedNodeCount(fixture.graph.nodes, NODE_IDENTITY) == 1);
        auto* first_net  = fixture.graph.GetNodeByName("net_0");
        auto* second_net = fixture.graph.GetNodeByName("net_1");
        REQUIRE(first_net != nullptr);
        REQUIRE(second_net != nullptr);
        auto connection = second_net->GetBottomBlobNames();
        // Device backends may insert host/device copies between the two networks.
        for (size_t i = 0; i < fixture.graph.nodes.size() && connection != first_net->GetTopBlobNames();
             ++i) {
            const auto producer = std::find_if(
                fixture.graph.nodes.begin(), fixture.graph.nodes.end(),
                [&connection](const auto& node) { return node->GetTopBlobNames() == connection; });
            REQUIRE(producer != fixture.graph.nodes.end());
            REQUIRE((*producer)->GetNodeType() == NODE_COPY);
            connection = (*producer)->GetBottomBlobNames();
        }
        CHECK(connection == first_net->GetTopBlobNames());
    }

}  // namespace
}  // namespace cosmo::nn
