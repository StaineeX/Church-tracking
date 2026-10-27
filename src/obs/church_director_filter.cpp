// Copyright (C) 2026 Church Director contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#include "obs/church_director_filter.hpp"

#include "core/geometry.hpp"
#include "core/frame_conversion.hpp"
#include "core/opencv_person_detector.hpp"
#include "core/person_tracker.hpp"
#include "obs/filter_state.hpp"

#include <graphics/vec4.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
constexpr const char *tracking_armed_key = "tracking_armed";
constexpr const char *toggle_tracking_hotkey_name = "church_director_toggle_tracking";
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
    std::atomic<bool> tracking_armed{false};
    std::atomic<bool> toggle_tracking_pressed{false};
    obs_hotkey_id toggle_tracking_hotkey = OBS_INVALID_HOTKEY_ID;
    obs_source_t *hotkey_owner = nullptr; // OBS owns the parent source.
    gs_effect_t *effect = nullptr; // Accessed only on the OBS graphics thread.
    gs_eparam_t *region_param = nullptr;
    gs_eparam_t *zoom_param = nullptr;
    std::string effect_path;
    std::string effect_text;
    bool effect_error_logged = false;
    bool settings_error_logged = false;
    bool parent_error_logged = false;
    bool crop_error_logged = false;
    bool frame_conversion_error_logged = false;
};

std::optional<church::VideoFrame> copy_obs_frame(const obs_source_frame &source)
{
    church::SourceFrameView view;
    view.width = source.width;
    view.height = source.height;
    view.timestamp_ns = source.timestamp;
    view.full_range = source.full_range;
    view.flip = source.flip;
    std::copy_n(source.color_matrix, view.color_matrix.size(), view.color_matrix.begin());
    for (std::size_t i = 0; i < view.planes.size(); ++i)
        view.planes[i] = {source.data[i], source.linesize[i]};

    switch (source.format) {
    case VIDEO_FORMAT_RGBA: view.format = church::SourcePixelFormat::rgba; break;
    case VIDEO_FORMAT_BGRA: view.format = church::SourcePixelFormat::bgra; break;
    case VIDEO_FORMAT_BGRX: view.format = church::SourcePixelFormat::bgrx; break;
    case VIDEO_FORMAT_BGR3: view.format = church::SourcePixelFormat::bgr3; break;
    case VIDEO_FORMAT_NV12: view.format = church::SourcePixelFormat::nv12; break;
    case VIDEO_FORMAT_I420: view.format = church::SourcePixelFormat::i420; break;
    default: return std::nullopt;
    }
    return church::convert_source_frame(view);
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

void invalidate_state(FilterContext &filter)
{
    std::shared_ptr<church::FilterState> state;
    {
        std::lock_guard lock(filter.state_mutex);
        state = filter.state;
    }
    if (state)
        state->invalidate();
}

void set_tracking_armed(FilterContext &filter, bool armed)
{
    const bool was_armed = filter.tracking_armed.exchange(armed, std::memory_order_acq_rel);
    if (was_armed == armed)
        return;
    invalidate_state(filter);
    blog(LOG_INFO, "Church Director: tracking %s", armed ? "ARMED" : "DISARMED");
}

void toggle_tracking_hotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
    auto *filter = static_cast<FilterContext *>(data);
    if (!filter)
        return;
    if (!pressed) {
        filter->toggle_tracking_pressed.store(false, std::memory_order_release);
        return;
    }
    if (filter->toggle_tracking_pressed.exchange(true, std::memory_order_acq_rel))
        return;
    set_tracking_armed(*filter, !filter->tracking_armed.load(std::memory_order_acquire));
}

void register_tracking_hotkey(FilterContext &filter, obs_source_t *parent)
{
    if (!parent || filter.toggle_tracking_hotkey != OBS_INVALID_HOTKEY_ID)
        return;
    filter.hotkey_owner = parent;
    filter.toggle_tracking_hotkey = obs_hotkey_register_source(
        parent, toggle_tracking_hotkey_name, obs_module_text("ChurchDirector.ToggleTracking"),
        toggle_tracking_hotkey, &filter);
    if (filter.toggle_tracking_hotkey == OBS_INVALID_HOTKEY_ID)
        blog(LOG_ERROR, "Church Director: unable to register the tracking hotkey for the parent source");
}

void unregister_tracking_hotkey(FilterContext &filter)
{
    if (filter.toggle_tracking_hotkey != OBS_INVALID_HOTKEY_ID)
        obs_hotkey_unregister(filter.toggle_tracking_hotkey);
    filter.toggle_tracking_hotkey = OBS_INVALID_HOTKEY_ID;
    filter.hotkey_owner = nullptr;
}

void filter_add(void *data, obs_source_t *parent)
{
    auto *filter = static_cast<FilterContext *>(data);
    if (filter)
        register_tracking_hotkey(*filter, parent);
}

void filter_remove(void *data, obs_source_t *)
{
    auto *filter = static_cast<FilterContext *>(data);
    if (filter)
        unregister_tracking_hotkey(*filter);
}

void filter_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<FilterContext *>(data);
    if (!filter || !settings)
        return;
    std::shared_ptr<church::FilterState> next;
    try {
        set_tracking_armed(*filter, obs_data_get_bool(settings, tracking_armed_key));
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
    unregister_tracking_hotkey(*filter);
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
    obs_data_set_default_bool(settings, tracking_armed_key, false);
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
    obs_properties_add_bool(properties, tracking_armed_key,
                            obs_module_text("ChurchDirector.TrackingArmed"));
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
    if (!filter->tracking_armed.load(std::memory_order_acquire))
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
    else {
        if (!filter->frame_conversion_error_logged) {
            blog(LOG_WARNING,
                 "Church Director: unable to convert frame for tracking (format=%d, dimensions=%ux%u); tracking reset",
                 static_cast<int>(frame->format), frame->width, frame->height);
            filter->frame_conversion_error_logged = true;
        }
        state->invalidate();
    }
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
    if (!filter->tracking_armed.load(std::memory_order_acquire)) {
        obs_source_skip_video_filter(filter->source);
        return;
    }
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
        value.filter_add = filter_add;
        value.filter_remove = filter_remove;
        value.filter_video = filter_video;
        value.video_render = filter_render;
        return value;
    }();
    return info;
}
