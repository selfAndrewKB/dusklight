#include "dusk/coop/line_render_diagnostics.h"

#include <cmath>

namespace dusk::coop::line_render_diagnostics {
namespace {

ExpansionRecord s_records[kMaxExpansionRecords];
int s_recordCount = 0;
uint32_t s_revision = 0;

bool isFinite(const cXyz& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

ExpansionRecord* findRecord(uintptr_t material, int lineIndex) {
    for (int i = 0; i < s_recordCount; i++) {
        if (s_records[i].material == material && s_records[i].lineIndex == lineIndex) {
            return &s_records[i];
        }
    }
    if (s_recordCount >= kMaxExpansionRecords) {
        return nullptr;
    }
    return &s_records[s_recordCount++];
}

}  // namespace

void recordExpansion(const void* material, int materialId, int lineKind, int lineIndex,
                     const cXyz* controlPoints, const cXyz* expandedPoints, int pointCount,
                     const cXyz& eye, bool presentationRefresh) {
    ExpansionRecord* record = findRecord(reinterpret_cast<uintptr_t>(material), lineIndex);
    if (record == nullptr) {
        return;
    }

    const uint32_t actorExpansionCount = record->actorExpansionCount;
    const uint32_t presentationRefreshRequestCount = record->presentationRefreshRequestCount;
    const uint32_t presentationExpansionCount = record->presentationExpansionCount;
    const cXyz lastPresentationEye = record->lastPresentationEye;
    *record = {};
    record->material = reinterpret_cast<uintptr_t>(material);
    record->materialId = materialId;
    record->lineKind = lineKind;
    record->lineIndex = lineIndex;
    record->pointCount = pointCount;
    record->presentationRefresh = presentationRefresh;
    record->eye = eye;
    record->lastPresentationEye = presentationRefresh ? eye : lastPresentationEye;
    record->actorExpansionCount = actorExpansionCount + (presentationRefresh ? 0 : 1);
    record->presentationRefreshRequestCount = presentationRefreshRequestCount;
    record->presentationExpansionCount =
        presentationExpansionCount + (presentationRefresh ? 1 : 0);

    for (int i = 0; controlPoints != nullptr && expandedPoints != nullptr && i < pointCount; i++) {
        const cXyz& control = controlPoints[i];
        const cXyz& left = expandedPoints[i * 2];
        const cXyz& right = expandedPoints[i * 2 + 1];
        if (!isFinite(control)) {
            record->controlNonFiniteCount++;
            continue;
        }
        if (i > 0 && isFinite(controlPoints[i - 1])) {
            const f32 segment = control.abs(controlPoints[i - 1]);
            if (segment > record->maxControlSegment) {
                record->maxControlSegment = segment;
            }
        }
        if (!isFinite(left) || !isFinite(right)) {
            record->expandedNonFiniteCount++;
            continue;
        }

        const f32 width = left.abs(right);
        if (width > record->maxExpandedWidth) {
            record->maxExpandedWidth = width;
        }
        const f32 leftDistance = left.abs(eye);
        const f32 rightDistance = right.abs(eye);
        if (leftDistance > record->maxExpandedDistanceFromEye) {
            record->maxExpandedDistanceFromEye = leftDistance;
        }
        if (rightDistance > record->maxExpandedDistanceFromEye) {
            record->maxExpandedDistanceFromEye = rightDistance;
        }
    }
    s_revision++;
}

void recordPresentationRefreshRequest(const void* material, int lineKind, const cXyz& eye) {
    ExpansionRecord* record = findRecord(reinterpret_cast<uintptr_t>(material), 0);
    if (record == nullptr) {
        return;
    }

    record->material = reinterpret_cast<uintptr_t>(material);
    record->lineKind = lineKind;
    record->lastPresentationEye = eye;
    record->presentationRefreshRequestCount++;
    s_revision++;
}

const ExpansionRecord* getRecords() {
    return s_records;
}

int getRecordCount() {
    return s_recordCount;
}

uint32_t getRevision() {
    return s_revision;
}

}  // namespace dusk::coop::line_render_diagnostics
