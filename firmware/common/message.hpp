/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <cstdint>
#include <cstddef>
#include <array>
#include <functional>

#include "utility.hpp"

#include "ch.h"

class Message {
public:
	static constexpr size_t MAX_SIZE = 276;
	
	enum class ID : uint16_t {
		/* Assign consecutive IDs. IDs are used to index array. */
		RSSIStatistics = 0,
		BasebandStatistics = 1,
		ChannelStatistics = 2,
		DisplayFrameSync = 3,
		ChannelSpectrum = 4,
		AudioStatistics = 5,
		BasebandConfiguration = 6,
		TPMSPacket = 7,
		Shutdown = 8,
		AISPacket = 9,
		TXDone = 10,
		SDCardStatus = 11,
		Retune = 12,
		ReadyForSwitch = 13,
		AFSKData = 14,
		ModuleID = 15,
		FIFOSignal = 16,
		FIFOData = 17,
		MAX
	};

	constexpr Message(
		ID id
	) : id { id }
	{
	}

	const ID id;
};

struct RSSIStatistics {
	uint32_t accumulator { 0 };
	uint32_t min { 0 };
	uint32_t max { 0 };
	uint32_t count { 0 };
};

class RSSIStatisticsMessage : public Message {
public:
	constexpr RSSIStatisticsMessage(
		const RSSIStatistics& statistics
	) : Message { ID::RSSIStatistics },
		statistics { statistics }
	{
	}

	RSSIStatistics statistics;
};

struct BasebandStatistics {
	uint32_t idle_ticks { 0 };
	uint32_t main_ticks { 0 };
	uint32_t rssi_ticks { 0 };
	uint32_t baseband_ticks { 0 };
	bool saturation { false };
};

class BasebandStatisticsMessage : public Message {
public:
	constexpr BasebandStatisticsMessage(
		const BasebandStatistics& statistics
	) : Message { ID::BasebandStatistics },
		statistics { statistics }
	{
	}

	BasebandStatistics statistics;
};

struct ChannelStatistics {
	int32_t max_db;
	size_t count;

	constexpr ChannelStatistics(
		int32_t max_db = -120,
		size_t count = 0
	) : max_db { max_db },
		count { count }
	{
	}
};

class ChannelStatisticsMessage : public Message {
public:
	constexpr ChannelStatisticsMessage(
	) : Message { ID::ChannelStatistics }
	{
	}

	ChannelStatistics statistics;
};

struct AudioStatistics {
	int32_t rms_db;
	int32_t max_db;
	size_t count;

	constexpr AudioStatistics(
	) : rms_db { -120 },
		max_db { -120 },
		count { 0 }
	{
	}

	constexpr AudioStatistics(
		int32_t rms_db,
		int32_t max_db,
		size_t count
	) : rms_db { rms_db },
		max_db { max_db },
		count { count }
	{
	}
};

class AudioStatisticsMessage : public Message {
public:
	constexpr AudioStatisticsMessage(
	) : Message { ID::AudioStatistics },
		statistics { }
	{
	}

	AudioStatistics statistics;
};

typedef enum {
	RX_NBAM_AUDIO = 0,
	RX_NBFM_AUDIO, 
	RX_WBFM_AUDIO,
	RX_AIS,
	RX_WBSPECTRUM,
	RX_TPMS,
	RX_AFSK,
	RX_SIGFOX,
	
	TX_RDS,
	TX_LCR,
	TX_TONE,
	TX_JAMMER,
	TX_XYLOS,
	
	PLAY_AUDIO,
	
	NONE,
	SWITCH = 0xFF
} mode_type;

struct BasebandConfiguration {
	mode_type mode;
	uint32_t sampling_rate;
	size_t decimation_factor;

	constexpr BasebandConfiguration(
		mode_type mode = NONE,
		uint32_t sampling_rate = 0,
		size_t decimation_factor = 1
	) : mode { mode },
		sampling_rate { sampling_rate },
		decimation_factor { decimation_factor }
	{
	}
};

class BasebandConfigurationMessage : public Message {
public:
	constexpr BasebandConfigurationMessage(
		BasebandConfiguration configuration
	) : Message { ID::BasebandConfiguration },
		configuration(configuration)
	{
	}

	BasebandConfiguration configuration;
};

struct ChannelSpectrum {
	std::array<uint8_t, 256> db { { 0 } };
	size_t db_count { 256 };
	uint32_t sampling_rate { 0 };
	uint32_t channel_filter_pass_frequency { 0 };
	uint32_t channel_filter_stop_frequency { 0 };
};

class ChannelSpectrumMessage : public Message {
public:
	constexpr ChannelSpectrumMessage(
	) : Message { ID::ChannelSpectrum }
	{
	}

	ChannelSpectrum spectrum;
};

#include <bitset>

struct AISPacket {
	std::bitset<1024> payload;
	size_t bits_received { 0 };
};

class AISPacketMessage : public Message {
public:
	constexpr AISPacketMessage(
	) : Message { ID::AISPacket }
	{
	}

	AISPacket packet;
};

struct TPMSPacket {
	std::bitset<1024> payload;
	size_t bits_received { 0 };
};

class TPMSPacketMessage : public Message {
public:
	constexpr TPMSPacketMessage(
	) : Message { ID::TPMSPacket }
	{
	}

	TPMSPacket packet;
};

class AFSKDataMessage : public Message {
public:
	constexpr AFSKDataMessage(
	) : Message { ID::AFSKData }
	{
	}

	int16_t data[128] = {0};
};

class ShutdownMessage : public Message {
public:
	constexpr ShutdownMessage(
	) : Message { ID::Shutdown }
	{
	}
};

class SDCardStatusMessage : public Message {
public:
	constexpr SDCardStatusMessage(
		bool is_mounted
	) : Message { ID::SDCardStatus },
		is_mounted { is_mounted }
	{
	}

	const bool is_mounted;
};

class TXDoneMessage : public Message {
public:
	TXDoneMessage(
	) : Message { ID::TXDone }
	{
	}
	
	int n = 0;
};

class ModuleIDMessage : public Message {
public:
	ModuleIDMessage(
	) : Message { ID::ModuleID }
	{
	}
	
	bool query;
	char md5_signature[16];
};

class ReadyForSwitchMessage : public Message {
public:
	ReadyForSwitchMessage(
	) : Message { ID::ReadyForSwitch }
	{
	}
};

class RetuneMessage : public Message {
public:
	RetuneMessage(
	) : Message { ID::Retune }
	{
	}
	
	int64_t freq = 0;
};

class DisplayFrameSyncMessage : public Message {
public:
	constexpr DisplayFrameSyncMessage(
	) : Message { ID::DisplayFrameSync }
	{
	}
};

class FIFOSignalMessage : public Message {
public:
	FIFOSignalMessage(
	) : Message { ID::FIFOSignal }
	{
	}

	char signaltype = 0;
};

class FIFODataMessage : public Message {
public:
	FIFODataMessage(
	) : Message { ID::FIFOData }
	{
	}

	int8_t * data;
};

class MessageHandlerMap {
public:
	using MessageHandler = std::function<void(Message* const p)>;

	void register_handler(const Message::ID id, MessageHandler&& handler) {
		if( map_[toUType(id)] != nullptr ) {
			chDbgPanic("MsgDblReg");
		}
		map_[toUType(id)] = std::move(handler);
	}

	void unregister_handler(const Message::ID id) {
		map_[toUType(id)] = nullptr;
	}

	void send(Message* const message) {
		if( message->id < Message::ID::MAX ) {
			auto& fn = map_[toUType(message->id)];
			if( fn ) {
				fn(message);
			}
		}
	}

private:
	using MapType = std::array<MessageHandler, toUType(Message::ID::MAX)>;
	MapType map_;
};

#endif/*__MESSAGE_H__*/
