// copyright defined in LICENSE.txt

#include "streamer_plugin.hpp"
#include "streams/logger.hpp"
#include "streams/rabbitmq.hpp"
#include "streams/stream.hpp"

#include <abieos.hpp>
#include <eosio/abi.hpp>
#include <fc/exception/exception.hpp>
#include <memory>

namespace b1 {

using namespace appbase;
using namespace std::literals;

struct streamer_plugin_impl {
   std::vector<std::unique_ptr<stream_handler>> streams;
};

static abstract_plugin& _streamer_plugin = app().register_plugin<streamer_plugin>();

streamer_plugin::streamer_plugin() : my(std::make_shared<streamer_plugin_impl>()) {}

streamer_plugin::~streamer_plugin() {}

void streamer_plugin::set_program_options(options_description& cli, options_description& cfg) {
   auto op = cfg.add_options();
   op("stream-rabbits", bpo::value<std::vector<string>>()->composing(),
      "RabbitMQ Streams if any; Format: USER:PASSWORD@ADDRESS:PORT/QUEUE[/ROUTING_KEYS, ...]");
   op("stream-loggers", bpo::value<std::vector<string>>()->composing(),
      "Logger Streams if any; Format: [routing_keys, ...]");
}

void streamer_plugin::plugin_initialize(const variables_map& options) {
   try {
      if (options.count("stream-loggers")) {
         auto loggers = options.at("stream-loggers").as<std::vector<std::string>>();
         initialize_loggers(my->streams, loggers);
      }

      if (options.count("stream-rabbits")) {
         auto rabbits = options.at("stream-rabbits").as<std::vector<std::string>>();
         initialize_rabbits(my->streams, rabbits);
      }

      ilog("initialized streams: ${streams}", ("streams", my->streams.size()));
   }
   FC_LOG_AND_RETHROW()
}

void streamer_plugin::plugin_startup() {
   cloner_plugin* cloner = app().find_plugin<cloner_plugin>();
   if (cloner) {
      cloner->set_streamer([this](const char* data, uint64_t data_size) { stream_data(data, data_size); });
   }
}

void streamer_plugin::plugin_shutdown() {}

void streamer_plugin::stream_data(const char* data, uint64_t data_size) {
   eosio::input_stream bin(data, data_size);
   if (eosio::result<stream_wrapper> res = eosio::from_bin<stream_wrapper>(bin)) {
      auto& sw = std::get<stream_wrapper_v0>(res.value());
      publish_to_streams(sw);
   } else {
      eosio::check(res.error());
   }
}

void streamer_plugin::publish_to_streams(const stream_wrapper_v0& sw) {
   for (auto& stream : my->streams) {
      if (stream->check_route(sw.route)) {
         stream->publish(sw.data.data(), sw.data.size());
      }
   }
}

} // namespace b1
