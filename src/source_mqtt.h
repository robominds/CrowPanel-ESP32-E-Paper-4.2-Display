// The indoor temperature, to or from the MQTT broker.
//
// secrets.ini names two topics, temperature in Celsius and relative humidity,
// carried as bare decimal ASCII, not JSON, about every ten seconds. The broker
// accepts anonymous connections. Which way the readings flow depends on the
// board: one with an AHT10 on its header publishes its own readings to those
// topics (JennOffice/AHT10/...), one without subscribes to another device's
// (MarkOffice/DHT/...).
//
// Temperature and humidity arrive as two separate messages. A history sample is
// appended when a temperature lands, carrying the most recent humidity, so the
// two topics do not need to be synchronised. No sample is appended until both
// have been seen at least once - a chart entry showing a real temperature
// beside a humidity of zero would be worse than a slightly later first point.
//
// Author: Mark Castelluccio <markacastelluccio@gmail.com>
// Written with assistance from Claude Code (Anthropic).

#pragma once

#include <cstdint>

#include "Channel.h"

namespace source_mqtt {

// Subscribe: the broker owns the indoor reading.
// PublishOnly: this panel owns it, reads it locally and writes it to the same
// two topics. A publishing panel never subscribes, so it cannot feed itself.
enum class Mode { Subscribe, PublishOnly };

// Binds the channel that incoming readings are written into, and the role this
// panel takes. The channel must outlive this module.
void begin(channel::Channel& indoor, Mode mode);

// PublishOnly only: writes one reading to the two configured topics, as bare
// decimals with one decimal place, QoS 0 and not retained. A no-op while
// disconnected - nothing is queued, the next sample is ten seconds away.
void publish(float temperature_c, float humidity_pct);

// Call every loop. Handles connection, reconnection with backoff, and message
// dispatch. Cheap when connected and idle.
void poll();

bool connected();

// Payloads rejected as malformed since boot. Surfaced so a broker that starts
// publishing something unexpected is visible rather than silently ignored.
uint32_t rejectedPayloads();

}  // namespace source_mqtt
