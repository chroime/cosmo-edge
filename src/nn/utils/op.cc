#include "nn/utils/op.h"

#include <utility>

#include "nn/utils/string_format.h"

namespace cosmo::nn {

Op::Op(std::string name_) : name(std::move(name_)) {}
Op::~Op() {}

UnknownOp::UnknownOp(std::string name_) : Op(std::move(name_)) {}
UnknownOp::~UnknownOp() {}
std::string UnknownOp::Description() {
    std::stringstream stream;
    stream << "unknown op";
    return stream.str();
}

Resize::Resize(std::string name_) : Op(std::move(name_)) {}
Resize::~Resize() {}
std::string Resize::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "dszie: " << VectorToString<int>(this->dsize) << ","
           << "padding: " << BoolToString(this->padding) << "," << "gravity: " << this->gravity << ","
           << "color: " << VectorToString<int>(this->color);

    return stream.str();
}

Normalize::Normalize(std::string name_) : Op(std::move(name_)) {}
Normalize::~Normalize() {}
std::string Normalize::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "mean: " << VectorToString<float>(this->mean) << ","
           << "std: " << VectorToString<float>(this->std) << "," << "scale: " << this->scale << ","
           << "is_bgr: " << BoolToString(this->is_bgr);

    return stream.str();
}

CropResize::CropResize(std::string name_) : Op(std::move(name_)) {}
CropResize::~CropResize() {}
std::string CropResize::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "type: " << this->type << ","
           << "hw_ratio_levels: " << VectorToString<float>(this->bbox_hw_ratio_levels) << ","
           << "h_top_crop: " << VectorToString<float>(this->h_top_crop) << ","
           << "h_bottom_crop: " << VectorToString<float>(this->h_bottom_crop) << ","
           << "w_left_crop: " << VectorToString<float>(this->w_left_crop) << ","
           << "w_right_crop: " << VectorToString<float>(this->w_right_crop) << ","
           << "square: " << BoolToString(this->square) << "," << "square_mode: " << this->square_mode << ","
           << "dszie: " << VectorToString<int>(this->dsize) << "," << "gravity: " << this->gravity << ","
           << "color: " << VectorToString<int>(this->color);
    return stream.str();
}

YoloPost::YoloPost(std::string name_) : Op(std::move(name_)) {}
YoloPost::~YoloPost() {}
std::string YoloPost::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "nms_threshold: " << this->nms_threshold << ","
           << "nms_detection_conf: " << this->nms_detection_conf << "," << "top_k: " << this->top_k;

    return stream.str();
}

YoloNpuPost::YoloNpuPost(std::string name_) : Op(std::move(name_)) {}
YoloNpuPost::~YoloNpuPost() {}
std::string YoloNpuPost::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "nms_threshold: " << this->nms_threshold << ","
           << "nms_detection_conf: " << this->nms_detection_conf << "," << "top_k: " << this->top_k << ","
           << "anchor grids: [";
    for (size_t i = 0; i < this->anchors.size(); i++) {
        stream << "[";
        for (size_t j = 0; j < this->anchors[i].size(); j++) {
            stream << VectorToString<float>(this->anchors[i][j]);
            if (j != this->anchors[i].size() - 1) {
                stream << ",";
            }
        }
        stream << "], ";
    }
    stream << "], steps: " << VectorToString<float>(this->stride);

    return stream.str();
}

AffineCrop::AffineCrop(std::string name_) : Op(std::move(name_)) {}
AffineCrop::~AffineCrop() {}
std::string AffineCrop::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "norm_ratio: " << this->norm_ratio << ","
           << "norm_mode: " << this->norm_mode << "," << "output_hw: " << VectorToString<int>(this->output_hw)
           << "," << "center_index: " << VectorToString<int>(this->center_index);

    return stream.str();
}

Sum::Sum(std::string name_) : Op(std::move(name_)) {}
Sum::~Sum() {}

std::string Sum::Description() {
    std::stringstream stream;
    stream << "name: " << this->name;

    return stream.str();
}

// Sequence
Sequence::Sequence(std::string name_) : Op(std::move(name_)) {}
Sequence::~Sequence() {}

std::string Sequence::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "size: " << this->size << ","
           << "dsize: " << VectorToString<int>(this->dsize) << "," << "scale: " << this->scale << ","
           << "is_bgr: " << BoolToString(this->is_bgr);

    return stream.str();
}

// ArgMax
ArgMax::ArgMax(std::string name_) : Op(std::move(name_)) {}
ArgMax::~ArgMax() {}

std::string ArgMax::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "axis: " << this->axis;

    return stream.str();
}

Split::Split(std::string name_) : Op(std::move(name_)) {}
Split::~Split() {}

std::string Split::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "axis: " << this->axis << ","
           << "split: " << VectorToString<int>(this->split);

    return stream.str();
}

SplitArgMax::SplitArgMax(std::string name_) : Op(std::move(name_)) {}
SplitArgMax::~SplitArgMax() {}

std::string SplitArgMax::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "split: " << VectorToString<int>(this->split);

    return stream.str();
}

// combine image
CombineImage::CombineImage(std::string name_) : Op(std::move(name_)) {}
CombineImage::~CombineImage() {}

std::string CombineImage::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "count: " << this->count << ","
           << "dst_height: " << this->dst_height << "," << "dst_width: " << this->dst_width;

    return stream.str();
}

DinoEncoder::DinoEncoder(std::string name_) : Op(std::move(name_)) {}
DinoEncoder::~DinoEncoder() {}

std::string DinoEncoder::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "dst_height: " << this->dst_height << ","
           << "dst_width: " << this->dst_width << "," << "is_bgr:" << BoolToString(this->is_bgr) << ","
           << "mean: " << VectorToString<float>(this->mean) << ","
           << "std: " << VectorToString<float>(this->std);

    return stream.str();
}

DinoDecode::DinoDecode(std::string name_) : Op(std::move(name_)) {}
DinoDecode::~DinoDecode() {}

std::string DinoDecode::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "text_threshold: " << this->text_threshold << ","
           << "box_threshold: " << this->box_threshold;

    return stream.str();
}

SAMEncoder::SAMEncoder(std::string name_) : Op(std::move(name_)) {}
SAMEncoder::~SAMEncoder() {}

std::string SAMEncoder::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "dst_height: " << this->dst_height << ","
           << "dst_width: " << this->dst_width << "," << "is_bgr:" << BoolToString(this->is_bgr) << ","
           << "mean: " << VectorToString<float>(this->mean) << ","
           << "std: " << VectorToString<float>(this->std);

    return stream.str();
}

SAMPromptEncode::SAMPromptEncode(std::string name_) : Op(std::move(name_)) {}
SAMPromptEncode::~SAMPromptEncode() {}

std::string SAMPromptEncode::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "prompt_type: " << this->prompt_type << ","
           << "normalize: " << BoolToString(this->normalize) << "," << "encoder_size: " << this->encoder_size
           << "," << "max_points: " << this->max_points;

    return stream.str();
}

SAMDecode::SAMDecode(std::string name_) : Op(std::move(name_)) {}
SAMDecode::~SAMDecode() {}

std::string SAMDecode::Description() {
    std::stringstream stream;
    stream << "name: " << this->name << "," << "threshold: " << this->threshold << ","
           << "output_size: " << VectorToString<int>(this->output_size);

    return stream.str();
}

}  // namespace cosmo::nn