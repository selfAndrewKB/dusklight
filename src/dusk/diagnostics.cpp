#include "dusk/diagnostics.h"

#include "aurora/gfx.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "dusk/coop/input.h"
#include "dusk/coop/player_slots.h"
#include "dusk/dusk.h"
#include "dusk/io.hpp"
#include "dusk/logging.h"
#include "dusk/main.h"
#include "f_op/f_op_actor_mng.h"
#include "fmt/format.h"
#include "m_Do/m_Do_controller_pad.h"
#include "nlohmann/json.hpp"

#include <chrono>
#include <ctime>
#include <deque>
#include <fstream>
#include <string>
#include <unordered_map>

namespace dusk::diagnostics {
namespace {

using json = nlohmann::json;

constexpr int kEventVersion = 1;
constexpr int kManifestVersion = 1;
constexpr const char* kProfileSecondaryAlinkActionMirror = "coop.secondary_alink.action_mirror";
constexpr size_t kRingBufferMaxEvents = 3600;
constexpr u32 kLatestWriteMinFrameInterval = 15;

aurora::Module DiagnosticsLog("dusk::diagnostics");

struct Provider {
    const char* name;
    int schemaVersion;
    int sampleEveryFrames;
    bool emitOnChange;
    json (*collect)();
};

struct State {
    bool enabled = false;
    bool initialized = false;
    u32 lastFrame = 0;
    std::string sessionId;
    std::string role = "local";
    std::filesystem::path latestDir;
    std::filesystem::path sessionDir;
    std::deque<json> ring;
    json latest;
    std::unordered_map<std::string, json> lastEmittedData;
    bool latestDirty = false;
    bool latestWriteInitialized = false;
    u32 lastLatestWriteFrame = 0;
    SecondaryAlinkState secondaryAlinkState{};
    bool hasSecondaryAlinkState = false;
};

State s_state;

std::string ptrString(uintptr_t ptr) {
    if (ptr == 0) {
        return "0x0";
    }

    return fmt::format(FMT_STRING("0x{:x}"), ptr);
}

u64 monotonicTimeUs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

std::string makeSessionId() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#if _WIN32
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", &localTime);
    return fmt::format(FMT_STRING("{}-{:x}"), buffer, static_cast<unsigned int>(monotonicTimeUs() & 0xffff));
}

void appendJsonLine(const std::filesystem::path& path, const json& event) {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (!out) {
        return;
    }

    out << event.dump() << '\n';
}

void writeJsonFile(const std::filesystem::path& path, const json& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return;
    }

    out << data.dump(2) << '\n';
}

void clearFile(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
}

json buildManifest();

void ensureInitialized() {
    if (s_state.initialized || ConfigPath.empty()) {
        return;
    }

    s_state.sessionId = makeSessionId();
    s_state.latestDir = ConfigPath / "diagnostics" / "latest";
    s_state.sessionDir = ConfigPath / "diagnostics" / "sessions" / s_state.sessionId / s_state.role;

    std::error_code ec;
    std::filesystem::create_directories(s_state.latestDir, ec);
    std::filesystem::create_directories(s_state.sessionDir, ec);
    if (ec) {
        DiagnosticsLog.warn("failed to create diagnostics directories: {}", ec.message());
        return;
    }

    s_state.initialized = true;
    clearFile(s_state.latestDir / "events.jsonl");
    clearFile(s_state.sessionDir / "events.jsonl");
    const json manifest = buildManifest();
    writeJsonFile(s_state.latestDir / "manifest.json", manifest);
    writeJsonFile(s_state.sessionDir / "manifest.json", manifest);
    DiagnosticsLog.info("diagnostics profile {} writing to {}", kProfileSecondaryAlinkActionMirror,
                        io::fs_path_to_string(s_state.sessionDir));
}

json makeEnvelope(const char* provider, int schemaVersion, const char* kind, const json& data) {
    return {
        {"event_version", kEventVersion},
        {"session_id", s_state.sessionId},
        {"role", s_state.role},
        {"frame", s_state.lastFrame},
        {"time_us", monotonicTimeUs()},
        {"profile", kProfileSecondaryAlinkActionMirror},
        {"provider", provider},
        {"provider_schema_version", schemaVersion},
        {"kind", kind},
        {"data", data},
    };
}

void storeEvent(const json& event) {
    s_state.ring.push_back(event);
    while (s_state.ring.size() > kRingBufferMaxEvents) {
        s_state.ring.pop_front();
    }

    appendJsonLine(s_state.latestDir / "events.jsonl", event);
    appendJsonLine(s_state.sessionDir / "events.jsonl", event);
}

void updateProviderLatest(const char* provider, const json& data) {
    if (!s_state.latest.is_object()) {
        s_state.latest = json::object();
    }
    if (s_state.latest.contains(provider) && s_state.latest[provider] == data) {
        return;
    }

    s_state.latest[provider] = data;
    s_state.latestDirty = true;
}

bool shouldEmitProviderEvent(const char* provider, const json& data) {
    const auto result = s_state.lastEmittedData.find(provider);
    if (result != s_state.lastEmittedData.end() && result->second == data) {
        return false;
    }

    s_state.lastEmittedData[provider] = data;
    return true;
}

json collectSceneCurrent() {
    const char* stage = dComIfGp_getStartStageName();
    return {
        {"schema_version", 1},
        {"stage", stage != nullptr ? stage : ""},
        {"room", static_cast<int>(dComIfGp_getStartStageRoomNo())},
        {"layer", static_cast<int>(dComIfGp_getStartStageLayer())},
        {"point", static_cast<int>(dComIfGp_getStartStagePoint())},
        {"old_layer", static_cast<int>(dComIfGp_getLayerOld())},
    };
}

json collectRenderStats() {
    const AuroraStats& stats = lastFrameAuroraStats;
    return {
        {"schema_version", 1},
        {"backend", static_cast<int>(aurora_get_backend())},
        {"frame_usage_pct", frameUsagePct},
        {"queued_pipelines", stats.queuedPipelines},
        {"created_pipelines", stats.createdPipelines},
        {"draw_call_count", stats.drawCallCount},
        {"merged_draw_call_count", stats.mergedDrawCallCount},
        {"last_vert_size", stats.lastVertSize},
        {"last_uniform_size", stats.lastUniformSize},
        {"last_index_size", stats.lastIndexSize},
        {"last_storage_size", stats.lastStorageSize},
        {"last_texture_upload_size", stats.lastTextureUploadSize},
    };
}

json collectPlayerSlots() {
    json slots = json::array();
    for (int i = 0; i < coop::kPlayerSlotCount; i++) {
        const auto slot = static_cast<coop::PlayerSlot>(i);
        fopAc_ac_c* actor = coop::getPlayer(slot);
        json slotData = {
            {"slot", i},
            {"actor_uid", nullptr},
            {"ptr", ptrString(reinterpret_cast<uintptr_t>(actor))},
            {"stable_actor_uid_deferred", true},
        };

        if (actor != nullptr) {
            slotData["profile"] = static_cast<int>(fopAcM_GetProfName(actor));
            slotData["room"] = static_cast<int>(fopAcM_GetRoomNo(actor));
            slotData["argument"] = static_cast<int>(actor->argument);
            slotData["pos"] = {actor->current.pos.x, actor->current.pos.y, actor->current.pos.z};
            slotData["angle_y"] = static_cast<int>(actor->shape_angle.y);
            slotData["speed_f"] = actor->speedF;
        }
        slots.push_back(slotData);
    }

    return {
        {"schema_version", 1},
        {"slots", slots},
    };
}

json inputForSlot(coop::PlayerSlot slot) {
    const coop::PlayerInputState input = coop::readLocalInput(slot);
    return {
        {"stick_value", input.stickValue},
        {"stick_angle_3d", static_cast<int>(input.stickAngle3D)},
        {"trigger_buttons", input.triggerButtons},
        {"hold_buttons", input.holdButtons},
        {"trigger_lock_r", input.triggerLockR},
        {"hold_lock_r", input.holdLockR},
        {"hold_r", (input.holdButtons & PAD_TRIGGER_R) != 0},
        {"hold_l", (input.holdButtons & PAD_TRIGGER_L) != 0},
        {"hold_z", (input.holdButtons & PAD_TRIGGER_Z) != 0},
    };
}

json collectInputPad() {
    return {
        {"schema_version", 1},
        {"p1", inputForSlot(coop::PlayerSlot::Primary)},
        {"p2", inputForSlot(coop::PlayerSlot::Secondary)},
    };
}

json collectCoopProbes() {
    return {
        {"schema_version", 1},
        {"secondary_alink_probe_flags", coop::getSecondaryAlinkProbeFlags()},
        {"skip_execute", coop::hasSecondaryAlinkProbeFlag(coop::SecondaryAlinkProbe_SkipExecute)},
        {"skip_draw", coop::hasSecondaryAlinkProbeFlag(coop::SecondaryAlinkProbe_SkipDraw)},
        {"restore_primary_model_data_owner", coop::hasSecondaryAlinkProbeFlag(coop::SecondaryAlinkProbe_RestorePrimaryModelDataOwner)},
        {"scoped_draw_model_data_owner", coop::hasSecondaryAlinkProbeFlag(coop::SecondaryAlinkProbe_ScopedDrawModelDataOwner)},
        {"scoped_execute_model_data_owner", coop::hasSecondaryAlinkProbeFlag(coop::SecondaryAlinkProbe_ScopedExecuteModelDataOwner)},
    };
}

json collectAlinkSecondary() {
    const SecondaryAlinkState& state = s_state.secondaryAlinkState;
    json data = {
        {"schema_version", 1},
        {"available", s_state.hasSecondaryAlinkState},
    };
    if (!s_state.hasSecondaryAlinkState) {
        return data;
    }

    data["actor"] = ptrString(state.actor);
    data["proc"] = static_cast<unsigned int>(state.proc);
    data["speed_f"] = state.speedF;
    data["normal_speed"] = state.normalSpeed;
    data["stick_value"] = state.stickValue;
    data["under_frame"] = state.underFrame;
    data["under_rate"] = state.underRate;
    data["anim"] = ptrString(state.anim);
    data["attention_flags"] = state.attentionFlags;
    data["input_r"] = state.inputR;
    data["attention_lock"] = state.attentionLock;
    data["target"] = ptrString(state.target);
    data["item_button_r"] = state.itemButtonR;
    data["item_trigger_r"] = state.itemTriggerR;
    data["raw_mask"] = state.rawMask;
    data["r_status"] = static_cast<unsigned int>(state.rStatus);
    data["model_user"] = ptrString(state.modelUser);
    data["owner_under"] = ptrString(state.ownerUnder);
    data["owner_upper"] = ptrString(state.ownerUpper);
    return data;
}

Provider s_providers[] = {
    {"scene.current", 1, 30, true, collectSceneCurrent},
    {"render.stats", 1, 30, true, collectRenderStats},
    {"player.slots", 1, 1, true, collectPlayerSlots},
    {"input.pad", 1, 1, true, collectInputPad},
    {"coop.probes", 1, 30, true, collectCoopProbes},
    {"alink.secondary", 1, 1, true, collectAlinkSecondary},
};

json buildManifest() {
    json providers = json::array();
    for (const Provider& provider : s_providers) {
        providers.push_back({
            {"name", provider.name},
            {"schema_version", provider.schemaVersion},
            {"sample_every_frames", provider.sampleEveryFrames},
            {"emit_on_change", provider.emitOnChange},
        });
    }

    return {
        {"manifest_version", kManifestVersion},
        {"session_id", s_state.sessionId},
        {"role", s_state.role},
        {"profile", kProfileSecondaryAlinkActionMirror},
        {"ring_buffer_max_events", kRingBufferMaxEvents},
        {"stable_actor_uid_deferred", true},
        {"log_path", GetLogFilePath() != nullptr ? GetLogFilePath() : ""},
        {"providers", providers},
    };
}

void updateLatestFile() {
    json latest = {
        {"event_version", kEventVersion},
        {"session_id", s_state.sessionId},
        {"role", s_state.role},
        {"frame", s_state.lastFrame},
        {"time_us", monotonicTimeUs()},
        {"profile", kProfileSecondaryAlinkActionMirror},
        {"providers", s_state.latest},
    };

    writeJsonFile(s_state.latestDir / "latest.json", latest);
    writeJsonFile(s_state.sessionDir / "latest.json", latest);
    s_state.latestDirty = false;
    s_state.latestWriteInitialized = true;
    s_state.lastLatestWriteFrame = s_state.lastFrame;
}

void updateLatestFileIfDue(bool force) {
    if (!s_state.latestDirty && !force) {
        return;
    }
    if (!force && s_state.latestWriteInitialized &&
        s_state.lastFrame - s_state.lastLatestWriteFrame < kLatestWriteMinFrameInterval)
    {
        return;
    }

    updateLatestFile();
}

}  // namespace

void setSecondaryAlinkActionMirrorProfileEnabled(bool enabled) {
    if (s_state.enabled == enabled) {
        return;
    }

    s_state.enabled = enabled;
    if (enabled) {
        ensureInitialized();
        s_state.lastEmittedData.clear();
        s_state.latestDirty = true;
        flush("profile-enabled");
    } else if (s_state.initialized) {
        flush("profile-disabled");
    }
}

bool isSecondaryAlinkActionMirrorProfileEnabled() {
    return s_state.enabled;
}

void tick(u32 frame) {
    if (!s_state.enabled) {
        return;
    }

    ensureInitialized();
    if (!s_state.initialized) {
        return;
    }

    s_state.lastFrame = frame;
    for (const Provider& provider : s_providers) {
        if (provider.sampleEveryFrames <= 0 || (frame % static_cast<u32>(provider.sampleEveryFrames)) != 0) {
            continue;
        }

        json data = provider.collect();
        updateProviderLatest(provider.name, data);
        if (provider.emitOnChange && !shouldEmitProviderEvent(provider.name, data)) {
            continue;
        }

        storeEvent(makeEnvelope(provider.name, provider.schemaVersion, "snapshot", data));
    }
    updateLatestFileIfDue(false);
}

void flush(const char* reason) {
    if (!s_state.initialized) {
        ensureInitialized();
    }
    if (!s_state.initialized) {
        return;
    }

    json data = {
        {"schema_version", 1},
        {"reason", reason != nullptr ? reason : "manual"},
        {"buffered_events", s_state.ring.size()},
    };
    storeEvent(makeEnvelope("diagnostics.flush", 1, "flush", data));
    updateLatestFileIfDue(true);
}

void recordSecondaryAlinkState(const char* phase, const SecondaryAlinkState& state) {
    s_state.secondaryAlinkState = state;
    s_state.hasSecondaryAlinkState = true;
    if (!s_state.enabled) {
        return;
    }

    ensureInitialized();
    if (!s_state.initialized) {
        return;
    }

    json data = collectAlinkSecondary();
    data["phase"] = phase != nullptr ? phase : "";
    updateProviderLatest("alink.secondary", data);
    json comparableData = data;
    comparableData.erase("phase");
    if (!shouldEmitProviderEvent("alink.secondary", comparableData)) {
        updateLatestFileIfDue(false);
        return;
    }

    storeEvent(makeEnvelope("alink.secondary", 1, "change", data));
    updateLatestFileIfDue(false);
}

const std::filesystem::path& getOutputPath() {
    ensureInitialized();
    return s_state.sessionDir;
}

}  // namespace dusk::diagnostics
