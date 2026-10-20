// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs/church_director_filter.hpp"

#include "core/geometry.hpp"
#include "core/opencv_person_detector.hpp"
#include "core/person_tracker.hpp"
#include "obs/filter_state.hpp"

#include <graphics/vec4.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <utility>

namespace {
constexpr const char *enabled_key = "enabled";
constexpr const char *synthetic_key = "synthetic_mode";
constexpr const char *dead_zone_key = "dead_zone";
constexpr const char *min_zoom_key = "min_zoom";
constexpr const char *max_zoom_key = "max_zoom";
constexpr const char *pan_speed_key = "pan_speed";
constexpr const char *zoom_speed_key = "zoom_speed";
constexpr const char *confidence_key = "confidence_threshold";
constexpr const char *stale_timeout_key = "stale_timeout";

struct FilterContext {
    obs_source_t *source = nullptr; // OBS owns the filter source.
    std::mutex state_mutex;
    std::shared_ptr<church::FilterState> state;
    gs_effect_t *effect = nullptr; // Accessed only on the OBS graphics thread.
    gs_eparam_t *region_param = nullptr;
    gs_eparam_t *zoom_param = nullptr;
    std::string effect_path;
    std::string effect_text;
    bool effect_error_logged = false;
    bool settings_error_logged = false;
    bool parent_error_logged = false;
    bool crop_error_logged = false;
};

struct YuvMatrix {
    float values[12];
};

std::optional<std::size_t> output_size(std::uint32_t width, std::uint32_t height)
{
    const auto row_bytes = static_cast<std::uint64_t>(width) * 4u;
    if (row_bytes > std::numeric_limits<std::uint32_t>::max() ||
        row_bytes > std::numeric_limits<std::size_t>::max() ||
        (height != 0 && row_bytes > std::numeric_limits<std::uint64_t>::max() / height))
        return std::nullopt;
    const auto total_bytes = row_bytes * static_cast<std::uint64_t>(height);
    if (total_bytes > std::numeric_limits<std::size_t>::max())
        return std::nullopt;
    return static_cast<std::size_t>(total_bytes);
}

bool valid_plane(const std::uint8_t *data, std::uint32_t stride, std::uint32_t rows,
                 std::uint64_t minimum_row_bytes)
{
    return data && static_cast<std::uint64_t>(stride) >= minimum_row_bytes && rows != 0 &&
           static_cast<std::uint64_t>(stride) <=
               std::numeric_limits<std::size_t>::max() / rows;
}

std::optional<church::VideoFrame> make_output(const obs_source_frame &source)
{
    if (source.width == 0 || source.height == 0 ||
        source.timestamp > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return std::nullopt;
    const auto bytes = output_size(source.width, source.height);
    if (!bytes)
        return std::nullopt;

    church::VideoFrame result;
    result.width = source.width;
    result.height = source.height;
    result.stride = source.width * 4u;
    result.format = church::PixelFormat::rgba8;
    result.timestamp_ns = static_cast<std::int64_t>(source.timestamp);
    result.pixels.resize(*bytes);
    return result;
}

std::uint8_t clamp_byte(float value)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 255.0f)));
}

YuvMatrix yuv_matrix(const obs_source_frame &source)
{
    bool valid = true;
    for (std::size_t i = 0; i < 12; ++i)
        valid = valid && std::isfinite(source.color_matrix[i]);
    if (valid) {
        YuvMatrix matrix{};
        std::copy_n(source.color_matrix, 12, matrix.values);
        return matrix;
    }
    if (source.full_range)
        return {{{1.0f, 0.0f, 1.4020f, -0.7010f,
                  1.0f, -0.3441f, -0.7141f, 0.5291f,
                  1.0f, 1.7720f, 0.0f, -0.8860f}}};
    return {{{1.1644f, 0.0f, 1.7927f, -0.9729f,
              1.1644f, -0.2132f, -0.5329f, 0.3015f,
              1.1644f, 2.1124f, 0.0f, -1.1334f}}};
}

void write_yuv_pixel(church::VideoFrame &result, std::uint32_t x, std::uint32_t y,
                     std::uint8_t y_sample, std::uint8_t u_sample, std::uint8_t v_sample,
                     const YuvMatrix &matrix)
{
    const float y_value = static_cast<float>(y_sample) / 255.0f;
    const float u_value = static_cast<float>(u_sample) / 255.0f;
    const float v_value = static_cast<float>(v_sample) / 255.0f;
    auto *pixel = result.pixels.data() + static_cast<std::size_t>(y) * result.stride + x * 4u;
    pixel[0] = clamp_byte((matrix.values[0] * y_value + matrix.values[1] * u_value +
                           matrix.values[2] * v_value + matrix.values[3]) * 255.0f);
    pixel[1] = clamp_byte((matrix.values[4] * y_value + matrix.values[5] * u_value +
                           matrix.values[6] * v_value + matrix.values[7]) * 255.0f);
    pixel[2] = clamp_byte((matrix.values[8] * y_value + matrix.values[9] * u_value +
                           matrix.values[10] * v_value + matrix.values[11]) * 255.0f);
    pixel[3] = 255;
}

std::optional<church::VideoFrame> copy_obs_frame(const obs_source_frame &source)
{
    try {
        switch (source.format) {
        case VIDEO_FORMAT_RGBA:
        case VIDEO_FORMAT_BGRA:
        case VIDEO_FORMAT_BGRX:
        case VIDEO_FORMAT_BGR3:
        case VIDEO_FORMAT_NV12:
        case VIDEO_FORMAT_I420:
            break;
        default:
            return std::nullopt;
        }
        auto result = make_output(source);
        if (!result)
            return std::nullopt;

        const auto width = source.width;
        const auto height = source.height;
        switch (source.format) {
        case VIDEO_FORMAT_RGBA:
        case VIDEO_FORMAT_BGRA:
        case VIDEO_FORMAT_BGRX:
        case VIDEO_FORMAT_BGR3: {
            const std::uint32_t bytes_per_pixel = source.format == VIDEO_FORMAT_BGR3 ? 3u : 4u;
            if (!valid_plane(source.data[0], source.linesize[0], height,
                             static_cast<std::uint64_t>(width) * bytes_per_pixel))
                return std::nullopt;
            for (std::uint32_t y = 0; y < height; ++y) {
                const auto *input = source.data[0] + static_cast<std::size_t>(y) * source.linesize[0];
                auto *output = result->pixels.data() + static_cast<std::size_t>(y) * result->stride;
                for (std::uint32_t x = 0; x < width; ++x) {
                    if (source.format == VIDEO_FORMAT_RGBA) {
                        output[x * 4u + 0] = input[x * 4u + 0];
                        output[x * 4u + 1] = input[x * 4u + 1];
                        output[x * 4u + 2] = input[x * 4u + 2];
                        output[x * 4u + 3] = input[x * 4u + 3];
                    } else {
                        const auto input_stride = static_cast<std::uint32_t>(x * bytes_per_pixel);
                        output[x * 4u + 0] = input[input_stride + 2];
                        output[x * 4u + 1] = input[input_stride + 1];
                        output[x * 4u + 2] = input[input_stride + 0];
                        output[x * 4u + 3] = source.format == VIDEO_FORMAT_BGRA
                                                 ? input[input_stride + 3] : 255;
                    }
                }
            }
            return result;
        }
        case VIDEO_FORMAT_NV12: {
            const auto chroma_width = width / 2u + width % 2u;
            const auto chroma_height = height / 2u + height % 2u;
            const auto chroma_row_bytes = static_cast<std::uint64_t>(chroma_width) * 2u;
            if (!valid_plane(source.data[0], source.linesize[0], height, width) ||
                !valid_plane(source.data[1], source.linesize[1], chroma_height, chroma_row_bytes))
                return std::nullopt;
            const auto matrix = yuv_matrix(source);
            for (std::uint32_t y = 0; y < height; ++y) {
                const auto *y_row = source.data[0] + static_cast<std::size_t>(y) * source.linesize[0];
                const auto *uv_row = source.data[1] +
                                     static_cast<std::size_t>(y / 2u) * source.linesize[1];
                for (std::uint32_t x = 0; x < width; ++x) {
                    const auto uv_index = (x / 2u) * 2u;
                    write_yuv_pixel(*result, x, y, y_row[x], uv_row[uv_index], uv_row[uv_index + 1],
                                    matrix);
                }
            }
            return result;
        }
        case VIDEO_FORMAT_I420: {
            const auto chroma_width = width / 2u + width % 2u;
            const auto chroma_height = height / 2u + height % 2u;
            if (!valid_plane(source.data[0], source.linesize[0], height, width) ||
                !valid_plane(source.data[1], source.linesize[1], chroma_height, chroma_width) ||
                !valid_plane(source.data[2], source.linesize[2], chroma_height, chroma_width))
                return std::nullopt;
            const auto matrix = yuv_matrix(source);
            for (std::uint32_t y = 0; y < height; ++y) {
                const auto *y_row = source.data[0] + static_cast<std::size_t>(y) * source.linesize[0];
                const auto *u_row = source.data[1] +
                                    static_cast<std::size_t>(y / 2u) * source.linesize[1];
                const auto *v_row = source.data[2] +
                                    static_cast<std::size_t>(y / 2u) * source.linesize[2];
                for (std::uint32_t x = 0; x < width; ++x)
                    write_yuv_pixel(*result, x, y, y_row[x], u_row[x / 2u], v_row[x / 2u], matrix);
            }
            return result;
        }
        default:
            return std::nullopt;
        }
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

double finite_or_default(double value, double fallback)
{
    return std::isfinite(value) ? value : fallback;
}

church::MotionSettings read_motion_settings(obs_data_t *data)
{
    church::MotionSettings result;
    const double zone = std::clamp(finite_or_default(obs_data_get_double(data, dead_zone_key), 0.3),
                                   0.1, 0.9);
    const float half = static_cast<float>(zone * 0.5);
    result.dead_zone = {0.5f - half, 0.5f - half, 0.5f + half, 0.5f + half};
    // The current MotionEngine contract requires min_zoom == 1.0. Keep the
    // setting visible for a stable UI/schema without promising unsupported zoom.
    result.min_zoom = 1.0f;
    result.max_zoom = static_cast<float>(std::clamp(
        finite_or_default(obs_data_get_double(data, max_zoom_key), 2.0), 1.0, 4.0));
    result.max_pan_speed = static_cast<float>(std::clamp(
        finite_or_default(obs_data_get_double(data, pan_speed_key), 1.0), 0.0, 4.0));
    result.zoom_speed = static_cast<float>(std::clamp(
        finite_or_default(obs_data_get_double(data, zoom_speed_key), 0.5), 0.0, 4.0));
    result.confidence_threshold = static_cast<float>(std::clamp(
        finite_or_default(obs_data_get_double(data, confidence_key), 0.5), 0.0, 1.0));
    result.stale_timeout = static_cast<float>(std::clamp(
        finite_or_default(obs_data_get_double(data, stale_timeout_key), 0.5), 0.0, 10.0));
    return result;
}

const char *filter_name(void *) { return obs_module_text("ChurchDirector.Filter"); }

void filter_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<FilterContext *>(data);
    if (!filter || !settings)
        return;
    std::shared_ptr<church::FilterState> next;
    try {
        const bool synthetic_mode = obs_data_get_bool(settings, synthetic_key);
        auto motion_settings = read_motion_settings(settings);
        std::unique_ptr<church::Detector> detector;
        std::unique_ptr<church::Tracker> tracker;
        if (!synthetic_mode) {
            detector = std::make_unique<church::OpenCvPersonDetector>();
            tracker = std::make_unique<church::PersonTracker>();
        }
        next = std::make_shared<church::FilterState>(
            obs_data_get_bool(settings, enabled_key), synthetic_mode, motion_settings,
            std::move(detector), std::move(tracker));
    } catch (...) {
        if (!filter->settings_error_logged) {
            blog(LOG_ERROR, "Church Director: failed to start tracking worker");
            filter->settings_error_logged = true;
        }
    }
    std::shared_ptr<church::FilterState> previous;
    {
        std::lock_guard lock(filter->state_mutex);
        previous = std::exchange(filter->state, std::move(next));
    }
    // Joining happens on the lifecycle/settings callback, never video_render.
    if (previous)
        previous->stop();
}

void *filter_create(obs_data_t *settings, obs_source_t *source)
{
    auto *filter = new (std::nothrow) FilterContext;
    if (!filter)
        return nullptr;
    filter->source = source;
    filter_update(filter, settings);
    char *path = obs_module_file("effects/church-director.effect");
    if (path) {
        filter->effect_path = path;
        bfree(path);
        std::ifstream input(filter->effect_path, std::ios::binary);
        if (input)
            filter->effect_text.assign(std::istreambuf_iterator<char>(input),
                                       std::istreambuf_iterator<char>());
    }
    if (filter->effect_text.empty()) {
        blog(LOG_ERROR, "Church Director: unable to read crop effect asset");
        filter->effect_error_logged = true;
    }
    return filter;
}

void destroy_effect(void *data)
{
    gs_effect_destroy(static_cast<gs_effect_t *>(data));
}

void filter_destroy(void *data)
{
    auto *filter = static_cast<FilterContext *>(data);
    if (!filter)
        return;
    std::shared_ptr<church::FilterState> previous;
    {
        std::lock_guard lock(filter->state_mutex);
        previous = std::move(filter->state);
    }
    if (previous)
        previous->stop();
    if (filter->effect)
        obs_queue_task(OBS_TASK_GRAPHICS, destroy_effect, filter->effect, true);
    delete filter;
}

void filter_defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, enabled_key, true);
    obs_data_set_default_bool(settings, synthetic_key, false);
    obs_data_set_default_double(settings, dead_zone_key, 0.3);
    obs_data_set_default_double(settings, min_zoom_key, 1.0);
    obs_data_set_default_double(settings, max_zoom_key, 2.0);
    obs_data_set_default_double(settings, pan_speed_key, 1.0);
    obs_data_set_default_double(settings, zoom_speed_key, 0.5);
    obs_data_set_default_double(settings, confidence_key, 0.5);
    obs_data_set_default_double(settings, stale_timeout_key, 0.5);
}

obs_properties_t *filter_properties(void *)
{
    obs_properties_t *properties = obs_properties_create();
    obs_properties_add_bool(properties, enabled_key, obs_module_text("ChurchDirector.Enabled"));
    obs_properties_add_bool(properties, synthetic_key, obs_module_text("ChurchDirector.SyntheticMode"));
    obs_properties_add_float_slider(properties, dead_zone_key,
                                    obs_module_text("ChurchDirector.DeadZone"), 0.1, 0.9, 0.01);
    obs_properties_add_float(properties, min_zoom_key,
                             obs_module_text("ChurchDirector.MinZoom"), 1.0, 1.0, 0.1);
    obs_properties_add_float_slider(properties, max_zoom_key,
                                    obs_module_text("ChurchDirector.MaxZoom"), 1.0, 4.0, 0.1);
    obs_properties_add_float_slider(properties, pan_speed_key,
                                    obs_module_text("ChurchDirector.PanSpeed"), 0.0, 4.0, 0.05);
    obs_properties_add_float_slider(properties, zoom_speed_key,
                                    obs_module_text("ChurchDirector.ZoomSpeed"), 0.0, 4.0, 0.05);
    obs_properties_add_float_slider(properties, confidence_key,
                                    obs_module_text("ChurchDirector.ConfidenceThreshold"), 0.0, 1.0, 0.05);
    obs_properties_add_float_slider(properties, stale_timeout_key,
                                    obs_module_text("ChurchDirector.StaleTimeout"), 0.0, 10.0, 0.1);
    return properties;
}

obs_source_frame *filter_video(void *data, obs_source_frame *frame)
{
    auto *filter = static_cast<FilterContext *>(data);
    if (!filter || !frame)
        return frame;

    std::shared_ptr<church::FilterState> state;
    {
        std::lock_guard lock(filter->state_mutex);
        state = filter->state;
    }
    if (!state || !state->active() || state->synthetic_mode())
        return frame;

    auto owned = copy_obs_frame(*frame);
    if (owned)
        state->submit(std::move(*owned));
    return frame;
}

bool load_effect(FilterContext &filter)
{
    if (filter.effect)
        return true;
    if (filter.effect_error_logged)
        return false;
    filter.effect = gs_effect_create(filter.effect_text.c_str(), filter.effect_path.c_str(), nullptr);
    if (filter.effect) {
        filter.region_param = gs_effect_get_param_by_name(filter.effect, "visible_region");
        filter.zoom_param = gs_effect_get_param_by_name(filter.effect, "zoom");
    }
    if (!filter.effect || !filter.region_param || !filter.zoom_param ||
        !gs_effect_get_param_by_name(filter.effect, "image")) {
        blog(LOG_ERROR, "Church Director: crop effect is missing or has invalid uniforms");
        filter.effect_error_logged = true;
        if (filter.effect) {
            gs_effect_destroy(filter.effect);
            filter.effect = nullptr;
        }
        return false;
    }
    return true;
}

void filter_render(void *data, gs_effect_t *)
{
    auto *filter = static_cast<FilterContext *>(data);
    if (!filter || !filter->source)
        return;
    obs_source_t *parent = obs_filter_get_target(filter->source);
    if (!parent)
        return;
    if (!obs_source_get_base_width(parent) || !obs_source_get_base_height(parent)) {
        if (!filter->parent_error_logged) {
            blog(LOG_WARNING, "Church Director: parent source is unavailable or has no video size");
            filter->parent_error_logged = true;
        }
        obs_source_skip_video_filter(filter->source);
        return;
    }
    std::shared_ptr<church::FilterState> state;
    {
        std::lock_guard lock(filter->state_mutex);
        state = filter->state;
    }
    if (!state || !state->active()) {
        obs_source_skip_video_filter(filter->source);
        return;
    }

    const auto frame_time = obs_get_video_frame_time();
    if (state->synthetic_mode() &&
        frame_time <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        state->submit_synthetic(static_cast<std::int64_t>(frame_time));
    const church::CropState crop = state->crop();
    if (!church::is_valid(crop.visible_region) || !std::isfinite(crop.zoom) ||
        crop.zoom < 1.0f) {
        if (!filter->crop_error_logged) {
            blog(LOG_ERROR, "Church Director: worker published an invalid crop state");
            filter->crop_error_logged = true;
        }
        obs_source_skip_video_filter(filter->source);
        return;
    }
    if (!load_effect(*filter)) {
        obs_source_skip_video_filter(filter->source);
        return;
    }
    if (!obs_source_process_filter_begin(filter->source, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
        return;
    const vec4 region{crop.visible_region.left, crop.visible_region.top,
                      crop.visible_region.right, crop.visible_region.bottom};
    gs_effect_set_vec4(filter->region_param, &region);
    gs_effect_set_float(filter->zoom_param, crop.zoom);
    obs_source_process_filter_end(filter->source, filter->effect, 0, 0);
}
} // namespace

const obs_source_info &church_director_filter_info()
{
    static const obs_source_info info = [] {
        obs_source_info value{};
        value.id = "church_director_filter";
        value.type = OBS_SOURCE_TYPE_FILTER;
        value.output_flags = OBS_SOURCE_VIDEO;
        value.get_name = filter_name;
        value.create = filter_create;
        value.destroy = filter_destroy;
        value.update = filter_update;
        value.get_defaults = filter_defaults;
        value.get_properties = filter_properties;
        value.filter_video = filter_video;
        value.video_render = filter_render;
        return value;
    }();
    return info;
}
