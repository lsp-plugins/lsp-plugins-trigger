/*
 * Copyright (C) 2026 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2026 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-trigger
 * Created on: 31 июл. 2021 г.
 *
 * lsp-plugins-trigger is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-trigger is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-trigger. If not, see <https://www.gnu.org/licenses/>.
 */

#include <private/plugins/trigger.h>
#include <private/ui/trigger.h>
#include <lsp-plug.in/plug-fw/ui.h>
#include <private/ui/trigger_midi.h>

namespace lsp
{
    namespace plugui
    {
        //---------------------------------------------------------------------
        // Plugin UI factory
        static const meta::plugin_t *plugin_uis[] =
        {
            &meta::trigger_mono,
            &meta::trigger_stereo,
            &meta::trigger_midi_mono,
            &meta::trigger_midi_stereo
        };

        static ui::Module *trigger_factory_func(const meta::plugin_t *meta)
        {
            return new trigger(meta);
        }

        static ui::Factory factory(trigger_factory_func, plugin_uis, 4);

        //---------------------------------------------------------------------
        // Trigger UI implementation
        trigger::trigger(const meta::plugin_t *meta): ui::Module(meta)
        {
            pCurrentSample          = NULL;
            pRevealSampleOnListen   = NULL;

            for (size_t i=0; i<meta::trigger_metadata::SAMPLE_FILES; ++i)
                wSampleListen[i]        = NULL;
        }

        trigger::~trigger()
        {
        }

        status_t trigger::init(ui::IWrapper *wrapper)
        {
            status_t res = ui::Module::init(wrapper);
            if (res != STATUS_OK)
                return res;

            // Seek for all velocity ports and create proxy ports
            for (size_t i=0, n=wrapper->ports(); i<n; ++i)
            {
                ui::IPort *port = wrapper->port(i);
                if (port == NULL)
                    continue;
                const meta::port_t *meta = port->metadata();
                if ((meta == NULL) || (meta->id == NULL))
                    continue;
                if (strstr(meta->id, "vl_") != meta->id)
                    continue;

                // Create proxy port
                trigger_midi::MidiVelocityPort *velocity = new trigger_midi::MidiVelocityPort();
                if (velocity == NULL)
                    return STATUS_NO_MEM;
                if ((res = velocity->init("midivel", port)) != STATUS_OK)
                {
                    delete velocity;
                    return res;
                }
                if ((res = pWrapper->bind_custom_port(velocity)) != STATUS_OK)
                {
                    delete velocity;
                    return res;
                }
            }

            return STATUS_OK;
        }

        status_t trigger::post_init()
        {
            ui::IWrapper *const w   = wrapper();

            pCurrentSample          = w->port("ssel");
            pRevealSampleOnListen   = w->port(UI_REVEAL_SAMPLE_ON_LISTEN_PORT);

            for (size_t i=0; i<meta::trigger_metadata::SAMPLE_FILES; ++i)
            {
                tk::Button * const listen   = w->get_widgetf<tk::Button>("trg_listen_sample_%d", int(i));
                if (listen != NULL)
                    listen->slots()->bind(tk::SLOT_CHANGE, slot_submit_listen_sample, this);

                wSampleListen[i]            = listen;
            }

            return STATUS_OK;
        }

        status_t trigger::slot_submit_listen_sample(tk::Widget *sender, void *ptr, void *data)
        {
            trigger * const self = static_cast<trigger *>(ptr);
            if (self == NULL)
                return STATUS_OK;
            if (self->pCurrentSample == NULL)
                return STATUS_OK;
            if ((self->pRevealSampleOnListen == NULL) || (self->pRevealSampleOnListen->value() < 0.5f))
                return STATUS_OK;

            // Ensure that button has been pushed down
            tk::Button * const btn = tk::widget_cast<tk::Button>(sender);
            if ((btn == NULL) || (!btn->down()->get()))
                return STATUS_OK;

            // Find the related sample and activate it
            for (size_t i=0; i<meta::trigger_metadata::SAMPLE_FILES; ++i)
            {
                if (sender == self->wSampleListen[i])
                {
                    self->pCurrentSample->begin_edit();
                    self->pCurrentSample->set_value(i);
                    self->pCurrentSample->notify_all(ui::PORT_USER_EDIT);
                    self->pCurrentSample->end_edit();
                    return STATUS_OK;
                }
            }

            return STATUS_OK;
        }

    } /* namespace plugui */
} /* namespace lsp */
