#include "dusk/diagnostics.h"

#include "aurora/gfx.h"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_horse.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "dusk/coop/alink_form_resources.h"
#include "dusk/coop/alink_probes.h"
#include "dusk/coop/bokoblin_attack_probe.h"
#include "dusk/coop/camera.h"
#include "dusk/coop/caught_stun_owner.h"
#include "dusk/coop/damage_owner.h"
#include "dusk/coop/defender_owner.h"
#include "dusk/coop/enemy_targeting.h"
#include "dusk/coop/event_presentation.h"
#include "dusk/coop/gibdo_state_probe.h"
#include "dusk/coop/hud_diagnostics.h"
#include "dusk/coop/horse_owner.h"
#include "dusk/coop/input.h"
#include "dusk/coop/line_render_diagnostics.h"
#include "dusk/coop/player_attention.h"
#include "dusk/coop/player_query.h"
#include "dusk/coop/player_slots.h"
#include "dusk/coop/selected_target_state.h"
#include "dusk/coop/young_gohma_state_probe.h"
#include "dusk/dusk.h"
#include "dusk/game_clock.h"
#include "dusk/io.hpp"
#include "dusk/logging.h"
#include "dusk/main.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_camera_mng.h"
#include "fmt/format.h"
#include "m_Do/m_Do_controller_pad.h"
#include "nlohmann/json.hpp"

#include <chrono>
#include <cmath>
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
constexpr size_t kCameraAreaLoadMaxCheckpoints = 24;
constexpr u32 kLatestWriteMinFrameInterval = 15;
constexpr u64 kBudgetWindowUs = 60ull * 1000ull * 1000ull;

aurora::Module DiagnosticsLog("dusk::diagnostics");

struct Provider {
    const char* name;
    int schemaVersion;
    const char* costClass;
    int sampleEveryFrames;
    bool emitOnChange;
    int maxEventsPerMinute;
    size_t maxPayloadBytes;
    json (*collect)();
};

struct ProviderStats {
    u64 eventsWritten = 0;
    u64 bytesWritten = 0;
    u64 eventsThrottled = 0;
    u64 payloadsOversized = 0;
    u64 windowStartUs = 0;
    int windowEvents = 0;
    bool throttleMarkerWritten = false;
};

struct State {
    bool enabled = true;
    bool initialized = false;
    u32 lastFrame = 0;
    std::string sessionId;
    std::string role = "local";
    std::filesystem::path latestDir;
    std::filesystem::path sessionDir;
    std::deque<json> ring;
    json latest;
    std::unordered_map<std::string, json> lastEmittedData;
    std::unordered_map<std::string, ProviderStats> providerStats;
    bool latestDirty = false;
    bool latestWriteInitialized = false;
    u32 lastLatestWriteFrame = 0;
    SecondaryAlinkState secondaryAlinkState{};
    bool hasSecondaryAlinkState = false;
    std::deque<json> cameraAreaLoadCheckpoints;
    u32 cameraAreaLoadRevision = 0;
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
json collectDiagnosticsStats();

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

bool shouldEmitProviderEvent(const std::string& providerKey, const json& data) {
    const auto result = s_state.lastEmittedData.find(providerKey);
    if (result != s_state.lastEmittedData.end() && result->second == data) {
        return false;
    }

    s_state.lastEmittedData[providerKey] = data;
    return true;
}

bool shouldEmitProviderEvent(const char* provider, const json& data) {
    return shouldEmitProviderEvent(std::string(provider != nullptr ? provider : ""), data);
}

const Provider* findProvider(const char* name);
json eventKeyForProvider(const char* provider, const json& data);

void storeEventDirect(const json& event) {
    s_state.ring.push_back(event);
    while (s_state.ring.size() > kRingBufferMaxEvents) {
        s_state.ring.pop_front();
    }

    appendJsonLine(s_state.latestDir / "events.jsonl", event);
    appendJsonLine(s_state.sessionDir / "events.jsonl", event);
}

void storeEvent(const json& event) {
    storeEventDirect(event);
}

void recordProviderWrite(const char* provider, size_t bytes) {
    ProviderStats& stats = s_state.providerStats[provider];
    stats.eventsWritten++;
    stats.bytesWritten += bytes;
}

void recordProviderThrottle(const char* provider, const Provider& config, const char* reason,
                            size_t payloadBytes) {
    ProviderStats& stats = s_state.providerStats[provider];
    stats.eventsThrottled++;
    if (stats.throttleMarkerWritten) {
        return;
    }

    stats.throttleMarkerWritten = true;
    json data = {
        {"schema_version", 1},
        {"provider", provider},
        {"reason", reason},
        {"max_events_per_minute", config.maxEventsPerMinute},
        {"max_payload_bytes", config.maxPayloadBytes},
        {"payload_bytes", payloadBytes},
    };
    const json event = makeEnvelope("diagnostics.throttled", 1, "marker", data);
    storeEventDirect(event);
    recordProviderWrite("diagnostics.throttled", event.dump().size());
}

bool emitProviderEvent(const Provider& provider, const char* kind, const json& data) {
    const u64 now = monotonicTimeUs();
    ProviderStats& stats = s_state.providerStats[provider.name];
    if (stats.windowStartUs == 0 || now - stats.windowStartUs >= kBudgetWindowUs) {
        stats.windowStartUs = now;
        stats.windowEvents = 0;
        stats.throttleMarkerWritten = false;
    }

    const json event = makeEnvelope(provider.name, provider.schemaVersion, kind, data);
    const std::string serialized = event.dump();
    if (provider.maxPayloadBytes > 0 && serialized.size() > provider.maxPayloadBytes) {
        stats.payloadsOversized++;
        recordProviderThrottle(provider.name, provider, "payload-too-large", serialized.size());
        return false;
    }

    if (provider.maxEventsPerMinute > 0 && stats.windowEvents >= provider.maxEventsPerMinute) {
        recordProviderThrottle(provider.name, provider, "event-budget-exhausted", serialized.size());
        return false;
    }

    stats.windowEvents++;
    storeEventDirect(event);
    recordProviderWrite(provider.name, serialized.size());
    return true;
}

json actorIdentityEventData(const json& data) {
    json eventData = {
        {"actor_uid", nullptr},
        {"ptr", data.value("ptr", "0x0")},
        {"stable_actor_uid_deferred", data.value("stable_actor_uid_deferred", true)},
    };
    if (data.contains("profile")) {
        eventData["profile"] = data["profile"];
    }
    if (data.contains("id")) {
        eventData["id"] = data["id"];
    }
    if (data.contains("room")) {
        eventData["room"] = data["room"];
    }
    if (data.contains("argument")) {
        eventData["argument"] = data["argument"];
    }
    if (data.contains("attention_flags")) {
        eventData["attention_flags"] = data["attention_flags"];
    }
    return eventData;
}

json playerSlotsEventKey(const json& data) {
    json slots = json::array();
    if (data.contains("slots") && data["slots"].is_array()) {
        for (const json& slot : data["slots"]) {
            json slotData = {
                {"slot", slot.value("slot", -1)},
                {"actor_uid", nullptr},
                {"ptr", slot.value("ptr", "0x0")},
                {"stable_actor_uid_deferred", slot.value("stable_actor_uid_deferred", true)},
            };
            if (slot.contains("profile")) {
                slotData["profile"] = slot["profile"];
            }
            if (slot.contains("room")) {
                slotData["room"] = slot["room"];
            }
            if (slot.contains("argument")) {
                slotData["argument"] = slot["argument"];
            }
            slots.push_back(slotData);
        }
    }

    return {
        {"schema_version", data.value("schema_version", 1)},
        {"slots", slots},
    };
}

int stickZone(const json& input) {
    const double value = input.value("stick_value", 0.0);
    if (value < 0.15) {
        return 0;
    }
    if (value < 0.55) {
        return 1;
    }
    if (value < 0.90) {
        return 2;
    }
    return 3;
}

unsigned int gameplayButtonMask(unsigned int buttons) {
    return buttons & (PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_X | PAD_BUTTON_Y | PAD_TRIGGER_Z |
                      PAD_TRIGGER_L | PAD_TRIGGER_R | PAD_BUTTON_START);
}

json inputPadEventKey(const json& data) {
    auto inputEvent = [](const json& input) {
        const unsigned int holdButtons = input.value("hold_buttons", 0u);
        return json{
            {"hold_buttons", gameplayButtonMask(holdButtons)},
            {"hold_lock_r", input.value("hold_lock_r", 0)},
            {"hold_r", input.value("hold_r", false)},
            {"hold_l", input.value("hold_l", false)},
            {"hold_z", input.value("hold_z", false)},
            {"stick_zone", stickZone(input)},
        };
    };

    return {
        {"schema_version", data.value("schema_version", 1)},
        {"p1", inputEvent(data.value("p1", json::object()))},
        {"p2", inputEvent(data.value("p2", json::object()))},
    };
}

json renderStatsEventKey(const json& data) {
    return {
        {"schema_version", data.value("schema_version", 1)},
        {"backend", data.value("backend", 0)},
        {"queued_pipelines", data.value("queued_pipelines", 0)},
        {"created_pipelines", data.value("created_pipelines", 0)},
    };
}

json attentionListEventKey(const json& list) {
    json eventList = json::array();
    if (!list.is_array()) {
        return eventList;
    }

    for (const json& entry : list) {
        eventList.push_back({
            {"index", entry.value("index", -1)},
            {"pid", entry.value("pid", -1)},
            {"type", entry.value("type", 0)},
            {"actor", actorIdentityEventData(entry.value("actor", json::object()))},
        });
    }
    return eventList;
}

json attentionStateEventKey(const json& data) {
    return {
        {"schema_version", data.value("schema_version", 1)},
        {"available", data.value("available", false)},
        {"ptr", data.value("ptr", "0x0")},
        {"owner", actorIdentityEventData(data.value("owner", json::object()))},
        {"pad_no", data.value("pad_no", 0)},
        {"player_attention_flags", data.value("player_attention_flags", 0)},
        {"flags", data.value("flags", 0)},
        {"lockon_truth", data.value("lockon_truth", false)},
        {"lockon", data.value("lockon", false)},
        {"lock_edge", data.value("lock_edge", false)},
        {"lock_target_pid", data.value("lock_target_pid", -1)},
        {"target_actor_pid", data.value("target_actor_pid", -1)},
        {"lockon_count", data.value("lockon_count", 0)},
        {"action_count", data.value("action_count", 0)},
        {"check_object_count", data.value("check_object_count", 0)},
        {"attn_status", data.value("attn_status", 0)},
        {"lockon_target_0", actorIdentityEventData(data.value("lockon_target_0", json::object()))},
        {"action_target_0", actorIdentityEventData(data.value("action_target_0", json::object()))},
        {"check_object_target_0", actorIdentityEventData(data.value("check_object_target_0", json::object()))},
        {"lockon_list_active", attentionListEventKey(data.value("lockon_list_active", json::array()))},
        {"action_list_active", attentionListEventKey(data.value("action_list_active", json::array()))},
        {"check_object_list_active", attentionListEventKey(data.value("check_object_list_active", json::array()))},
    };
}

json playerStatusEventKey(const json& data) {
    const json buttonStatus = data.value("button_status", json::object());
    const json buttonStatusForce = data.value("button_status_force", json::object());

    return {
        {"schema_version", data.value("schema_version", 1)},
        {"button_status", {
            {"r", buttonStatus.value("r", 0)},
            {"a", buttonStatus.value("a", 0)},
            {"do", buttonStatus.value("do", 0)},
            {"z", buttonStatus.value("z", 0)},
            {"x", buttonStatus.value("x", 0)},
            {"y", buttonStatus.value("y", 0)},
        }},
        {"button_status_force", {
            {"r", buttonStatusForce.value("r", 0)},
            {"a", buttonStatusForce.value("a", 0)},
            {"do", buttonStatusForce.value("do", 0)},
            {"z", buttonStatusForce.value("z", 0)},
        }},
        {"player_status_words", data.value("player_status_words", json::array())},
        {"attention_lock", data.value("attention_lock", false)},
        {"secondary_attention_lock", data.value("secondary_attention_lock", false)},
        {"secondary_proc", data.value("secondary_proc", 0)},
    };
}

json horseOwnerActorIdentityEventData(const json& data) {
    json eventData = actorIdentityEventData(data);
    eventData.erase("attention_flags");
    return eventData;
}

json horseOwnerEventKey(const json& data) {
    json slots = json::array();
    if (data.contains("slots") && data["slots"].is_array()) {
        for (const json& slot : data["slots"]) {
            slots.push_back({
                {"slot", slot.value("slot", -1)},
                {"horse", horseOwnerActorIdentityEventData(slot.value("horse", json::object()))},
                {"player", horseOwnerActorIdentityEventData(slot.value("player", json::object()))},
                {"retained_ride_actor",
                 horseOwnerActorIdentityEventData(
                     slot.value("retained_ride_actor", json::object()))},
                {"canonical", slot.value("canonical", false)},
                {"runtime_clone", slot.value("runtime_clone", false)},
                {"spawn_pending", slot.value("spawn_pending", false)},
                {"pending_spawn_id", slot.value("pending_spawn_id", 0u)},
                {"riding", slot.value("riding", false)},
                {"call_wait", slot.value("call_wait", false)},
                {"call_deferred", slot.value("call_deferred", false)},
                {"placement_deferred", slot.value("placement_deferred", false)},
                {"call_target_valid", slot.value("call_target_valid", false)},
                {"last_summon_decision", slot.value("last_summon_decision", "")},
                {"localized_animation_count", slot.value("localized_animation_count", 0)},
                {"owner_mismatch", slot.value("owner_mismatch", false)},
                {"retained_horse_mismatch", slot.value("retained_horse_mismatch", false)},
            });
        }
    }

    return {
        {"schema_version", data.value("schema_version", 1)},
        {"canonical_horse", data.value("canonical_horse", "0x0")},
        {"duplicate_retained_horse", data.value("duplicate_retained_horse", false)},
        {"last_summon_revision", data.value("last_summon_revision", 0u)},
        {"last_summon_activator", data.value("last_summon_activator", -1)},
        {"last_summon_pos_valid", data.value("last_summon_pos_valid", false)},
        {"slots", slots},
    };
}

json playerQueryDecisionEventKey(const json& decision);
json enemyTargetingDecisionEventKey(const json& decision);

json playerQueryEventKey(const json& data) {
    json decisions = json::array();
    if (data.contains("decisions") && data["decisions"].is_array()) {
        for (const json& decision : data["decisions"]) {
            decisions.push_back(playerQueryDecisionEventKey(decision));
        }
    }

    return {
        {"schema_version", data.value("schema_version", 1)},
        {"decisions", decisions},
    };
}

json playerQueryDecisionEventKey(const json& decision) {
    json candidates = json::array();
    for (const json& candidate : decision.value("candidates", json::array())) {
        candidates.push_back({
            {"slot", candidate.value("slot", -1)},
            {"actor", actorIdentityEventData(candidate.value("actor", json::object()))},
        });
    }

    return {
        {"system", decision.value("system", std::string())},
        {"observer", actorIdentityEventData(decision.value("observer", json::object()))},
        {"found", decision.value("found", false)},
        {"selected_slot", decision.value("selected_slot", -1)},
        {"selected_actor", actorIdentityEventData(decision.value("selected_actor", json::object()))},
        {"candidates", candidates},
    };
}

std::string playerQueryDecisionEventStateKey(const json& decision) {
    const json observer = decision.value("observer", json::object());
    const std::string system = decision.value("system", std::string());
    const std::string observerPtr = observer.value("ptr", std::string("0x0"));
    return fmt::format(FMT_STRING("coop.player_query:{}:{}:{}"),
                       system, observerPtr, observer.value("id", 0));
}

json enemyTargetingDecisionEventKey(const json& decision) {
    return {
        {"scope", decision.value("scope", std::string())},
        {"observer", actorIdentityEventData(decision.value("observer", json::object()))},
        {"found", decision.value("found", false)},
        {"selected_slot", decision.value("selected_slot", -1)},
        {"selected_actor", actorIdentityEventData(decision.value("selected_actor", json::object()))},
        {"reason", decision.value("reason", std::string())},
        {"committed", decision.value("committed", false)},
    };
}

std::string enemyTargetingDecisionEventStateKey(const json& decision) {
    const json observer = decision.value("observer", json::object());
    const std::string scope = decision.value("scope", std::string());
    const std::string observerPtr = observer.value("ptr", std::string("0x0"));
    return fmt::format(FMT_STRING("enemy.targeting:{}:{}:{}"),
                       scope, observerPtr, observer.value("id", 0));
}

json selectedTargetStateDecisionEventKey(const json& decision) {
    return {
        {"label", decision.value("label", std::string())},
        {"observer", actorIdentityEventData(decision.value("observer", json::object()))},
        {"found", decision.value("found", false)},
        {"slot", decision.value("slot", -1)},
        {"actor", actorIdentityEventData(decision.value("actor", json::object()))},
        {"cut_active", decision.value("cut_active", false)},
        {"cut_type", decision.value("cut_type", -1)},
        {"horse_ride", decision.value("horse_ride", false)},
        {"reason", decision.value("reason", std::string())},
    };
}

std::string selectedTargetStateDecisionEventStateKey(const json& decision) {
    const json observer = decision.value("observer", json::object());
    const std::string label = decision.value("label", std::string());
    const std::string observerPtr = observer.value("ptr", std::string("0x0"));
    return fmt::format(FMT_STRING("selected_target.state:{}:{}:{}"),
                       label, observerPtr, observer.value("id", 0));
}

void emitPlayerQueryEvents(const Provider& provider, const json& data) {
    if (!data.contains("decisions") || !data["decisions"].is_array()) {
        return;
    }

    for (const json& decision : data["decisions"]) {
        const json eventKey = playerQueryDecisionEventKey(decision);
        if (provider.emitOnChange &&
            !shouldEmitProviderEvent(playerQueryDecisionEventStateKey(decision), eventKey))
        {
            continue;
        }

        const json eventData = {
            {"schema_version", data.value("schema_version", 1)},
            {"decision", eventKey},
        };
        emitProviderEvent(provider, "decision", eventData);
    }
}

void emitEnemyTargetingEvents(const Provider& provider, const json& data) {
    if (!data.contains("decisions") || !data["decisions"].is_array()) {
        return;
    }

    for (const json& decision : data["decisions"]) {
        const json eventKey = enemyTargetingDecisionEventKey(decision);
        if (provider.emitOnChange &&
            !shouldEmitProviderEvent(enemyTargetingDecisionEventStateKey(decision), eventKey))
        {
            continue;
        }

        const json eventData = {
            {"schema_version", data.value("schema_version", 1)},
            {"decision", eventKey},
            {"label", decision.value("label", std::string())},
            {"mode", decision.value("mode", std::string())},
            {"retain_seconds", decision.value("retain_seconds", 0.0f)},
            {"sticky_elapsed_seconds", decision.value("sticky_elapsed_seconds", 0.0f)},
            {"changed", decision.value("changed", false)},
        };
        emitProviderEvent(provider, "decision", eventData);
    }
}

void emitSelectedTargetStateEvents(const Provider& provider, const json& data) {
    if (!data.contains("decisions") || !data["decisions"].is_array()) {
        return;
    }

    for (const json& decision : data["decisions"]) {
        const json eventKey = selectedTargetStateDecisionEventKey(decision);
        if (provider.emitOnChange &&
            !shouldEmitProviderEvent(selectedTargetStateDecisionEventStateKey(decision), eventKey))
        {
            continue;
        }

        const json eventData = {
            {"schema_version", data.value("schema_version", 1)},
            {"decision", eventKey},
            {"speed_f", decision.value("speed_f", 0.0f)},
            {"shape_angle_y", decision.value("shape_angle_y", 0)},
            {"pos", decision.value("pos", json::array())},
        };
        emitProviderEvent(provider, "decision", eventData);
    }
}

void emitDamageOwnerEvents(const Provider& provider, const json& data) {
    if (!data.contains("hits") || !data["hits"].is_array()) {
        return;
    }

    for (const json& hit : data["hits"]) {
        const u64 eventId = hit.value("event_id", 0ull);
        if (eventId == 0) {
            continue;
        }

        const json eventKey = {
            {"event_id", eventId},
        };
        const std::string stateKey =
            fmt::format(FMT_STRING("damage.owner:{}"), static_cast<unsigned long long>(eventId));
        if (provider.emitOnChange && !shouldEmitProviderEvent(stateKey, eventKey)) {
            continue;
        }

        const json eventData = {
            {"schema_version", data.value("schema_version", 1)},
            {"hit", hit},
        };
        emitProviderEvent(provider, "hit", eventData);
    }
}

void emitDefenderOwnerEvents(const Provider& provider, const json& data) {
    if (!data.contains("decisions") || !data["decisions"].is_array()) {
        return;
    }

    for (const json& decision : data["decisions"]) {
        const u64 eventId = decision.value("event_id", 0ull);
        if (eventId == 0) {
            continue;
        }

        const json eventKey = {
            {"event_id", eventId},
        };
        const std::string stateKey =
            fmt::format(FMT_STRING("defender.owner:{}"), static_cast<unsigned long long>(eventId));
        if (provider.emitOnChange && !shouldEmitProviderEvent(stateKey, eventKey)) {
            continue;
        }

        const json eventData = {
            {"schema_version", data.value("schema_version", 1)},
            {"decision", decision},
        };
        emitProviderEvent(provider, "contact", eventData);
    }
}

void emitCaughtStunOwnerEvents(const Provider& provider, const json& data) {
    if (!data.contains("decisions") || !data["decisions"].is_array()) {
        return;
    }

    for (const json& decision : data["decisions"]) {
        const u64 eventId = decision.value("event_id", 0ull);
        if (eventId == 0) {
            continue;
        }

        const json eventKey = {
            {"event_id", eventId},
        };
        const std::string stateKey =
            fmt::format(FMT_STRING("caught_stun.owner:{}"), static_cast<unsigned long long>(eventId));
        if (provider.emitOnChange && !shouldEmitProviderEvent(stateKey, eventKey)) {
            continue;
        }

        const json eventData = {
            {"schema_version", data.value("schema_version", 1)},
            {"decision", decision},
        };
        emitProviderEvent(provider, "stun", eventData);
    }
}

void emitBokoblinAttackProbeEvents(const Provider& provider, const json& data) {
    if (!data.contains("probes") || !data["probes"].is_array()) {
        return;
    }

    for (const json& probe : data["probes"]) {
        const u64 eventId = probe.value("event_id", 0ull);
        if (eventId == 0) {
            continue;
        }

        const json eventKey = {
            {"event_id", eventId},
        };
        const std::string stateKey = fmt::format(
            FMT_STRING("bokoblin.attack:{}"), static_cast<unsigned long long>(eventId));
        if (provider.emitOnChange && !shouldEmitProviderEvent(stateKey, eventKey)) {
            continue;
        }

        const json eventData = {
            {"schema_version", data.value("schema_version", 1)},
            {"probe", probe},
        };
        emitProviderEvent(provider, probe.value("loop_suspect", false) ? "loop_suspect" : "state",
                          eventData);
    }
}

void emitGibdoStateProbeEvents(const Provider& provider, const json& data) {
    if (!data.contains("probes") || !data["probes"].is_array()) {
        return;
    }

    for (const json& probe : data["probes"]) {
        const u64 eventId = probe.value("event_id", 0ull);
        if (eventId == 0) {
            continue;
        }

        const json eventKey = {
            {"event_id", eventId},
        };
        const std::string stateKey = fmt::format(
            FMT_STRING("gibdo.state:{}"), static_cast<unsigned long long>(eventId));
        if (provider.emitOnChange && !shouldEmitProviderEvent(stateKey, eventKey)) {
            continue;
        }

        const json eventData = {
            {"schema_version", data.value("schema_version", 1)},
            {"probe", probe},
        };
        emitProviderEvent(provider, probe.value("loop_suspect", false) ? "loop_suspect" : "state",
                          eventData);
    }
}

void emitYoungGohmaStateProbeEvents(const Provider& provider, const json& data) {
    if (!data.contains("probes") || !data["probes"].is_array()) {
        return;
    }

    for (const json& probe : data["probes"]) {
        const u64 eventId = probe.value("event_id", 0ull);
        if (eventId == 0) {
            continue;
        }

        const json eventKey = {
            {"event_id", eventId},
        };
        const std::string stateKey = fmt::format(
            FMT_STRING("young_gohma.state:{}"), static_cast<unsigned long long>(eventId));
        if (provider.emitOnChange && !shouldEmitProviderEvent(stateKey, eventKey)) {
            continue;
        }

        const json eventData = {
            {"schema_version", data.value("schema_version", 1)},
            {"probe", probe},
        };
        emitProviderEvent(provider, probe.value("loop_suspect", false) ? "loop_suspect" : "state",
                          eventData);
    }
}

json alinkSecondaryEventKey(const json& data) {
    json copyRodKey = nullptr;
    if (data.contains("copy_rod")) {
        const json copyRod = data.value("copy_rod", json::object());
        copyRodKey = {
            {"active", true},
        };
        copyRodKey["actor"] = copyRod.value("actor", "0x0");
        copyRodKey["control_actor"] = copyRod.value("control_actor", "0x0");
        copyRodKey["camera_actor"] = copyRod.value("camera_actor", "0x0");
        copyRodKey["top_use"] = copyRod.value("top_use", false);
    }

    json bombKey = nullptr;
    if (data.contains("bomb")) {
        const json bomb = data.value("bomb", json::object());
        bombKey = {
            {"active", true},
            {"active_count", bomb.value("active_count", 0)},
            {"insect_count", bomb.value("insect_count", 0)},
            {"item_actor", bomb.value("item_actor", "0x0")},
            {"item_actor_id", bomb.value("item_actor_id", 0)},
            {"item_actor_name", bomb.value("item_actor_name", 0)},
        };
    }

    json eventKey = {
        {"schema_version", data.value("schema_version", 1)},
        {"available", data.value("available", false)},
        {"actor", data.value("actor", "0x0")},
        {"proc", data.value("proc", 0)},
        {"equip_item", data.value("equip_item", 0)},
        {"select_item_id", data.value("select_item_id", 0)},
        {"item_actor", data.value("item_actor", "0x0")},
        {"item_actor_id", data.value("item_actor_id", 0)},
        {"item_actor_name", data.value("item_actor_name", 0)},
        {"ride_actor", data.value("ride_actor", "0x0")},
        {"ride_actor_id", data.value("ride_actor_id", 0)},
        {"ride_actor_name", data.value("ride_actor_name", 0)},
        {"ride_status", data.value("ride_status", 0)},
        {"throw_boomerang_actor", data.value("throw_boomerang_actor", "0x0")},
        {"item_button", data.value("item_button", 0)},
        {"item_trigger", data.value("item_trigger", 0)},
        {"use_button_flags", data.value("use_button_flags", 0)},
        {"previous_use_button_flags", data.value("previous_use_button_flags", 0)},
        {"stick_active", data.value("stick_active", false)},
        {"move_active", data.value("move_active", false)},
        {"speed_active", data.value("speed_active", false)},
        {"input_r", data.value("input_r", false)},
        {"attention_lock", data.value("attention_lock", false)},
        {"target", data.value("target", "0x0")},
        {"item_button_r", data.value("item_button_r", false)},
        {"item_trigger_r", data.value("item_trigger_r", false)},
        {"r_status", data.value("r_status", 0)},
        {"model_user", data.value("model_user", "0x0")},
        {"owner_under", data.value("owner_under", "0x0")},
        {"owner_upper", data.value("owner_upper", "0x0")},
    };

    if (!copyRodKey.is_null()) {
        eventKey["copy_rod"] = copyRodKey;
    }
    if (!bombKey.is_null()) {
        eventKey["bomb"] = bombKey;
    }

    return eventKey;
}

json hudPresentationEventKey(const json& data) {
    json snapshots = json::array();
    for (const json& snapshot : data.value("snapshots", json::array())) {
        json items = json::array();
        for (const json& item : snapshot.value("items", json::array())) {
            const json pane = item.value("pane", json::object());
            items.push_back({
                {"button", item.value("button", "")},
                {"select_index", item.value("select_index", 0xff)},
                {"mix_index", item.value("mix_index", 0xff)},
                {"item", item.value("item", 0xff)},
                {"count", item.value("count", 0)},
                {"max_count", item.value("max_count", 0)},
                {"visible", pane.value("visible", false)},
                {"texture_visible", pane.value("texture_visible", false)},
                {"third_digit_visible", pane.value("third_digit_visible", false)},
            });
        }
        snapshots.push_back({
            {"phase", snapshot.value("phase", "")},
            {"presentation_slot", snapshot.value("presentation_slot", -1)},
            {"do_status", snapshot.value("do_status", 0)},
            {"items", items},
        });
    }

    return {
        {"schema_version", data.value("schema_version", 2)},
        {"slot_items", data.value("slot_items", json::array())},
        {"snapshots", snapshots},
        {"ring_admission", data.value("ring_admission", json::object())},
    };
}

const char* alinkProcName(u16 proc) {
    switch (proc) {
    case daAlink_c::PROC_SERVICE_WAIT:
        return "PROC_SERVICE_WAIT";
    case daAlink_c::PROC_WAIT:
        return "PROC_WAIT";
    case daAlink_c::PROC_MOVE:
        return "PROC_MOVE";
    case daAlink_c::PROC_ATN_MOVE:
        return "PROC_ATN_MOVE";
    case daAlink_c::PROC_ATN_ACTOR_WAIT:
        return "PROC_ATN_ACTOR_WAIT";
    case daAlink_c::PROC_ATN_ACTOR_MOVE:
        return "PROC_ATN_ACTOR_MOVE";
    case daAlink_c::PROC_FRONT_ROLL:
        return "PROC_FRONT_ROLL";
    case daAlink_c::PROC_CUT_NORMAL:
        return "PROC_CUT_NORMAL";
    case daAlink_c::PROC_BOW_SUBJECT:
        return "PROC_BOW_SUBJECT";
    case daAlink_c::PROC_BOW_MOVE:
        return "PROC_BOW_MOVE";
    case daAlink_c::PROC_BOOMERANG_SUBJECT:
        return "PROC_BOOMERANG_SUBJECT";
    case daAlink_c::PROC_BOOMERANG_MOVE:
        return "PROC_BOOMERANG_MOVE";
    case daAlink_c::PROC_BOOMERANG_CATCH:
        return "PROC_BOOMERANG_CATCH";
    case daAlink_c::PROC_COPY_ROD_SUBJECT:
        return "PROC_COPY_ROD_SUBJECT";
    case daAlink_c::PROC_COPY_ROD_MOVE:
        return "PROC_COPY_ROD_MOVE";
    case daAlink_c::PROC_COPY_ROD_SWING:
        return "PROC_COPY_ROD_SWING";
    case daAlink_c::PROC_COPY_ROD_REVIVE:
        return "PROC_COPY_ROD_REVIVE";
    case daAlink_c::PROC_SPINNER_READY:
        return "PROC_SPINNER_READY";
    case daAlink_c::PROC_SPINNER_WAIT:
        return "PROC_SPINNER_WAIT";
    case daAlink_c::PROC_CANOE_ROD_GRAB:
        return "PROC_CANOE_ROD_GRAB";
    case daAlink_c::PROC_CANOE_FISHING_WAIT:
        return "PROC_CANOE_FISHING_WAIT";
    case daAlink_c::PROC_CANOE_FISHING_REEL:
        return "PROC_CANOE_FISHING_REEL";
    case daAlink_c::PROC_CANOE_FISHING_GET:
        return "PROC_CANOE_FISHING_GET";
    case daAlink_c::PROC_FISHING_CAST:
        return "PROC_FISHING_CAST";
    case daAlink_c::PROC_FISHING_FOOD:
        return "PROC_FISHING_FOOD";
    default:
        return "";
    }
}

json eventKeyForProvider(const char* provider, const json& data) {
    const std::string name = provider != nullptr ? provider : "";
    if (name == "player.slots") {
        return playerSlotsEventKey(data);
    }
    if (name == "horse.owner") {
        return horseOwnerEventKey(data);
    }
    if (name == "render.lines") {
        json records = json::array();
        for (const json& record : data.value("records", json::array())) {
            records.push_back({
                {"material", record.value("material", "")},
                {"material_id", record.value("material_id", -1)},
                {"line_kind", record.value("line_kind", 0)},
                {"line_index", record.value("line_index", 0)},
                {"point_count", record.value("point_count", 0)},
                {"presentation_refresh", record.value("presentation_refresh", false)},
                {"control_non_finite_count", record.value("control_non_finite_count", 0)},
                {"expanded_non_finite_count", record.value("expanded_non_finite_count", 0)},
                {"suspicious", record.value("suspicious", false)},
            });
        }
        return {
            {"schema_version", data.value("schema_version", 1)},
            {"records", records},
        };
    }
    if (name == "input.pad") {
        return inputPadEventKey(data);
    }
    if (name == "render.stats") {
        return renderStatsEventKey(data);
    }
    if (name == "render.windows") {
        return {
            {"enabled", data.value("split_screen_enabled", false)},
            {"window_count", data.value("window_count", 0)},
            {"secondary_ready", data.value("secondary_ready", false)},
            {"secondary_requested", data.value("secondary_requested", false)},
            {"layout", data.value("layout", "")},
            {"windows", data.value("windows", json::array())},
        };
    }
    if (name == "camera.state") {
        const json camera0 = data.value("camera0", json::object());
        const json camera1 = data.value("camera1", json::object());
        const auto camera_body_key = [](const json& camera) {
            const json body = camera.value("body", json::object());
            return json{
                {"camera_id", body.value("camera_id", 0u)},
                {"owner", body.value("owner", std::string("0x0"))},
                {"type", body.value("type", 0)},
                {"mode", body.value("mode", 0)},
                {"active", body.value("active", false)},
                {"state", body.value("state", 0)},
                {"style", body.value("style", 0)},
                {"trim_size", body.value("trim_size", 0)},
                {"gear", body.value("gear", 0)},
            };
        };
        return {
            {"enabled", data.value("split_screen_enabled", false)},
            {"window_count", data.value("window_count", 0)},
            {"secondary_ready", data.value("secondary_ready", false)},
            {"secondary_requested", data.value("secondary_requested", false)},
            {"camera0_available", camera0.value("available", false)},
            {"camera1_available", camera1.value("available", false)},
            {"camera0_initialized", camera0.value("initialized", false)},
            {"camera1_initialized", camera1.value("initialized", false)},
            {"camera0_win", camera0.value("win_id", 0)},
            {"camera1_win", camera1.value("win_id", 0)},
            {"camera0_player", camera0.value("player1_id", 0)},
            {"camera1_player", camera1.value("player1_id", 0)},
            {"camera0_body", camera_body_key(camera0)},
            {"camera1_body", camera_body_key(camera1)},
        };
    }
    if (name == "camera.area_load") {
        return {
            {"schema_version", data.value("schema_version", 1)},
            {"revision", data.value("revision", 0u)},
            {"last_phase", data.value("last_phase", std::string())},
        };
    }
    if (name == "attention.state") {
        return attentionStateEventKey(data);
    }
    if (name == "player.status") {
        return playerStatusEventKey(data);
    }
    if (name == "coop.player_query") {
        return playerQueryEventKey(data);
    }
    if (name == "enemy.targeting") {
        json decisions = json::array();
        if (data.contains("decisions") && data["decisions"].is_array()) {
            for (const json& decision : data["decisions"]) {
                decisions.push_back(enemyTargetingDecisionEventKey(decision));
            }
        }
        return {
            {"schema_version", data.value("schema_version", 1)},
            {"decisions", decisions},
        };
    }
    if (name == "alink.secondary") {
        return alinkSecondaryEventKey(data);
    }
    if (name == "hud.presentation") {
        return hudPresentationEventKey(data);
    }
    return data;
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

json windowSummary(int idx) {
    json data = {
        {"index", idx},
        {"available", false},
    };

    dDlst_window_c* window = dComIfGp_getWindow(idx);
    if (window == nullptr) {
        return data;
    }

    view_port_class* viewport = window->getViewPort();
    data["available"] = true;
    data["camera_id"] = static_cast<int>(window->getCameraID());
    data["viewport"] = {
        {"x", viewport->x_orig},
        {"y", viewport->y_orig},
        {"width", viewport->width},
        {"height", viewport->height},
        {"near_z", viewport->near_z},
        {"far_z", viewport->far_z},
    };
    data["scissor"] = {
        {"x", viewport->scissor.x_orig},
        {"y", viewport->scissor.y_orig},
        {"width", viewport->scissor.width},
        {"height", viewport->scissor.height},
    };
    return data;
}

json cameraMapSummary(dCamera_c& camera);
json cameraToolSummary(dCamera_c& camera, dCamMapToolData const& tool);

json cameraSummary(int idx) {
    json data = {
        {"index", idx},
        {"available", false},
        {"win_id", static_cast<int>(dComIfGp_getCameraWinID(idx))},
        {"player1_id", static_cast<int>(dComIfGp_getCameraPlayer1ID(idx))},
        {"player2_id", static_cast<int>(dComIfGp_getCameraPlayer2ID(idx))},
        {"attention_status", static_cast<unsigned int>(dComIfGp_getCameraAttentionStatus(idx))},
    };

    camera_process_class* camera = dComIfGp_getCamera(idx);
    if (camera == nullptr) {
        data["ptr"] = ptrString(0);
        data["initialized"] = false;
        return data;
    }

    data["available"] = true;
    data["ptr"] = ptrString(reinterpret_cast<uintptr_t>(camera));
    data["initialized"] = camera->mCamera.field_0xb0c != 0;
    if (camera->mCamera.field_0xb0c == 0) {
        return data;
    }

    data["near"] = camera->view.near_;
    data["far"] = camera->view.far_;
    data["fovy"] = camera->view.fovy;
    data["aspect"] = camera->view.aspect;
    data["eye"] = {camera->view.lookat.eye.x, camera->view.lookat.eye.y, camera->view.lookat.eye.z};
    data["center"] = {camera->view.lookat.center.x, camera->view.lookat.center.y,
                      camera->view.lookat.center.z};
    data["distance"] = camera->view.lookat.eye.abs(camera->view.lookat.center);
    data["body"] = {
        {"camera_id", static_cast<unsigned int>(camera->mCamera.CameraID())},
        {"owner", ptrString(reinterpret_cast<uintptr_t>(camera->mCamera.mpPlayerActor))},
        {"owner_angle_y", camera->mCamera.mpPlayerActor != nullptr
                              ? static_cast<int>(camera->mCamera.mpPlayerActor->shape_angle.y)
                              : 0},
        {"owner_room", camera->mCamera.mpPlayerActor != nullptr
                           ? static_cast<int>(fopAcM_GetRoomNo(camera->mCamera.mpPlayerActor))
                           : -1},
        {"global_stay_room", static_cast<int>(dComIfGp_roomControl_getStayNo())},
        {"type", camera->mCamera.Type()},
        {"type_name", camera->mCamera.mCamTypeData != nullptr
                          ? camera->mCamera.mCamTypeData[camera->mCamera.Type()].name
                          : ""},
        {"mode", camera->mCamera.Mode()},
        {"active", camera->mCamera.Active()},
        {"state", camera->mCamera.mCurState},
        {"style", camera->mCamera.mCamStyle},
        {"style_timer", camera->mCamera.mCurCamStyleTimer},
        {"trim_height", camera->mCamera.TrimHeight()},
        {"trim_size", camera->mCamera.mTrimSize},
        {"gear", camera->mCamera.Gear()},
        {"window_width", camera->mCamera.mWindowWidth},
        {"window_height", camera->mCamera.mWindowHeight},
        {"window_aspect", camera->mCamera.mWindowAspect},
        {"view_cache_distance", camera->mCamera.iEye().abs(camera->mCamera.iCenter())},
        {"view_cache_yaw", camera->mCamera.iU()},
        {"controlled_yaw", camera->mCamera.U2()},
        {"stored_yaw", camera->mCamera.U()},
        {"map", cameraMapSummary(camera->mCamera)},
    };
    return data;
}

json cameraToolSummary(dCamera_c& camera, dCamMapToolData const& tool) {
    json data = {
        {"camera_index", tool.mCameraIndex},
        {"arrow_index", tool.mArrowIndex},
        {"flags", tool.mFlags},
        {"priority", tool.mPriority},
        {"path_id", tool.mPathId},
        {"actor", ptrString(reinterpret_cast<uintptr_t>(tool.mpActor))},
    };
    if (tool.mCameraIndex != 0xFF) {
        data["type_name"] = tool.mCamData.m_cam_type;
        data["resolved_type"] = static_cast<unsigned int>(tool.mCamData.field_0x16);
        if (camera.mCamTypeData != nullptr && tool.mCamData.field_0x16 < camera.mCamTypeNum) {
            data["resolved_type_name"] = camera.mCamTypeData[tool.mCamData.field_0x16].name;
        }
    }
    return data;
}

json cameraMapSummary(dCamera_c& camera) {
    json data = {
        {"map_tool_type", camera.mMapToolType},
        {"map_tool_type_name", camera.mCamTypeData != nullptr && camera.mMapToolType >= 0 &&
                                   camera.mMapToolType < camera.mCamTypeNum
                                   ? camera.mCamTypeData[camera.mMapToolType].name
                                   : ""},
        {"room_tool", cameraToolSummary(camera, camera.mRoomMapTool)},
        {"stage_tool", cameraToolSummary(camera, camera.mStageCamTool)},
        {"default_room_tool", cameraToolSummary(camera, camera.mDefRoomCamTool)},
        {"tag_tool", cameraToolSummary(camera, camera.mTagCamTool)},
    };
    return data;
}

json collectRenderWindows() {
    const int windowCount = dComIfGp_getWindowNum();
    json windows = json::array();
    for (int i = 0; i < windowCount; i++) {
        windows.push_back(windowSummary(i));
    }

    return {
        {"schema_version", 2},
        {"split_screen_enabled", dusk::coop::camera::isSplitScreenEnabled()},
        {"split_screen_requested", dusk::coop::camera::isSplitScreenRequested()},
        {"window_count", windowCount},
        {"secondary_ready", dusk::coop::camera::isSecondaryCameraReady()},
        {"secondary_requested", dusk::coop::camera::isSecondaryCameraRequested()},
        {"layout", dusk::coop::camera::getSplitScreenLayout() ==
                       dusk::coop::camera::SplitScreenLayout::Horizontal
                       ? "horizontal"
                       : "vertical"},
        {"windows", windows},
    };
}

json collectCameraState() {
    return {
        {"schema_version", 2},
        {"split_screen_enabled", dusk::coop::camera::isSplitScreenEnabled()},
        {"split_screen_requested", dusk::coop::camera::isSplitScreenRequested()},
        {"window_count", dComIfGp_getWindowNum()},
        {"secondary_ready", dusk::coop::camera::isSecondaryCameraReady()},
        {"secondary_requested", dusk::coop::camera::isSecondaryCameraRequested()},
        {"camera0", cameraSummary(0)},
        {"camera1", cameraSummary(1)},
    };
}

json collectCameraAreaLoad() {
    json checkpoints = json::array();
    for (const json& checkpoint : s_state.cameraAreaLoadCheckpoints) {
        checkpoints.push_back(checkpoint);
    }

    return {
        {"schema_version", 1},
        {"revision", s_state.cameraAreaLoadRevision},
        {"last_phase", checkpoints.empty() ? std::string()
                                             : checkpoints.back().value("phase", std::string())},
        {"checkpoints", checkpoints},
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
            {"requested", coop::isPlayerRequested(slot)},
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
        {"schema_version", 2},
        {"slots", slots},
    };
}

json actorSummary(const fopAc_ac_c* actor) {
    json data = {
        {"actor_uid", nullptr},
        {"ptr", ptrString(reinterpret_cast<uintptr_t>(actor))},
        {"stable_actor_uid_deferred", true},
    };
    if (actor == nullptr) {
        return data;
    }

    data["profile"] = static_cast<int>(fopAcM_GetProfName(actor));
    data["id"] = static_cast<int>(fopAcM_GetID(actor));
    data["room"] = static_cast<int>(fopAcM_GetRoomNo(actor));
    data["argument"] = static_cast<int>(actor->argument);
    data["attention_flags"] = actor->attention_info.flags;
    data["pos"] = {actor->current.pos.x, actor->current.pos.y, actor->current.pos.z};
    data["angle_y"] = static_cast<int>(actor->shape_angle.y);
    return data;
}

json vecSummary(const cXyz* pos) {
    if (pos == nullptr) {
        return json::array();
    }

    return json::array({pos->x, pos->y, pos->z});
}

json attentionListEntry(dAttList_c& entry) {
    fopAc_ac_c* actor = entry.getActor();
    return {
        {"pid", static_cast<int>(entry.getPid())},
        {"type", entry.mType},
        {"angle", static_cast<int>(entry.mAngle.Val())},
        {"weight", entry.mWeight},
        {"distance", entry.mDistance},
        {"actor", actorSummary(actor)},
    };
}

json attentionListSummary(dAttList_c* entries, int capacity) {
    json list = json::array();
    for (int i = 0; i < capacity; i++) {
        if (entries[i].getPid() == -1 && entries[i].getActor() == nullptr) {
            continue;
        }
        json entry = attentionListEntry(entries[i]);
        entry["index"] = i;
        list.push_back(entry);
    }
    return list;
}

json reinPointSummary(const cXyz* points, int count, const cXyz& horsePos) {
    constexpr f32 kFarPointDistance = 2000.0f;
    constexpr f32 kLongSegmentDistance = 1000.0f;
    constexpr f32 kDistanceBucketSize = 100.0f;
    int nonFiniteCount = 0;
    f32 maxDistance = 0.0f;
    f32 maxSegment = 0.0f;
    for (int i = 0; points != nullptr && i < count; i++) {
        const cXyz& point = points[i];
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
            nonFiniteCount++;
            continue;
        }

        const f32 distance = point.abs(horsePos);
        if (distance > maxDistance) {
            maxDistance = distance;
        }
        if (i > 0) {
            const f32 segment = point.abs(points[i - 1]);
            if (segment > maxSegment) {
                maxSegment = segment;
            }
        }
    }

    const bool farPoint = maxDistance > kFarPointDistance;
    const bool longSegment = maxSegment > kLongSegmentDistance;
    json summary = {
        {"non_finite_count", nonFiniteCount},
        {"far_from_horse", farPoint},
        {"long_segment", longSegment},
        {"max_distance_bucket", static_cast<int>(maxDistance / kDistanceBucketSize)},
        {"max_segment_bucket", static_cast<int>(maxSegment / kDistanceBucketSize)},
    };
    if (nonFiniteCount != 0 || farPoint || longSegment) {
        summary["max_distance"] = maxDistance;
        summary["max_segment"] = maxSegment;
    }
    return summary;
}

json collectHorseOwner() {
    json slots = json::array();
    daHorse_c* retainedHorses[coop::kPlayerSlotCount] = {};
    bool duplicateRetainedHorse = false;
    const cXyz* lastSummonPos = coop::horse_owner::getLastSummonPos();
    for (int i = 0; i < coop::kPlayerSlotCount; i++) {
        const coop::PlayerSlot slot = static_cast<coop::PlayerSlot>(i);
        daHorse_c* horse = coop::horse_owner::getHorse(slot);
        daAlink_c* player = static_cast<daAlink_c*>(coop::getPlayer(slot));
        fopAc_ac_c* rideActor = player != nullptr ? player->getRideActor() : nullptr;
        daHorse_c* retainedHorse =
            rideActor != nullptr && fopAcM_GetName(rideActor) == fpcNm_HORSE_e
                ? static_cast<daHorse_c*>(rideActor)
                : nullptr;
        retainedHorses[i] = retainedHorse;
        for (int previous = 0; previous < i; previous++) {
            if (retainedHorse != nullptr && retainedHorse == retainedHorses[previous]) {
                duplicateRetainedHorse = true;
            }
        }
        const fpc_ProcID pendingSpawnId = coop::horse_owner::getPendingHorseSpawnId(slot);
        const coop::horse_owner::HorseReinSimulationState* reins =
            coop::horse_owner::getReinSimulationState(horse);
        const cXyz* callTarget = coop::horse_owner::getCallTarget(slot);
        const coop::horse_owner::HorseSummonDecision lastSummonDecision =
            coop::horse_owner::getLastSummonDecision(slot);

        json slotData = {
            {"slot", i},
            {"horse", actorSummary(horse)},
            {"player", actorSummary(player)},
            {"retained_ride_actor", actorSummary(rideActor)},
            {"canonical", coop::horse_owner::isCanonicalHorse(horse)},
            {"runtime_clone", coop::horse_owner::isAdditionalHorse(horse)},
            {"spawn_pending", pendingSpawnId != fpcM_ERROR_PROCESS_ID_e},
            {"pending_spawn_id", static_cast<unsigned int>(pendingSpawnId)},
            {"call_deferred", coop::horse_owner::isCallDeferred(slot)},
            {"call_delay_seconds", coop::horse_owner::getCallDelaySeconds(slot)},
            {"placement_deferred", coop::horse_owner::isPlacementDeferred(slot)},
            {"call_target_valid", callTarget != nullptr},
            {"call_target", vecSummary(callTarget)},
            {"last_summon_decision",
             coop::horse_owner::getHorseSummonDecisionName(lastSummonDecision)},
            {"last_summon_decision_id", static_cast<unsigned int>(lastSummonDecision)},
            {"owner_mismatch", horse != nullptr &&
                                   coop::horse_owner::getPlayerForHorse(horse) != player},
            {"retained_horse_mismatch", retainedHorse != nullptr && retainedHorse != horse},
        };

        if (horse != nullptr) {
            slotData["speed_f"] = horse->speedF;
            slotData["process"] = static_cast<unsigned int>(horse->getProcID());
            slotData["riding"] = horse->isRidden();
            slotData["call_wait"] = horse->checkHorseCallWait() != 0;
            if (callTarget != nullptr) {
                slotData["call_target_distance_xz"] = horse->current.pos.absXZ(*callTarget);
            }
            if (lastSummonPos != nullptr) {
                slotData["last_summon_distance_xz"] = horse->current.pos.absXZ(*lastSummonPos);
            }
            if (player != nullptr) {
                slotData["owner_distance_xz"] = horse->current.pos.absXZ(player->current.pos);
            }
            slotData["lash_count"] = static_cast<int>(horse->getLashCount());
            const int reinPointCount = horse->getReinPointCount();
            slotData["rein_point_count"] = reinPointCount;
            slotData["rein_material"] =
                ptrString(reinterpret_cast<uintptr_t>(horse->getReinLineMaterial()));
            slotData["rein_points"] =
                reinPointSummary(horse->getReinPoints(), reinPointCount, horse->current.pos);
            slotData["rein_suppressed_by_subjectivity"] =
                horse->isRidden() && player != nullptr && player->checkHorseSubjectivity();
            slotData["localized_animation_count"] =
                coop::horse_owner::getLocalizedAnimationCount(horse);
            slotData["animations"] = json::array({
                {
                    {"index", static_cast<unsigned int>(horse->getAnmIdx(0))},
                    {"frame", horse->getAnmFrame(0)},
                    {"frame_max", horse->getAnmFrameMax(0)},
                },
                {
                    {"index", static_cast<unsigned int>(horse->getAnmIdx(1))},
                    {"frame", horse->getAnmFrame(1)},
                    {"frame_max", horse->getAnmFrameMax(1)},
                },
                {
                    {"index", static_cast<unsigned int>(horse->getAnmIdx(2))},
                    {"frame", horse->getAnmFrame(2)},
                    {"frame_max", horse->getAnmFrameMax(2)},
                },
            });
        }
        if (reins != nullptr) {
            slotData["rein_interp"] = {
                {"previous_valid", reins->previousValid},
                {"current_valid", reins->currentValid},
                {"previous_count", reins->previousCount},
                {"current_count", reins->currentCount},
            };
            slotData["rein_snapshot"] =
                reinPointSummary(reins->current, reins->currentCount,
                                 horse != nullptr ? horse->current.pos : cXyz::Zero);
        }
        slots.push_back(slotData);
    }

    return {
        {"schema_version", 4},
        {"canonical_horse", ptrString(reinterpret_cast<uintptr_t>(dComIfGp_getHorseActor()))},
        {"duplicate_retained_horse", duplicateRetainedHorse},
        {"last_summon_revision", coop::horse_owner::getLastSummonRevision()},
        {"last_summon_activator",
         static_cast<int>(coop::horse_owner::getLastSummonActivator())},
        {"last_summon_pos_valid", lastSummonPos != nullptr},
        {"last_summon_pos", vecSummary(lastSummonPos)},
        {"slots", slots},
    };
}

json collectEventPresentation() {
    const coop::event_presentation::DebugState& state =
        coop::event_presentation::getDebugState();
    json hiddenSlots = json::array();
    for (int i = 0; i < coop::kPlayerSlotCount; i++) {
        const coop::PlayerSlot slot = static_cast<coop::PlayerSlot>(i);
        if (coop::event_presentation::shouldHideSlot(slot)) {
            hiddenSlots.push_back(i);
        }
    }

    return {
        {"schema_version", 3},
        {"revision", state.revision},
        {"fullscreen", state.fullscreen},
        {"split_screen_capability", coop::camera::isSplitScreenEnabled()},
        {"split_viewports_presented", coop::event_presentation::shouldPresentSplitViewports()},
        {"presenter_slot", static_cast<int>(state.presenterSlot)},
        {"presenter_window", state.presenterWindowIndex},
        {"hide_non_presenter_visuals", state.hideNonPresenterVisuals},
        {"total_depth", state.totalDepth},
        {"sources", {
            {"wolf_howl", state.wolfHowlDepth},
            {"item_ring", state.itemRingDepth},
            {"pause_menu", state.pauseMenuDepth},
            {"field_map", state.fieldMapDepth},
            {"dungeon_map", state.dungeonMapDepth},
            {"agitha_insect", state.agithaInsectDepth},
            {"midna_service", state.midnaServiceDepth},
        }},
        {"last_transition", coop::event_presentation::transitionName(state.lastTransition)},
        {"last_source", coop::event_presentation::sourceName(state.lastSource)},
        {"hidden_slots", hiddenSlots},
    };
}

json collectAlinkFormResources() {
    const coop::alink_form_resources::DebugState& state =
        coop::alink_form_resources::getDebugState();

    json arcs = json::array();
    for (int i = 0; i < state.arcCount; i++) {
        const coop::alink_form_resources::DebugArcState& arc = state.arcs[i];
        arcs.push_back({
            {"arc", arc.arcName != nullptr ? arc.arcName : ""},
            {"heap", ptrString(arc.heap)},
            {"retain_count", arc.retainCount},
            {"phase_id", arc.phaseId},
            {"loaded", arc.loaded},
        });
    }

    json slots = json::array();
    for (int i = 0; i < coop::kPlayerSlotCount; i++) {
        const coop::alink_form_resources::DebugSlotState& slot = state.slots[i];
        slots.push_back({
            {"slot", static_cast<int>(slot.slot)},
            {"actor", ptrString(slot.actor)},
            {"current_arc", slot.currentArc != nullptr ? slot.currentArc : ""},
            {"pending_release_arc",
             slot.pendingReleaseArc != nullptr ? slot.pendingReleaseArc : ""},
            {"desired_wolf", slot.desiredWolf},
            {"desired_known", slot.desiredKnown},
            {"swapping", slot.swapping},
        });
    }

    return {
        {"schema_version", 1},
        {"revision", state.revision},
        {"arcs", arcs},
        {"slots", slots},
    };
}

json attentionObjectSummary(dAttention_c* attention, int slot) {
    json data = {
        {"slot", slot},
        {"available", attention != nullptr},
    };
    if (attention == nullptr) {
        return data;
    }

    data["ptr"] = ptrString(reinterpret_cast<uintptr_t>(attention));
    data["owner"] = actorSummary(attention->mpPlayer);
    data["pad_no"] = attention->mPadNo;
    data["player_attention_flags"] = attention->mPlayerAttentionFlags;
    data["flags"] = attention->mFlags;
    data["lockon_truth"] = attention->LockonTruth();
    data["lockon"] = attention->Lockon();
    data["lock_edge"] = attention->LockEdge();
    data["lock_target_pid"] = static_cast<int>(attention->mLockTargetID);
    data["target_actor_pid"] = static_cast<int>(attention->mTargetActorID);
    data["lockon_count"] = attention->GetLockonCount();
    data["lockon_offset"] = attention->mLockOnOffset;
    data["action_count"] = attention->GetActionCount();
    data["action_offset"] = attention->mActionOffset;
    data["check_object_count"] = attention->GetCheckObjectCount();
    data["check_object_offset"] = attention->mCheckObjectOffset;
    data["attn_status"] = static_cast<unsigned int>(attention->mAttnStatus);
    data["attn_button_state"] = static_cast<unsigned int>(attention->field_0x32b);
    data["attn_refresh_timer"] = static_cast<unsigned int>(attention->field_0x32e);
    data["attn_release_timer"] = static_cast<unsigned int>(attention->field_0x32f);
    data["attn_block_timer"] = attention->mAttnBlockTimer;
    data["lockon_target_0"] = actorSummary(attention->LockonTarget(0));
    data["action_target_0"] = actorSummary(attention->ActionTarget(0));
    data["check_object_target_0"] = actorSummary(attention->CheckObjectTarget(0));

    data["lockon_list_capacity"] = 8;
    data["lockon_list_active"] = attentionListSummary(attention->mLockOnList, 8);
    data["action_list_capacity"] = 4;
    data["action_list_active"] = attentionListSummary(attention->mActionList, 4);
    data["check_object_list_capacity"] = 4;
    data["check_object_list_active"] = attentionListSummary(attention->mCheckObjectList, 4);
    return data;
}

json collectAttentionState() {
    dAttention_c* attention = dComIfGp_getAttention();
    json data = {
        {"schema_version", 2},
        {"available", attention != nullptr},
    };
    if (attention == nullptr) {
        return data;
    }

    json primary = attentionObjectSummary(attention, 0);
    for (auto& item : primary.items()) {
        data[item.key()] = item.value();
    }

    json slots = json::array();
    for (int slot = 0; slot < coop::kPlayerSlotCount; slot++) {
        slots.push_back(attentionObjectSummary(coop::player_attention::existingAttentionForSlot(slot), slot));
    }
    data["slots"] = slots;
    return data;
}

json collectButtonStatus() {
    return {
        {"message", static_cast<unsigned int>(dComIfGp_getMesgStatus())},
        {"r", static_cast<unsigned int>(dComIfGp_getRStatus())},
        {"a", static_cast<unsigned int>(dComIfGp_getAStatus())},
        {"do", static_cast<unsigned int>(dComIfGp_getDoStatus())},
        {"z", static_cast<unsigned int>(dComIfGp_getZStatus())},
        {"three_d", static_cast<unsigned int>(dComIfGp_get3DStatus())},
        {"c_stick", static_cast<unsigned int>(dComIfGp_getCStickStatus())},
        {"s_button", static_cast<unsigned int>(dComIfGp_getSButtonStatus())},
        {"x", static_cast<unsigned int>(dComIfGp_getXStatus())},
        {"y", static_cast<unsigned int>(dComIfGp_getYStatus())},
    };
}

json collectButtonStatusForce() {
    return {
        {"r", static_cast<unsigned int>(dComIfGp_getRStatusForce())},
        {"a", static_cast<unsigned int>(dComIfGp_getAStatusForce())},
        {"do", static_cast<unsigned int>(dComIfGp_getDoStatusForce())},
        {"z", static_cast<unsigned int>(dComIfGp_getZStatusForce())},
        {"three_d", static_cast<unsigned int>(dComIfGp_get3DStatusForce())},
        {"c_stick", static_cast<unsigned int>(dComIfGp_getCStickStatusForce())},
        {"s_button", static_cast<unsigned int>(dComIfGp_getSButtonStatusForce())},
        {"x", static_cast<unsigned int>(dComIfGp_getXStatusForce())},
        {"y", static_cast<unsigned int>(dComIfGp_getYStatusForce())},
    };
}

json collectPlayerStatusWords() {
    json words = json::array();
    for (int i = 0; i < 4; i++) {
        words.push_back(static_cast<unsigned int>(g_dComIfG_gameInfo.play.mPlayerStatus[0][i]));
    }
    return words;
}

json collectCameraAttentionStatus() {
    json statuses = json::array();
    statuses.push_back(static_cast<unsigned int>(dComIfGp_getCameraAttentionStatus(0)));
    return statuses;
}

json collectPlayerStatus() {
    dAttention_c* attention = dComIfGp_getAttention();
    json data = {
        {"schema_version", 1},
        {"button_status", collectButtonStatus()},
        {"button_status_force", collectButtonStatusForce()},
        {"player_status_words", collectPlayerStatusWords()},
        {"camera_attention_status", collectCameraAttentionStatus()},
        {"attention_lock", attention != nullptr ? static_cast<bool>(attention->Lockon()) : false},
        {"attention_flags", attention != nullptr ? static_cast<unsigned int>(attention->mFlags) : 0},
        {"secondary_available", s_state.hasSecondaryAlinkState},
    };

    if (s_state.hasSecondaryAlinkState) {
        const SecondaryAlinkState& state = s_state.secondaryAlinkState;
        data["secondary_actor"] = ptrString(state.actor);
        data["secondary_proc"] = static_cast<unsigned int>(state.proc);
        data["secondary_attention_lock"] = static_cast<bool>(state.attentionLock);
        data["secondary_input_r"] = static_cast<bool>(state.inputR);
        data["secondary_item_button_r"] = static_cast<bool>(state.itemButtonR);
        data["secondary_item_trigger_r"] = static_cast<bool>(state.itemTriggerR);
        data["secondary_raw_mask"] = static_cast<unsigned int>(state.rawMask);
        data["secondary_target"] = ptrString(state.target);
    } else {
        data["secondary_attention_lock"] = false;
        data["secondary_raw_mask"] = 0;
    }

    return data;
}

json playerQueryActorSummary(const coop::PlayerQueryActorDebug& actor) {
    json data = {
        {"actor_uid", nullptr},
        {"ptr", ptrString(actor.ptr)},
        {"stable_actor_uid_deferred", true},
        {"available", actor.available},
    };
    if (!actor.available) {
        return data;
    }

    data["profile"] = actor.profile;
    data["id"] = actor.id;
    data["room"] = actor.room;
    data["argument"] = actor.argument;
    data["pos"] = {actor.pos[0], actor.pos[1], actor.pos[2]};
    data["angle_y"] = static_cast<int>(actor.angleY);
    return data;
}

json playerQueryCandidateSummary(const coop::PlayerQueryCandidateDebug& candidate) {
    return {
        {"slot", candidate.slot != coop::PlayerSlot::Invalid ? static_cast<int>(candidate.slot) : -1},
        {"actor", playerQueryActorSummary(candidate.actorDebug)},
        {"distance", candidate.distance},
        {"distance_xz", candidate.distanceXZ},
        {"angle_y", static_cast<int>(candidate.angleY)},
    };
}

json playerQueryDecisionSummary(const coop::PlayerQueryDecisionDebug& decision) {
    json candidates = json::array();
    for (int i = 0; i < decision.candidateCount && i < coop::kPlayerSlotCount; i++) {
        candidates.push_back(playerQueryCandidateSummary(decision.candidates[i]));
    }

    return {
        {"system", decision.system},
        {"observer", playerQueryActorSummary(decision.observerDebug)},
        {"found", decision.selected.found},
        {"selected_slot", decision.selected.slot != coop::PlayerSlot::Invalid
                              ? static_cast<int>(decision.selected.slot)
                              : -1},
        {"selected_actor", playerQueryActorSummary(decision.selectedActorDebug)},
        {"distance", decision.selected.distance},
        {"distance_xz", decision.selected.distanceXZ},
        {"angle_y", static_cast<int>(decision.selected.angleY)},
        {"candidates", candidates},
    };
}

json collectPlayerQuery() {
    const coop::PlayerQueryDebugState& state = coop::getPlayerQueryDebugState();
    json decisions = json::array();
    for (int i = 0; i < state.decisionCount; i++) {
        decisions.push_back(playerQueryDecisionSummary(state.decisions[i]));
    }

    return {
        {"schema_version", 1},
        {"decisions", decisions},
    };
}

json enemyTargetingDecisionSummary(const coop::EnemyTargetDecisionDebug& decision) {
    json candidates = json::array();
    for (int i = 0; i < decision.candidateCount && i < coop::kPlayerSlotCount; i++) {
        candidates.push_back(playerQueryCandidateSummary(decision.candidates[i]));
    }

    return {
        {"scope", coop::enemyTargetScopeName(decision.scope)},
        {"mode", coop::enemyTargetModeName(decision.mode)},
        {"label", decision.label},
        {"observer", playerQueryActorSummary(decision.observerDebug)},
        {"found", decision.selected.found},
        {"selected_slot", decision.selected.slot != coop::PlayerSlot::Invalid
                              ? static_cast<int>(decision.selected.slot)
                              : -1},
        {"selected_actor", playerQueryActorSummary(decision.selectedActorDebug)},
        {"distance", decision.selected.distance},
        {"distance_xz", decision.selected.distanceXZ},
        {"angle_y", static_cast<int>(decision.selected.angleY)},
        {"nearest_found", decision.nearest.found},
        {"nearest_slot", decision.nearest.slot != coop::PlayerSlot::Invalid
                             ? static_cast<int>(decision.nearest.slot)
                             : -1},
        {"nearest_actor", playerQueryActorSummary(decision.nearestActorDebug)},
        {"nearest_distance", decision.nearest.distance},
        {"nearest_distance_xz", decision.nearest.distanceXZ},
        {"nearest_angle_y", static_cast<int>(decision.nearest.angleY)},
        {"reason", coop::enemyTargetReasonName(decision.reason)},
        {"committed", decision.committed},
        {"changed", decision.changed},
        {"retained_valid", decision.retainedValid},
        {"retention_blocked_nearest", decision.retentionBlockedNearest},
        {"retain_seconds", decision.retainSeconds},
        {"sticky_elapsed_seconds", decision.stickyElapsedSeconds},
        {"current_sim_frame", static_cast<unsigned int>(decision.currentSimFrame)},
        {"last_updated_sim_frame", static_cast<unsigned int>(decision.lastUpdatedSimFrame)},
        {"candidates", candidates},
    };
}

json collectEnemyTargeting() {
    const coop::EnemyTargetingDebugState& state = coop::getEnemyTargetingDebugState();
    json decisions = json::array();
    for (int i = 0; i < state.decisionCount; i++) {
        decisions.push_back(enemyTargetingDecisionSummary(state.decisions[i]));
    }

    return {
        {"schema_version", 1},
        {"default_retain_seconds", coop::kDefaultEnemyTargetRetainSeconds},
        {"time_source", "simulation"},
        {"sim_pace_seconds", game_clock::sim_pace()},
        {"decisions", decisions},
    };
}

json selectedTargetStateSummary(
    const coop::selected_target_state::SelectedTargetState& state) {
    return {
        {"slot", state.slot != coop::PlayerSlot::Invalid ? static_cast<int>(state.slot) : -1},
        {"actor", playerQueryActorSummary(state.actorDebug)},
        {"available", state.available},
        {"pos", {state.pos.x, state.pos.y, state.pos.z}},
        {"shape_angle_y", static_cast<int>(state.shapeAngleY)},
        {"speed_f", state.speedF},
        {"cut_type", state.cutType},
        {"cut_count", state.cutCount},
        {"cut_active", state.cutActive},
        {"horse_ride", state.horseRide},
    };
}

json selectedTargetStateDecisionSummary(
    const coop::selected_target_state::SelectedTargetDecisionDebug& decision) {
    json data = selectedTargetStateSummary(decision.state);
    data["label"] = decision.label;
    data["observer"] = playerQueryActorSummary(decision.observerDebug);
    data["found"] = decision.found;
    data["reason"] =
        coop::selected_target_state::selectedTargetStateReasonName(decision.reason);
    return data;
}

json collectSelectedTargetState() {
    const coop::selected_target_state::SelectedTargetDebugState& state =
        coop::selected_target_state::getSelectedTargetDebugState();
    json decisions = json::array();
    for (int i = 0; i < state.decisionCount; i++) {
        decisions.push_back(selectedTargetStateDecisionSummary(state.decisions[i]));
    }

    return {
        {"schema_version", 1},
        {"decisions", decisions},
    };
}

json damageActorSummary(const coop::damage_owner::DamageActorDebug& actor) {
    json data = {
        {"actor_uid", nullptr},
        {"ptr", ptrString(actor.ptr)},
        {"stable_actor_uid_deferred", true},
        {"available", actor.available},
    };
    if (!actor.available) {
        return data;
    }

    data["profile"] = actor.profile;
    data["name"] = actor.name;
    data["id"] = actor.id;
    data["room"] = actor.room;
    data["argument"] = actor.argument;
    data["pos"] = {actor.pos[0], actor.pos[1], actor.pos[2]};
    data["angle_y"] = static_cast<int>(actor.angleY);
    return data;
}

json damageOwnerHitSummary(const coop::damage_owner::DamageOwnerHitDebug& hit) {
    const coop::damage_owner::DamageOwnerResult& owner = hit.owner;
    return {
        {"event_id", static_cast<unsigned long long>(hit.eventId)},
        {"sim_frame", static_cast<unsigned int>(hit.simFrame)},
        {"label", hit.label},
        {"victim", damageActorSummary(owner.victimDebug)},
        {"hit_actor", damageActorSummary(owner.hitActorDebug)},
        {"owner", damageActorSummary(owner.ownerDebug)},
        {"owner_slot", owner.slot != coop::PlayerSlot::Invalid ? static_cast<int>(owner.slot) : -1},
        {"found", owner.found},
        {"reason", coop::damage_owner::damageOwnerReasonName(owner.reason)},
        {"attack_type", owner.attackType},
        {"attack_name", coop::damage_owner::damageOwnerAttackName(owner.attackType)},
        {"collider_atp", static_cast<unsigned int>(owner.atp)},
        {"collider_special", owner.special},
        {"hit_type", hit.hitType},
        {"hit_type_name", coop::damage_owner::damageOwnerHitTypeName(hit.hitType)},
        {"attack_power", static_cast<unsigned int>(hit.attackPower)},
        {"hit_status", hit.hitStatus},
        {"cut_type", owner.cutType},
        {"cut_type_name", coop::damage_owner::damageOwnerCutTypeName(owner.cutType)},
        {"cut_count", owner.cutCount},
        {"reaction_mode", hit.reactionMode},
        {"hit_pos", {hit.hitPos.x, hit.hitPos.y, hit.hitPos.z}},
    };
}

json collectDamageOwner() {
    const coop::damage_owner::DamageOwnerDebugState& state =
        coop::damage_owner::getDamageOwnerDebugState();
    json hits = json::array();
    for (int i = 0; i < state.hitCount; i++) {
        if (state.hits[i].eventId == 0) {
            continue;
        }
        hits.push_back(damageOwnerHitSummary(state.hits[i]));
    }

    return {
        {"schema_version", 1},
        {"hits", hits},
    };
}

json defenderActorSummary(const coop::defender_owner::DefenderActorDebug& actor) {
    json data = {
        {"actor_uid", nullptr},
        {"ptr", ptrString(actor.ptr)},
        {"stable_actor_uid_deferred", true},
        {"available", actor.available},
    };
    if (!actor.available) {
        return data;
    }

    data["profile"] = actor.profile;
    data["name"] = actor.name;
    data["id"] = actor.id;
    data["room"] = actor.room;
    data["argument"] = actor.argument;
    data["pos"] = {actor.pos[0], actor.pos[1], actor.pos[2]};
    data["angle_y"] = static_cast<int>(actor.angleY);
    return data;
}

json defenderOwnerDecisionSummary(
    const coop::defender_owner::DefenderOwnerDecisionDebug& decision) {
    const coop::defender_owner::DefenderOwnerResult& defender = decision.defender;
    return {
        {"event_id", static_cast<unsigned long long>(decision.eventId)},
        {"sim_frame", static_cast<unsigned int>(decision.simFrame)},
        {"label", decision.label},
        {"attacker", defenderActorSummary(defender.attackerDebug)},
        {"hit_actor", defenderActorSummary(defender.hitActorDebug)},
        {"defender", defenderActorSummary(defender.defenderDebug)},
        {"defender_slot", defender.slot != coop::PlayerSlot::Invalid
                              ? static_cast<int>(defender.slot)
                              : -1},
        {"found", defender.found},
        {"guarded", defender.guarded},
        {"guard_break", defender.guardBreak},
        {"at_shield_hit", defender.atShieldHit},
        {"target_shield", defender.targetShield},
        {"target_special_shield", defender.targetSpecialShield},
        {"target_small_shield", defender.targetSmallShield},
        {"target_shield_hit", defender.targetShieldHit},
        {"reason", coop::defender_owner::defenderOwnerReasonName(defender.reason)},
        {"hit_pos", {defender.hitPos.x, defender.hitPos.y, defender.hitPos.z}},
    };
}

json collectDefenderOwner() {
    const coop::defender_owner::DefenderOwnerDebugState& state =
        coop::defender_owner::getDefenderOwnerDebugState();
    json decisions = json::array();
    for (int i = 0; i < state.decisionCount; i++) {
        if (state.decisions[i].eventId == 0) {
            continue;
        }
        decisions.push_back(defenderOwnerDecisionSummary(state.decisions[i]));
    }

    return {
        {"schema_version", 1},
        {"decisions", decisions},
    };
}

json caughtStunActorSummary(const coop::caught_stun_owner::CaughtStunActorDebug& actor) {
    json data = {
        {"actor_uid", nullptr},
        {"ptr", ptrString(actor.ptr)},
        {"stable_actor_uid_deferred", true},
        {"available", actor.available},
    };
    if (!actor.available) {
        return data;
    }

    data["profile"] = actor.profile;
    data["name"] = actor.name;
    data["id"] = actor.id;
    data["room"] = actor.room;
    data["argument"] = actor.argument;
    data["pos"] = {actor.pos[0], actor.pos[1], actor.pos[2]};
    data["angle_y"] = static_cast<int>(actor.angleY);
    return data;
}

json caughtStunOwnerDecisionSummary(
    const coop::caught_stun_owner::CaughtStunOwnerDecisionDebug& decision) {
    const coop::caught_stun_owner::CaughtStunOwnerState& state = decision.state;
    json affectedSlots = json::array();
    for (int i = 0; i < state.affectedCount; i++) {
        affectedSlots.push_back({
            {"slot", state.affectedSlots[i] != coop::PlayerSlot::Invalid
                         ? static_cast<int>(state.affectedSlots[i])
                         : -1},
            {"local_actor", actorSummary(state.affectedLocalActors[i])},
        });
    }

    return {
        {"event_id", static_cast<unsigned long long>(decision.eventId)},
        {"sim_frame", static_cast<unsigned int>(decision.simFrame)},
        {"label", decision.label},
        {"enemy", caughtStunActorSummary(state.enemyDebug)},
        {"player", caughtStunActorSummary(state.playerDebug)},
        {"owner_slot", state.slot != coop::PlayerSlot::Invalid ? static_cast<int>(state.slot) : -1},
        {"affected_count", state.affectedCount},
        {"affected_slots", affectedSlots},
        {"found", state.found},
        {"active", state.active},
        {"reason", coop::caught_stun_owner::caughtStunOwnerReasonName(state.reason)},
        {"stun_timer", state.stunTimer},
        {"cry_timer", state.cryTimer},
    };
}

json collectCaughtStunOwner() {
    const coop::caught_stun_owner::CaughtStunOwnerDebugState& state =
        coop::caught_stun_owner::getCaughtStunOwnerDebugState();
    json decisions = json::array();
    for (int i = 0; i < state.decisionCount; i++) {
        decisions.push_back(caughtStunOwnerDecisionSummary(state.decisions[i]));
    }

    return {
        {"schema_version", 1},
        {"decisions", decisions},
    };
}

json bokoblinAttackProbeSummary(
    const coop::bokoblin_attack_probe::BokoblinAttackProbe& probe) {
    return {
        {"event_id", static_cast<unsigned long long>(probe.eventId)},
        {"sim_frame", static_cast<unsigned int>(probe.simFrame)},
        {"actor", ptrString(probe.actor)},
        {"actor_id", probe.actorId},
        {"action", probe.action},
        {"state", probe.state},
        {"bck", probe.bck},
        {"anim_frame", probe.animFrame},
        {"play_speed", probe.playSpeed},
        {"speed_f", probe.speedF},
        {"target_slot", probe.targetSlot != coop::PlayerSlot::Invalid
                            ? static_cast<int>(probe.targetSlot)
                            : -1},
        {"target_found", probe.targetFound},
        {"target_distance", probe.targetDistance},
        {"target_angle_y", static_cast<int>(probe.targetAngleY)},
        {"attack_animation_started", probe.attackAnimationStarted},
        {"attack_active_window", probe.attackActiveWindow},
        {"guarded_hit", probe.guardedHit},
        {"sphere0_hit", probe.sphere0Hit},
        {"sphere1_hit", probe.sphere1Hit},
        {"pre_active_window_hit", probe.preActiveWindowHit},
        {"pre_active_window_guarded", probe.preActiveWindowGuarded},
        {"first_hit_frame", static_cast<unsigned int>(probe.firstHitFrame)},
        {"first_hit_anim_frame", probe.firstHitAnimFrame},
        {"first_hit_before_active_window", probe.firstHitBeforeActiveWindow},
        {"first_hit_guarded", probe.firstHitGuarded},
        {"sphere0_defender_slot", probe.sphere0.slot != coop::PlayerSlot::Invalid
                                      ? static_cast<int>(probe.sphere0.slot)
                                      : -1},
        {"sphere1_defender_slot", probe.sphere1.slot != coop::PlayerSlot::Invalid
                                      ? static_cast<int>(probe.sphere1.slot)
                                      : -1},
        {"sphere0_guarded", probe.sphere0.guarded},
        {"sphere1_guarded", probe.sphere1.guarded},
        {"sphere0_shield_hit", probe.sphere0.targetShieldHit},
        {"sphere1_shield_hit", probe.sphere1.targetShieldHit},
        {"attack_run_frames", static_cast<unsigned int>(probe.attackRunFrames)},
        {"loop_suspect", probe.loopSuspect},
    };
}

json collectBokoblinAttackProbe() {
    const coop::bokoblin_attack_probe::BokoblinAttackProbeDebugState& state =
        coop::bokoblin_attack_probe::getBokoblinAttackProbeDebugState();
    json probes = json::array();
    for (int i = 0; i < state.probeCount; i++) {
        if (state.probes[i].eventId == 0) {
            continue;
        }
        probes.push_back(bokoblinAttackProbeSummary(state.probes[i]));
    }

    return {
        {"schema_version", 1},
        {"probes", probes},
    };
}

json gibdoStateProbeSummary(const coop::gibdo_state_probe::GibdoStateProbe& probe) {
    return {
        {"event_id", static_cast<unsigned long long>(probe.eventId)},
        {"sim_frame", static_cast<unsigned int>(probe.simFrame)},
        {"actor", ptrString(probe.actor)},
        {"actor_id", probe.actorId},
        {"label", probe.label != nullptr ? probe.label : ""},
        {"action", probe.action},
        {"move_mode", probe.moveMode},
        {"bck", probe.bck},
        {"anim_frame", probe.animFrame},
        {"play_speed", probe.playSpeed},
        {"speed_f", probe.speedF},
        {"attack_delay", probe.attackDelay},
        {"stun_timer", probe.stunTimer},
        {"cry_timer", probe.cryTimer},
        {"target_slot", probe.targetSlot != coop::PlayerSlot::Invalid
                            ? static_cast<int>(probe.targetSlot)
                            : -1},
        {"target_found", probe.targetFound},
        {"target_distance", probe.targetDistance},
        {"target_angle_y", static_cast<int>(probe.targetAngleY)},
        {"range_gate", probe.rangeGate},
        {"angle_gate", probe.angleGate},
        {"los_clear", probe.losClear},
        {"delay_gate", probe.delayGate},
        {"attack_gate", probe.attackGate},
        {"attack_start", probe.attackStart},
        {"scream_owner_active", probe.screamOwnerActive},
        {"scream_owner_attack_started", probe.screamOwnerAttackStarted},
        {"cry_owner", ptrString(probe.cryOwner)},
        {"state_run_frames", static_cast<unsigned int>(probe.stateRunFrames)},
        {"loop_suspect", probe.loopSuspect},
    };
}

json collectGibdoStateProbe() {
    const coop::gibdo_state_probe::GibdoStateProbeDebugState& state =
        coop::gibdo_state_probe::getGibdoStateProbeDebugState();
    json probes = json::array();
    for (int i = 0; i < state.probeCount; i++) {
        if (state.probes[i].eventId == 0) {
            continue;
        }
        probes.push_back(gibdoStateProbeSummary(state.probes[i]));
    }

    return {
        {"schema_version", 1},
        {"probes", probes},
    };
}

json youngGohmaStateProbeSummary(
    const coop::young_gohma_state_probe::YoungGohmaStateProbe& probe) {
    return {
        {"event_id", static_cast<unsigned long long>(probe.eventId)},
        {"sim_frame", static_cast<unsigned int>(probe.simFrame)},
        {"actor", ptrString(probe.actor)},
        {"actor_id", probe.actorId},
        {"label", probe.label != nullptr ? probe.label : ""},
        {"action", probe.action},
        {"sub_action", probe.subAction},
        {"bck", probe.bck},
        {"anim_frame", probe.animFrame},
        {"play_speed", probe.playSpeed},
        {"speed_f", probe.speedF},
        {"target_slot", probe.targetSlot != coop::PlayerSlot::Invalid
                            ? static_cast<int>(probe.targetSlot)
                            : -1},
        {"target_found", probe.targetFound},
        {"target_distance", probe.targetDistance},
        {"target_angle_y", static_cast<int>(probe.targetAngleY)},
        {"angle_diff", static_cast<int>(probe.angleDiff)},
        {"check_range", probe.checkRange},
        {"check_angle", static_cast<int>(probe.checkAngle)},
        {"range_gate", probe.rangeGate},
        {"angle_gate", probe.angleGate},
        {"los_clear", probe.losClear},
        {"pl_check", probe.plCheck},
        {"attack_collider_active", probe.attackColliderActive},
        {"state_run_frames", static_cast<unsigned int>(probe.stateRunFrames)},
        {"loop_suspect", probe.loopSuspect},
    };
}

json collectYoungGohmaStateProbe() {
    const coop::young_gohma_state_probe::YoungGohmaStateProbeDebugState& state =
        coop::young_gohma_state_probe::getYoungGohmaStateProbeDebugState();
    json probes = json::array();
    for (int i = 0; i < state.probeCount; i++) {
        if (state.probes[i].eventId == 0) {
            continue;
        }
        probes.push_back(youngGohmaStateProbeSummary(state.probes[i]));
    }

    return {
        {"schema_version", 1},
        {"probes", probes},
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
        {"schema_version", 2},
        {"secondary_alink_probe_flags", coop::getSecondaryAlinkProbeFlags()},
        {"skip_execute", coop::hasSecondaryAlinkProbeFlag(coop::SecondaryAlinkProbe_SkipExecute)},
        {"skip_draw", coop::hasSecondaryAlinkProbeFlag(coop::SecondaryAlinkProbe_SkipDraw)},
        {"model_data_owner_policy", "scoped_runtime"},
    };
}

json collectAlinkSecondary() {
    const SecondaryAlinkState& state = s_state.secondaryAlinkState;
    json data = {
        {"schema_version", 4},
        {"available", s_state.hasSecondaryAlinkState},
    };
    if (!s_state.hasSecondaryAlinkState) {
        return data;
    }

    data["phase"] = state.phase != nullptr ? state.phase : "";
    data["actor"] = ptrString(state.actor);
    data["proc"] = static_cast<unsigned int>(state.proc);
    data["proc_name"] = alinkProcName(state.proc);
    data["equip_item"] = static_cast<unsigned int>(state.equipItem);
    data["select_item_id"] = static_cast<unsigned int>(state.selectItemId);
    data["item_actor"] = ptrString(state.itemActor);
    data["item_actor_id"] = static_cast<int>(state.itemActorId);
    data["item_actor_name"] = static_cast<int>(state.itemActorName);
    data["ride_actor"] = ptrString(state.rideActor);
    data["ride_actor_id"] = static_cast<int>(state.rideActorId);
    data["ride_actor_name"] = static_cast<int>(state.rideActorName);
    data["ride_status"] = static_cast<unsigned int>(state.rideStatus);
    data["throw_boomerang_actor"] = ptrString(state.throwBoomerangActor);
    const bool bombActive = state.activeBombCount != 0 || state.insectBombCount != 0 ||
                            state.equipItem == dItemNo_NORMAL_BOMB_e ||
                            state.equipItem == dItemNo_WATER_BOMB_e ||
                            state.equipItem == dItemNo_POKE_BOMB_e;
    if (bombActive) {
        data["bomb"] = {
            {"active_count", static_cast<unsigned int>(state.activeBombCount)},
            {"insect_count", static_cast<unsigned int>(state.insectBombCount)},
            {"item_actor", ptrString(state.itemActor)},
            {"item_actor_id", static_cast<int>(state.itemActorId)},
            {"item_actor_name", static_cast<int>(state.itemActorName)},
        };
    }
    const bool copyRodActive = state.copyRodActor != 0 || state.copyRodControlActor != 0 ||
                               state.copyRodCameraActor != 0 || state.copyRodTopUse;
    if (copyRodActive) {
        data["copy_rod"] = {
            {"actor", ptrString(state.copyRodActor)},
            {"control_actor", ptrString(state.copyRodControlActor)},
            {"camera_actor", ptrString(state.copyRodCameraActor)},
            {"top_use", state.copyRodTopUse},
        };
    }
    data["item_button"] = static_cast<unsigned int>(state.itemButton);
    data["item_trigger"] = static_cast<unsigned int>(state.itemTrigger);
    data["use_button_flags"] = static_cast<unsigned int>(state.useButtonFlags);
    data["previous_use_button_flags"] = static_cast<unsigned int>(state.previousUseButtonFlags);
    data["speed_f"] = state.speedF;
    data["normal_speed"] = state.normalSpeed;
    data["stick_value"] = state.stickValue;
    data["move_value"] = state.moveValue;
    data["stick_angle"] = static_cast<int>(state.stickAngle);
    data["move_angle"] = static_cast<int>(state.moveAngle);
    data["current_angle_y"] = static_cast<int>(state.currentAngleY);
    data["shape_angle_y"] = static_cast<int>(state.shapeAngleY);
    data["pos"] = {state.posX, state.posY, state.posZ};
    data["stick_active"] = state.stickValue > 0.05f;
    data["move_active"] = state.moveValue > 0.05f;
    data["speed_active"] = state.speedF > 0.05f || state.speedF < -0.05f;
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

json collectHudPresentation() {
    const coop::hud_diagnostics::HudPresentationDebugState& state =
        coop::hud_diagnostics::getState();
    json slotItems = json::array();
    for (int slot = 0; slot < 2; slot++) {
        json items = json::array();
        for (int item = 0; item < 2; item++) {
            const coop::hud_diagnostics::ItemResolverDebug& resolved = state.slotItems[slot][item];
            items.push_back({
                {"button", item == 0 ? "x" : "y"},
                {"select_index", static_cast<unsigned int>(resolved.selectIndex)},
                {"mix_index", static_cast<unsigned int>(resolved.mixIndex)},
                {"item", static_cast<unsigned int>(resolved.item)},
                {"count", static_cast<int>(resolved.count)},
                {"max_count", resolved.maxCount},
            });
        }
        slotItems.push_back({
            {"slot", slot},
            {"items", items},
        });
    }

    json snapshots = json::array();
    for (int i = 0; i < static_cast<int>(coop::hud_diagnostics::ReplayPhase::Count); i++) {
        const coop::hud_diagnostics::ReplaySnapshot& snapshot = state.snapshots[i];
        if (!snapshot.valid) {
            continue;
        }

        json items = json::array();
        for (int item = 0; item < 2; item++) {
            const coop::hud_diagnostics::ItemResolverDebug& resolved = snapshot.resolved[item];
            const coop::hud_diagnostics::ItemPaneDebug& pane = snapshot.panes[item];
            items.push_back({
                {"button", item == 0 ? "x" : "y"},
                {"select_index", static_cast<unsigned int>(resolved.selectIndex)},
                {"mix_index", static_cast<unsigned int>(resolved.mixIndex)},
                {"item", static_cast<unsigned int>(resolved.item)},
                {"count", static_cast<int>(resolved.count)},
                {"max_count", resolved.maxCount},
                {"pane", {
                    {"visible", pane.visible},
                    {"texture_visible", pane.textureVisible},
                    {"alpha", static_cast<unsigned int>(pane.alpha)},
                    {"alpha_rate", pane.alphaRate},
                    {"third_digit_visible", pane.thirdDigitVisible},
                    {"translate", {pane.translateX, pane.translateY}},
                    {"scale", {pane.scaleX, pane.scaleY}},
                }},
            });
        }

        snapshots.push_back({
            {"phase", coop::hud_diagnostics::replayPhaseName(snapshot.phase)},
            {"presentation_slot", snapshot.presentationSlot != coop::PlayerSlot::Invalid
                                      ? static_cast<int>(snapshot.presentationSlot)
                                      : -1},
            {"do_status", static_cast<unsigned int>(snapshot.doStatus)},
            {"items", items},
        });
    }

    const coop::hud_diagnostics::HudPresentationDebugState::RingAdmissionDebug& ring =
        state.ringAdmission;
    json ringAdmission = {
        {"valid", ring.valid},
        {"phase", coop::hud_diagnostics::ringAdmissionPhaseName(ring.phase)},
        {"owner_slot", ring.owner != coop::PlayerSlot::Invalid ? static_cast<int>(ring.owner) : -1},
        {"heap_lock", static_cast<unsigned int>(ring.heapLock)},
        {"sub_heap_locks", {static_cast<unsigned int>(ring.subHeapLocks[0]),
                            static_cast<unsigned int>(ring.subHeapLocks[1])}},
        {"primary_prompt", ring.primaryPrompt},
        {"secondary_prompt", ring.secondaryPrompt},
        {"message_status", static_cast<unsigned int>(ring.messageStatus)},
        {"floating_message_visible", ring.floatingMessageVisible},
    };

    return {
        {"schema_version", 2},
        {"revision", state.revision},
        {"slot_items", slotItems},
        {"snapshots", snapshots},
        {"ring_admission", ringAdmission},
    };
}

json collectRenderLines() {
    constexpr f32 kWideRibbonDistance = 500.0f;
    constexpr f32 kLongSegmentDistance = 1000.0f;
    constexpr f32 kDistanceBucketSize = 10.0f;
    json records = json::array();
    const coop::line_render_diagnostics::ExpansionRecord* expansions =
        coop::line_render_diagnostics::getRecords();
    const int count = coop::line_render_diagnostics::getRecordCount();
    for (int i = 0; i < count; i++) {
        const coop::line_render_diagnostics::ExpansionRecord& expansion = expansions[i];
        const bool suspicious = expansion.controlNonFiniteCount != 0 ||
                                expansion.expandedNonFiniteCount != 0 ||
                                expansion.maxControlSegment > kLongSegmentDistance ||
                                expansion.maxExpandedWidth > kWideRibbonDistance;
        json record = {
            {"material", ptrString(expansion.material)},
            {"material_id", expansion.materialId},
            {"line_kind", expansion.lineKind},
            {"line_index", expansion.lineIndex},
            {"point_count", expansion.pointCount},
            {"presentation_refresh", expansion.presentationRefresh},
            {"actor_expansion_count", expansion.actorExpansionCount},
            {"presentation_refresh_request_count", expansion.presentationRefreshRequestCount},
            {"presentation_expansion_count", expansion.presentationExpansionCount},
            {"last_presentation_eye",
             {expansion.lastPresentationEye.x, expansion.lastPresentationEye.y,
              expansion.lastPresentationEye.z}},
            {"control_non_finite_count", expansion.controlNonFiniteCount},
            {"expanded_non_finite_count", expansion.expandedNonFiniteCount},
            {"max_control_segment_bucket",
             static_cast<int>(expansion.maxControlSegment / kDistanceBucketSize)},
            {"max_expanded_width_bucket",
             static_cast<int>(expansion.maxExpandedWidth / kDistanceBucketSize)},
            {"max_expanded_distance_from_eye_bucket",
             static_cast<int>(expansion.maxExpandedDistanceFromEye / kDistanceBucketSize)},
            {"suspicious", suspicious},
        };
        if (suspicious) {
            record["eye"] = {expansion.eye.x, expansion.eye.y, expansion.eye.z};
            record["max_control_segment"] = expansion.maxControlSegment;
            record["max_expanded_width"] = expansion.maxExpandedWidth;
            record["max_expanded_distance_from_eye"] = expansion.maxExpandedDistanceFromEye;
        }
        records.push_back(record);
    }
    return {
        {"schema_version", 2},
        {"revision", coop::line_render_diagnostics::getRevision()},
        {"records", records},
    };
}

Provider s_providers[] = {
    {"scene.current", 1, "cheap", 30, true, 20, 4096, collectSceneCurrent},
    {"render.stats", 1, "cheap", 30, true, 20, 4096, collectRenderStats},
    {"render.windows", 2, "cheap", 1, true, 20, 8192, collectRenderWindows},
    {"camera.state", 2, "cheap", 1, true, 20, 8192, collectCameraState},
    {"camera.area_load", 1, "cheap", 1, true, 120, 32768, collectCameraAreaLoad},
    {"player.slots", 2, "cheap", 1, true, 120, 8192, collectPlayerSlots},
    {"horse.owner", 4, "cheap", 1, true, 120, 12288, collectHorseOwner},
    {"event.presentation", 2, "cheap", 1, true, 120, 4096, collectEventPresentation},
    {"alink.form_resources", 2, "cheap", 1, true, 120, 8192, collectAlinkFormResources},
    {"render.lines", 2, "cheap", 1, true, 120, 32768, collectRenderLines},
    {"input.pad", 1, "cheap", 1, true, 120, 4096, collectInputPad},
    {"attention.state", 2, "medium", 5, true, 60, 32768, collectAttentionState},
    {"player.status", 1, "cheap", 1, true, 120, 8192, collectPlayerStatus},
    {"coop.player_query", 1, "cheap", 5, true, 240, 8192, collectPlayerQuery},
    {"enemy.targeting", 1, "cheap", 5, true, 240, 12288, collectEnemyTargeting},
    {"selected_target.state", 1, "cheap", 5, true, 240, 8192, collectSelectedTargetState},
    {"damage.owner", 1, "cheap", 1, true, 600, 12288, collectDamageOwner},
    {"defender.owner", 1, "cheap", 1, true, 600, 12288, collectDefenderOwner},
    {"caught_stun.owner", 1, "cheap", 1, true, 240, 8192, collectCaughtStunOwner},
    {"bokoblin.attack", 1, "cheap", 1, true, 240, 8192, collectBokoblinAttackProbe},
    {"gibdo.state", 1, "cheap", 1, true, 240, 8192, collectGibdoStateProbe},
    {"young_gohma.state", 1, "cheap", 1, true, 240, 8192, collectYoungGohmaStateProbe},
    {"coop.probes", 2, "cheap", 30, true, 20, 4096, collectCoopProbes},
    {"alink.secondary", 4, "cheap", 1, true, 120, 8192, collectAlinkSecondary},
    {"hud.presentation", 2, "cheap", 1, true, 120, 8192, collectHudPresentation},
};

const Provider* findProvider(const char* name) {
    for (const Provider& provider : s_providers) {
        if (std::string(provider.name) == name) {
            return &provider;
        }
    }
    return nullptr;
}

json collectDiagnosticsStats() {
    json providers = json::object();
    for (const Provider& provider : s_providers) {
        const ProviderStats& stats = s_state.providerStats[provider.name];
        providers[provider.name] = {
            {"events_written", stats.eventsWritten},
            {"bytes_written", stats.bytesWritten},
            {"events_throttled", stats.eventsThrottled},
            {"payloads_oversized", stats.payloadsOversized},
            {"window_events", stats.windowEvents},
            {"max_events_per_minute", provider.maxEventsPerMinute},
            {"max_payload_bytes", provider.maxPayloadBytes},
        };
    }

    const ProviderStats& throttledStats = s_state.providerStats["diagnostics.throttled"];
    providers["diagnostics.throttled"] = {
        {"events_written", throttledStats.eventsWritten},
        {"bytes_written", throttledStats.bytesWritten},
    };

    return {
        {"schema_version", 1},
        {"events_buffered", s_state.ring.size()},
        {"events_buffer_max", kRingBufferMaxEvents},
        {"providers", providers},
    };
}

json buildManifest() {
    json providers = json::array();
    for (const Provider& provider : s_providers) {
        providers.push_back({
            {"name", provider.name},
            {"schema_version", provider.schemaVersion},
            {"cost_class", provider.costClass},
            {"sample_every_frames", provider.sampleEveryFrames},
            {"emit_on_change", provider.emitOnChange},
            {"max_events_per_minute", provider.maxEventsPerMinute},
            {"max_payload_bytes", provider.maxPayloadBytes},
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
    json providers = s_state.latest.is_object() ? s_state.latest : json::object();
    providers["diagnostics.stats"] = collectDiagnosticsStats();
    json latest = {
        {"event_version", kEventVersion},
        {"session_id", s_state.sessionId},
        {"role", s_state.role},
        {"frame", s_state.lastFrame},
        {"time_us", monotonicTimeUs()},
        {"profile", kProfileSecondaryAlinkActionMirror},
        {"providers", providers},
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
        if (std::string(provider.name) == "camera.area_load" &&
            data.value("revision", 0u) == 0)
        {
            continue;
        }
        if (std::string(provider.name) == "coop.player_query") {
            emitPlayerQueryEvents(provider, data);
            continue;
        }
        if (std::string(provider.name) == "enemy.targeting") {
            emitEnemyTargetingEvents(provider, data);
            continue;
        }
        if (std::string(provider.name) == "selected_target.state") {
            emitSelectedTargetStateEvents(provider, data);
            continue;
        }
        if (std::string(provider.name) == "damage.owner") {
            emitDamageOwnerEvents(provider, data);
            continue;
        }
        if (std::string(provider.name) == "defender.owner") {
            emitDefenderOwnerEvents(provider, data);
            continue;
        }
        if (std::string(provider.name) == "caught_stun.owner") {
            emitCaughtStunOwnerEvents(provider, data);
            continue;
        }
        if (std::string(provider.name) == "bokoblin.attack") {
            emitBokoblinAttackProbeEvents(provider, data);
            continue;
        }
        if (std::string(provider.name) == "gibdo.state") {
            emitGibdoStateProbeEvents(provider, data);
            continue;
        }
        if (std::string(provider.name) == "young_gohma.state") {
            emitYoungGohmaStateProbeEvents(provider, data);
            continue;
        }

        json eventKey = eventKeyForProvider(provider.name, data);
        if (provider.emitOnChange && !shouldEmitProviderEvent(provider.name, eventKey)) {
            continue;
        }

        json eventData = data;
        if (std::string(provider.name) == "camera.state") {
            eventData["event_context"] = {
                {"player_slots", collectPlayerSlots()},
            };
        }
        emitProviderEvent(provider, "snapshot", eventData);
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
    json event = makeEnvelope("diagnostics.flush", 1, "flush", data);
    storeEventDirect(event);
    recordProviderWrite("diagnostics.flush", event.dump().size());
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
    json eventKey = eventKeyForProvider("alink.secondary", data);
    if (!shouldEmitProviderEvent("alink.secondary", eventKey)) {
        updateLatestFileIfDue(false);
        return;
    }

    const Provider* provider = findProvider("alink.secondary");
    if (provider != nullptr) {
        emitProviderEvent(*provider, "change", data);
    } else {
        storeEvent(makeEnvelope("alink.secondary", 1, "change", data));
    }
    updateLatestFileIfDue(false);
}

void recordCameraAreaLoadCheckpoint(const char* phase, const char* startupSource, int cameraId,
                                    const fopAc_ac_c* actor, const cXyz* center, const cXyz* eye,
                                    s16 cameraYaw, int startMode, int cameraFrame) {
    const cXyz& restartCenter = dComIfGs_getTurnRestart().getCameraCtr();
    const cXyz& restartEye = dComIfGs_getTurnRestart().getCameraEye();
    const char* stage = dComIfGp_getStartStageName();
    json checkpoint = {
        {"revision", ++s_state.cameraAreaLoadRevision},
        {"phase", phase != nullptr ? phase : ""},
        {"startup_source", startupSource != nullptr ? startupSource : ""},
        {"camera_id", cameraId},
        {"camera_frame", cameraFrame},
        {"camera_yaw", static_cast<int>(cameraYaw)},
        {"actor", ptrString(reinterpret_cast<uintptr_t>(actor))},
        {"actor_room", actor != nullptr ? static_cast<int>(fopAcM_GetRoomNo(actor)) : -1},
        {"actor_argument", actor != nullptr ? static_cast<int>(actor->argument) : 0},
        {"actor_angle_y", actor != nullptr ? static_cast<int>(actor->shape_angle.y) : 0},
        {"actor_pos", actor != nullptr
                          ? json{actor->current.pos.x, actor->current.pos.y, actor->current.pos.z}
                          : json::array()},
        {"camera_center", center != nullptr ? json{center->x, center->y, center->z} : json::array()},
        {"camera_eye", eye != nullptr ? json{eye->x, eye->y, eye->z} : json::array()},
        {"turn_restart_center", {restartCenter.x, restartCenter.y, restartCenter.z}},
        {"turn_restart_eye", {restartEye.x, restartEye.y, restartEye.z}},
        {"turn_restart_angle_y", static_cast<int>(dComIfGs_getTurnRestartAngleY())},
        {"last_scene_mode", static_cast<unsigned int>(dComIfGs_getLastSceneMode())},
        {"start_mode", startMode},
        {"stage", stage != nullptr ? stage : ""},
        {"stage_room", static_cast<int>(dComIfGp_getStartStageRoomNo())},
        {"stage_layer", static_cast<int>(dComIfGp_getStartStageLayer())},
        {"stage_point", static_cast<int>(dComIfGp_getStartStagePoint())},
        {"split_screen_enabled", coop::camera::isSplitScreenEnabled()},
        {"split_screen_requested", coop::camera::isSplitScreenRequested()},
        {"secondary_ready", coop::camera::isSecondaryCameraReady()},
        {"secondary_requested", coop::camera::isSecondaryCameraRequested()},
    };
    s_state.cameraAreaLoadCheckpoints.push_back(checkpoint);
    while (s_state.cameraAreaLoadCheckpoints.size() > kCameraAreaLoadMaxCheckpoints) {
        s_state.cameraAreaLoadCheckpoints.pop_front();
    }

    if (!s_state.enabled) {
        return;
    }

    ensureInitialized();
    if (!s_state.initialized) {
        return;
    }

    updateProviderLatest("camera.area_load", collectCameraAreaLoad());
    updateLatestFileIfDue(false);
}

const std::filesystem::path& getOutputPath() {
    ensureInitialized();
    return s_state.sessionDir;
}

}  // namespace dusk::diagnostics
