#pragma once

class daAlink_c;

namespace dusk::coop::alink_model_data_owner {

// Co-op: ALINK body model data is shared while its installed calculators are actor-local.
void restorePrimary();

class ScopedOwner {
public:
    explicit ScopedOwner(daAlink_c* player);
    ~ScopedOwner();

    ScopedOwner(const ScopedOwner&) = delete;
    ScopedOwner& operator=(const ScopedOwner&) = delete;

private:
    bool mRestorePrimary = false;
};

}  // namespace dusk::coop::alink_model_data_owner
