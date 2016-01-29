/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
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

//TODO: Morse coder
//TODO: Playdead amnesia and login
//TODO: Touch screen calibration
//TODO: Display module info (name, desc) somewhere
//TODO: Show MD5 mismatches for modules not found, etc...
//TODO: More gfx, cute icons :)
//TODO: Check jammer bandwidths
//TODO: GSM channel detector
//TODO: AFSK receiver
//TODO: SIGFOX RX/TX
//TODO: Reset baseband if module not found (instead of lockup in RAM loop)
//TODO: Module name/filename in modules.hpp to indicate requirement in case it's not found ui_loadmodule
//TODO: LCD backlight PWM
//TODO: BUG: Crash after TX stop (unregister message !)
//TODO: Check bw setting in LCR TX
//TODO: BUG: Crash after PSN entry in RDS TX
//TODO: Bodet :)
//TODO: Whistler
//TODO: Setup: Play dead by default ? Enable/disable ?
//TODO: Hide statusview when playing dead
//TODO: Persistent playdead !
//TODO: LCR EC=A,J,N
//TODO: LCR full message former (see norm)
//TODO: LCR address scan
//TODO: AFSK NRZI
//TODO: TX power

#include "ch.h"
#include "test.h"

#include "lpc43xx_cpp.hpp"
using namespace lpc43xx;

#include "portapack.hpp"
#include "portapack_io.hpp"
#include "portapack_shared_memory.hpp"
#include "portapack_persistent_memory.hpp"
using namespace portapack;

#include "cpld_update.hpp"

#include "message_queue.hpp"

#include "ui.hpp"
#include "ui_widget.hpp"
#include "ui_painter.hpp"
#include "ui_navigation.hpp"

#include "irq_ipc.hpp"
#include "irq_lcd_frame.hpp"
#include "irq_controls.hpp"
#include "irq_rtc.hpp"

#include "event.hpp"

#include "m4_startup.hpp"
#include "spi_image.hpp"

#include "debug.hpp"
#include "led.hpp"

#include "gcc.hpp"

#include <string.h>

#include "sd_card.hpp"

#include <string.h>

class EventDispatcher {
public:
	EventDispatcher(
		ui::Widget* const top_widget,
		ui::Painter& painter,
		ui::Context& context
	) : top_widget { top_widget },
		painter(painter),
		context(context)
	{
		// touch_manager.on_started = [this](const ui::TouchEvent event) {
		// 	this->context.focus_manager.update(this->top_widget, event);
		// };

		touch_manager.on_event = [this](const ui::TouchEvent event) {
			this->on_touch_event(event);
		};
	}

	void run() {
		while(is_running) {
			const auto events = wait();
			dispatch(events);
		}
	}

	void request_stop() {
		is_running = false;
	}

private:
	touch::Manager touch_manager;
	ui::Widget* const top_widget;
	ui::Painter& painter;
	ui::Context& context;
	uint32_t encoder_last = 0;
	bool is_running = true;
	bool sd_card_present = false;

	eventmask_t wait() {
		return chEvtWaitAny(ALL_EVENTS);
	}

	void dispatch(const eventmask_t events) {
		if( events & EVT_MASK_APPLICATION ) {
			handle_application_queue();
		}

		if( events & EVT_MASK_RTC_TICK ) {
			handle_rtc_tick();
		}

		if( events & EVT_MASK_LCD_FRAME_SYNC ) {
			handle_lcd_frame_sync();
		}

		if( events & EVT_MASK_SWITCHES ) {
			handle_switches();
		}

		if( events & EVT_MASK_ENCODER ) {
			handle_encoder();
		}

		if( events & EVT_MASK_TOUCH ) {
			handle_touch();
		}
	}

	void handle_application_queue() {
		std::array<uint8_t, Message::MAX_SIZE> message_buffer;
		while(Message* const message = shared_memory.application_queue.pop(message_buffer)) {
			context.message_map().send(message);
		}
	}

	void handle_rtc_tick() {
		uint16_t bloff_time;
		const auto sd_card_present_now = sdc_lld_is_card_inserted(&SDCD1);
		
		bloff_time = portapack::persistent_memory::ui_config_bloff();
		if (bloff_time) {
			if (portapack::bl_tick_counter >= bloff_time)
				io.lcd_backlight(0);
			else
				portapack::bl_tick_counter++;
		}
		
		if( sd_card_present_now != sd_card_present ) {
			sd_card_present = sd_card_present_now;

			if( sd_card_present ) {
				if( sdcConnect(&SDCD1) == CH_SUCCESS ) {
					if( sd_card::filesystem::mount() == FR_OK ) {
						SDCardStatusMessage message { true };
						context.message_map().send(&message);
					} else {
						// TODO: Error, modal warning?
					}
				} else {
					// TODO: Error, modal warning?
				}
			} else {
				sdcDisconnect(&SDCD1);

				SDCardStatusMessage message { false };
				context.message_map().send(&message);
			}
		}
	}

	static ui::Widget* touch_widget(ui::Widget* const w, ui::TouchEvent event) {
		if( !w->hidden() ) {
			// To achieve reverse depth ordering (last object drawn is
			// considered "top"), descend first.
			for(const auto child : w->children()) {
				const auto touched_widget = touch_widget(child, event);
				if( touched_widget ) {
					return touched_widget;
				}
			}

			const auto r = w->screen_rect();
			if( r.contains(event.point) ) {
				if( w->on_touch(event) ) {
					// This widget responded. Return it up the call stack.
					return w;
				}
			}
		}
		return nullptr;
	}

	ui::Widget* captured_widget { nullptr };

	void on_touch_event(ui::TouchEvent event) {
		/* TODO: Capture widget receiving the Start event, send Move and
		 * End events to the same widget.
		 */
		/* Capture Start widget.
		 * If touch is over Start widget at Move event, then the widget
		 * should be highlighted. If the touch is not over the Start
		 * widget at Move event, widget should un-highlight.
		 * If touch is over Start widget at End event, then the widget
		 * action should occur.
		 */
		if( event.type == ui::TouchEvent::Type::Start ) {
			captured_widget = touch_widget(this->top_widget, event);
		}

		if( captured_widget ) {
			captured_widget->on_touch(event);
		}
	}

	void handle_lcd_frame_sync() {
		DisplayFrameSyncMessage message;
		context.message_map().send(&message);
		painter.paint_widget_tree(top_widget);
	}

	void handle_switches() {
		const auto switches_state = get_switches_state();
		
		io.lcd_backlight(1);
		portapack::bl_tick_counter = 0;
		
		for(size_t i=0; i<switches_state.size(); i++) {
			// TODO: Ignore multiple keys at the same time?
			if( switches_state[i] ) {
				const auto event = static_cast<ui::KeyEvent>(i);
				if( !event_bubble_key(event) ) {
					context.focus_manager().update(top_widget, event);
				}
			}
		}
	}

	void handle_encoder() {
		const uint32_t encoder_now = get_encoder_position();
		const int32_t delta = static_cast<int32_t>(encoder_now - encoder_last);
		
		io.lcd_backlight(1);
		portapack::bl_tick_counter = 0;
		
		encoder_last = encoder_now;
		const auto event = static_cast<ui::EncoderEvent>(delta);
		event_bubble_encoder(event);
	}

	void handle_touch() {
		touch_manager.feed(get_touch_frame());
	}

	bool event_bubble_key(const ui::KeyEvent event) {
		auto target = context.focus_manager().focus_widget();
		while( (target != nullptr) && !target->on_key(event) ) {
			target = target->parent();
		}

		/* Return true if event was consumed. */
		return (target != nullptr);
	}

	void event_bubble_encoder(const ui::EncoderEvent event) {
		auto target = context.focus_manager().focus_widget();
		while( (target != nullptr) && !target->on_encoder(event) ) {
			target = target->parent();
		}
	}
};

///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

/* Thinking things through a bit:

	main() produces UI events.
		Touch events:
			Hit test entire screen hierarchy and send to hit widget.
			If modal view shown, block UI events destined outside.
		Navigation events:
			Move from current focus widget to "nearest" focusable widget.
			If current view is modal, don't allow events to bubble outside
			of modal view.
		System events:
			Power off from WWDT provides enough time to flush changes to
				VBAT RAM?
			SD card events? Insert/eject/error.


	View stack:
		Views that are hidden should deconstruct their widgets?
		Views that are shown after being hidden will reconstruct their
			widgets from data in their model?
		Hence, hidden views will not eat up memory beyond their model?
		Beware loops where the stack can get wildly deep?
		Breaking out data models from views should allow some amount of
			power-off persistence in the VBAT RAM area. In fact, the data
			models could be instantiated there? But then, how to protect
			from corruption if power is pulled? Does WWDT provide enough
			warning to flush changes?

	Navigation...
		If you move off the left side of the screen, move to breadcrumb
			"back" item, no matter where you're coming from?
*/

/*
message_handlers[Message::ID::FSKPacket] = [](const Message* const p) {
	const auto message = static_cast<const FSKPacketMessage*>(p);
	fsk_packet(message);
};

message_handlers[Message::ID::TestResults] = [&system_view](const Message* const p) {
	const auto message = static_cast<const TestResultsMessage*>(p);
	char c[10];
	c[0] = message->results.translate_by_fs_over_4_and_decimate_by_2_cic3 ? '+' : '-';
	c[1] = message->results.fir_cic3_decim_2_s16_s16 ? '+' : '-';
	c[2] = message->results.fir_64_and_decimate_by_2_complex ? '+' : '-';
	c[3] = message->results.fxpt_atan2 ? '+' : '-';
	c[4] = message->results.multiply_conjugate_s16_s32 ? '+' : '-';
	c[5] = 0;
	system_view.status_view.portapack.set(c);
};
*/

int main(void) {
	portapack::init();

	if( !cpld_update_if_necessary() ) {
		chSysHalt();
	}

	init_message_queues();

	portapack::io.init();
	portapack::display.init();

	sdcStart(&SDCD1, nullptr);

	events_initialize(chThdSelf());
	init_message_queues();

	ui::Context context;
	ui::SystemView system_view {
		context,
		portapack::display.screen_rect()
	};
	ui::Painter painter;
	EventDispatcher event_dispatcher { &system_view, painter, context };

	auto& message_handlers = context.message_map();
	message_handlers.register_handler(Message::ID::Shutdown,
		[&event_dispatcher](const Message* const) {
			event_dispatcher.request_stop();
		}
	);

	m4_init(portapack::spi_flash::baseband, portapack::memory::map::m4_code);

	controls_init();
	lcd_frame_sync_configure();
	rtc_interrupt_enable();
	m4txevent_interrupt_enable();

	event_dispatcher.run();

	sdcDisconnect(&SDCD1);
	sdcStop(&SDCD1);

	portapack::shutdown();
	m4_init(portapack::spi_flash::hackrf, portapack::memory::map::m4_code_hackrf);

	rgu::reset(rgu::Reset::M0APP);

	return 0;
}
