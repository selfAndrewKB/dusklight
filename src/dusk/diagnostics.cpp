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
    std::unordered_map<std::string, ProviderStats> providerStats;
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

bool shouldEmitProviderEvent(const char* provider, const json& data) {
    const auto result = s_state.lastEmittedData.find(provider);
    if (result != s_state.lastEmittedData.end() && result->second == data) {
        return false;
    }

    s_state.lastEmittedData[provider] = data;
    return true;
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

json alinkSecondaryEventKey(const json& data) {
    return {
        {"schema_version", data.value("schema_version", 1)},
        {"available", data.value("available", false)},
        {"actor", data.value("actor", "0x0")},
        {"proc", data.value("proc", 0)},
        {"equip_item", data.value("equip_item", 0)},
        {"select_item_id", data.value("select_item_id", 0)},
        {"item_actor", data.value("item_actor", "0x0")},
        {"item_actor_id", data.value("item_actor_id", 0)},
        {"item_actor_name", data.value("item_actor_name", 0)},
        {"throw_boomerang_actor", data.value("throw_boomerang_actor", "0x0")},
        {"copy_rod_actor", data.value("copy_rod_actor", "0x0")},
        {"copy_rod_control_actor", data.value("copy_rod_control_actor", "0x0")},
        {"copy_rod_camera_actor", data.value("copy_rod_camera_actor", "0x0")},
        {"copy_rod_top_use", data.value("copy_rod_top_use", false)},
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
}

const char* alinkProcName(u16 proc) {
    switch (proc) {
    case daAlink_c::PROC_WAIT:
        return "PROC_WAIT";
    case daAlink_c::PROC_MOVE:
        return "PROC_MOVE";
    case daAlink_c::PROC_ATN_MOVE:
        return "PROC_ATN_MOVE";
    case daAlink_c::PROC_ATN_ACTOR_WAIT:
        return "PROC_ATN_ACTOR_WAIT";
    case daAlink_c::PROC_FRONT_ROLL:
        return "PROC_FRONT_ROLL";
    case daAlink_c::PROC_CUT_NORMAL:
        return "PROC_CUT_NORMAL";
    case daAlink_c::PROC_BOOMERANG_SUBJECT:
        return "PROC_BOOMERANG_SUBJECT";
    case daAlink_c::PROC_BOOMERANG_MOVE:
        return "PROC_BOOMERANG_MOVE";
    case daAlink_c::PROC_BOOMERANG_CATCH:
        return "PROC_BOOMERANG_CATCH";
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
    if (name == "input.pad") {
        return inputPadEventKey(data);
    }
    if (name == "render.stats") {
        return renderStatsEventKey(data);
    }
    if (name == "attention.state") {
        return attentionStateEventKey(data);
    }
    if (name == "player.status") {
        return playerStatusEventKey(data);
    }
    if (name == "alink.secondary") {
        return alinkSecondaryEventKey(data);
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

json collectAttentionState() {
    dAttention_c* attention = dComIfGp_getAttention();
    json data = {
        {"schema_version", 1},
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
        {"ignore_shared_attention_lock", coop::hasSecondaryAlinkProbeFlag(coop::SecondaryAlinkProbe_IgnoreSharedAttentionLock)},
    };
}

json collectAlinkSecondary() {
    const SecondaryAlinkState& state = s_state.secondaryAlinkState;
    json data = {
        {"schema_version", 2},
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
    data["throw_boomerang_actor"] = ptrString(state.throwBoomerangActor);
    data["copy_rod_actor"] = ptrString(state.copyRodActor);
    data["copy_rod_control_actor"] = ptrString(state.copyRodControlActor);
    data["copy_rod_camera_actor"] = ptrString(state.copyRodCameraActor);
    data["copy_rod_top_use"] = state.copyRodTopUse;
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

Provider s_providers[] = {
    {"scene.current", 1, "cheap", 30, true, 20, 4096, collectSceneCurrent},
    {"render.stats", 1, "cheap", 30, true, 20, 4096, collectRenderStats},
    {"player.slots", 1, "cheap", 1, true, 120, 8192, collectPlayerSlots},
    {"input.pad", 1, "cheap", 1, true, 120, 4096, collectInputPad},
    {"attention.state", 1, "medium", 5, true, 60, 12288, collectAttentionState},
    {"player.status", 1, "cheap", 1, true, 120, 8192, collectPlayerStatus},
    {"coop.probes", 1, "cheap", 30, true, 20, 4096, collectCoopProbes},
    {"alink.secondary", 2, "cheap", 1, true, 120, 8192, collectAlinkSecondary},
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
        json eventKey = eventKeyForProvider(provider.name, data);
        if (provider.emitOnChange && !shouldEmitProviderEvent(provider.name, eventKey)) {
            continue;
        }

        emitProviderEvent(provider, "snapshot", data);
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

const std::filesystem::path& getOutputPath() {
    ensureInitialized();
    return s_state.sessionDir;
}

}  // namespace dusk::diagnostics
