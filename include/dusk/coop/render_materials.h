#pragma once

class J3DModel;
class J3DModelData;
class cXyz;
struct dKy_tevstr_c;

namespace dusk::coop::render_materials {

void registerKankyoTevstrContext(int tevstrType, const cXyz* pos, dKy_tevstr_c* tevstr);
void registerKankyoMaterial(J3DModelData* modelData, dKy_tevstr_c* tevstr);
void registerKankyoModel(J3DModel* model, dKy_tevstr_c* tevstr);
void refreshKankyoMaterialsForCurrentView();

}  // namespace dusk::coop::render_materials
