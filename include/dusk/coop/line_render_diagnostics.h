#pragma once

#include "SSystem/SComponent/c_xyz.h"

#include <cstdint>

namespace dusk::coop::line_render_diagnostics {

constexpr int kMaxExpansionRecords = 128;

struct ExpansionRecord {
    uintptr_t material = 0;
    int materialId = -1;
    int lineKind = 0;
    int lineIndex = 0;
    int pointCount = 0;
    bool presentationRefresh = false;
    cXyz eye;
    cXyz lastPresentationEye;
    uint32_t actorExpansionCount = 0;
    uint32_t presentationRefreshRequestCount = 0;
    uint32_t presentationExpansionCount = 0;
    int controlNonFiniteCount = 0;
    int expandedNonFiniteCount = 0;
    f32 maxControlSegment = 0.0f;
    f32 maxExpandedWidth = 0.0f;
    f32 maxExpandedDistanceFromEye = 0.0f;
};

void recordExpansion(const void* material, int materialId, int lineKind, int lineIndex,
                     const cXyz* controlPoints, const cXyz* expandedPoints, int pointCount,
                     const cXyz& eye, bool presentationRefresh);
void recordPresentationRefreshRequest(const void* material, int lineKind, const cXyz& eye);
const ExpansionRecord* getRecords();
int getRecordCount();
uint32_t getRevision();

}  // namespace dusk::coop::line_render_diagnostics
