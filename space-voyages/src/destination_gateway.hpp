// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openrac/openrac_mod.h>
#include "registered_destination.hpp"
#include <cstdint>

namespace spacevoyages {
// Generic SDK boundary for any registered destination. No garage, story,
// celestial model, renderer, unlock flag or save-policy knowledge belongs here.
// The host remains authoritative for registration, ownership and handoff epochs.
class DestinationGateway final {
public:
    DestinationGateway(const openrac_api* api,openrac_mod* mod)noexcept:api_(api),mod_(mod){}
    ~DestinationGateway()noexcept;
    DestinationGateway(const DestinationGateway&)=delete;
    DestinationGateway& operator=(const DestinationGateway&)=delete;
    bool supported()const noexcept;
    bool lookup(RegisteredDestination,openrac_destination&)const noexcept;
    bool current(RegisteredDestination&,std::uint32_t* native_generation=nullptr)const noexcept;
    bool begin_departure(RegisteredDestination)noexcept;
    bool prepare(RegisteredDestination)noexcept;
    bool discard_prepared()noexcept;
    bool commit()noexcept;
    bool cancel()noexcept;
    RegisteredDestination origin_route()const noexcept{return origin_;}
    RegisteredDestination prepared_route()const noexcept{return token_?prepared_:RegisteredDestination{};}
    const openrac_destination_context& context()const noexcept{return context_;}
    bool has_departure()const noexcept{return ticket_!=0&&nonce_!=0;}
private:
    const openrac_api* api_{};openrac_mod* mod_{};
    openrac_destination_context context_{};
    RegisteredDestination origin_{},prepared_{};
    std::uint64_t ticket_{},token_{},nonce_{};
    bool read_context(openrac_destination_context&,RegisteredDestination&)const noexcept;
    bool same_departure(const openrac_destination_context&,RegisteredDestination)const noexcept;
    void clear()noexcept;
};
}
