#include "dusk/coop/alink_model_data_owner.h"

#include "d/actor/d_a_alink.h"
#include "dusk/coop/alink_form_resources.h"
#include "dusk/coop/player_slots.h"

namespace dusk::coop::alink_model_data_owner {
namespace {

void installForPlayer(daAlink_c* player) {
    if (player == nullptr || !alink_form_resources::canInstallModelDataOwner(player)) {
        return;
    }

    if (player->checkWolf()) {
        player->changeModelDataDirectWolf(0);
    } else {
        player->changeModelDataDirect(0);
    }
}

}  // namespace

void restorePrimary() {
    installForPlayer(static_cast<daAlink_c*>(getPrimaryPlayer()));
}

ScopedOwner::ScopedOwner(daAlink_c* player) {
    if (!isAdditionalPlayer(player)) {
        return;
    }

    installForPlayer(player);
    mRestorePrimary = true;
}

ScopedOwner::~ScopedOwner() {
    if (mRestorePrimary) {
        restorePrimary();
    }
}

}  // namespace dusk::coop::alink_model_data_owner
