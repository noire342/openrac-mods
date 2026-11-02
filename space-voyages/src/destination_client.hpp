// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "destination_gateway.hpp"
#include "garage_destination_adapter.hpp"

namespace spacevoyages {
// Compatibility facade for the current galaxy/story policy. Ticket and SDK
// ownership live in the generic gateway; the Garage adapter only maps the
// current body's alternate route. Future catalogs use RegisteredDestination.
class DestinationClient final {
public:
    DestinationClient(const openrac_api* api,openrac_mod* mod)noexcept:gateway_(api,mod){}
    DestinationClient(const DestinationClient&)=delete;
    DestinationClient& operator=(const DestinationClient&)=delete;
    bool supported()const noexcept{return gateway_.supported();}
    bool lookup(DestinationRoute route,openrac_destination& out)const noexcept{return gateway_.lookup(registered_route(route),out);}
    bool current(DestinationRoute& out,std::uint32_t* generation=nullptr)const noexcept {
        RegisteredDestination identity;out={};if(generation)*generation=0;
        std::uint32_t observed{};
        if(!gateway_.current(identity,&observed))return false;
        out=galaxy_route(identity);if(!out.valid())return false;
        if(generation)*generation=observed;return true;
    }
    bool begin_departure(DestinationRoute expected)noexcept{return gateway_.begin_departure(registered_route(expected));}
    bool prepare(DestinationRoute target)noexcept{return gateway_.prepare(registered_route(target));}
    bool discard_prepared()noexcept{return gateway_.discard_prepared();}
    bool commit()noexcept{return gateway_.commit();}
    bool cancel()noexcept{return gateway_.cancel();}
    DestinationRoute origin_route()const noexcept{return galaxy_route(gateway_.origin_route());}
    DestinationRoute prepared_route()const noexcept{return galaxy_route(gateway_.prepared_route());}
    const openrac_destination_context& context()const noexcept{return gateway_.context();}
    bool has_departure()const noexcept{return gateway_.has_departure();}
private:
    DestinationGateway gateway_;
};
}
